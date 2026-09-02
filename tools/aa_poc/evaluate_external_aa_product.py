#!/usr/bin/env python3
"""Evaluate an external renderer against the product AA SSAA8 oracle.

This path deliberately keeps external renderer manifests separate from the
ThorVG GL manifest schema.  The external manifest proves matrix completeness
and names candidate images; the trusted SSAA8 paths and their GL route proof
come from an ordinary ``aa_product_poc`` noaa manifest.
"""

from __future__ import annotations

import argparse
import csv
import dataclasses
import json
import math
import pathlib
import sys
from collections import OrderedDict
from typing import Optional, Sequence

import evaluate_aa_product as product
import generate_aa_report as report


EXTERNAL_COLUMNS = (
    "mode",
    "scene",
    "scale",
    "offset_x",
    "offset_y",
    "candidate_png",
)
FORBIDDEN_EXTERNAL_COLUMNS = frozenset(
    (
        "reference_png",
        "selected_mode",
        "root_samples",
        "route_valid",
        "expected_route_count",
        "actual_route_count",
        "total_route_count",
        "fallback_count",
    )
)


@dataclasses.dataclass(frozen=True)
class ExternalCase:
    row_number: int
    mode: str
    scene: str
    scale: str
    offset_x: float
    offset_y: float
    candidate_png: str
    reference_png: str
    candidate_path: pathlib.Path
    reference_path: pathlib.Path


@dataclasses.dataclass(frozen=True)
class RowResult:
    case: ExternalCase
    metrics: report.Metrics
    visual_failure: str

    @property
    def visual_status(self) -> str:
        return "fail" if self.visual_failure else "pass"


@dataclasses.dataclass(frozen=True)
class Aggregate:
    mode: str
    scene: Optional[str]
    scale: Optional[str]
    row_count: int
    metrics: report.Metrics
    gate_status: str
    failure: str = ""


class ExternalQualityError(RuntimeError):
    """A user-facing external manifest or evaluation error."""


def _planned_keys() -> set[tuple[str, str, float, float]]:
    return {
        (scene, scale, offset_x, offset_y)
        for scene in product.FULL_SCENES
        for scale in product.FULL_SCALES
        for offset_x, offset_y in product.FULL_OFFSETS
    }


def _required_text(raw: dict, field: str, row_number: int) -> str:
    value = raw.get(field)
    if value is None or not value.strip():
        raise ExternalQualityError(
            f"external manifest row {row_number}: {field} cannot be empty"
        )
    return value.strip()


def _finite_float(raw: dict, field: str, row_number: int) -> float:
    text = _required_text(raw, field, row_number)
    try:
        value = float(text)
    except ValueError as error:
        raise ExternalQualityError(
            f"external manifest row {row_number}: {field} must be a number, got {text!r}"
        ) from error
    if not math.isfinite(value):
        raise ExternalQualityError(
            f"external manifest row {row_number}: {field} must be finite"
        )
    return value


def read_external_manifest(
    path: pathlib.Path, external_mode: str
) -> tuple[ExternalCase, ...]:
    path = pathlib.Path(path).resolve()
    try:
        source = path.open("r", encoding="utf-8-sig", newline="")
    except OSError as error:
        raise ExternalQualityError(f"cannot read external manifest {path}: {error}") from error

    with source:
        reader = csv.DictReader(source, delimiter="\t")
        if reader.fieldnames is None:
            raise ExternalQualityError(f"external manifest {path} is empty")
        missing = [field for field in EXTERNAL_COLUMNS if field not in reader.fieldnames]
        if missing:
            raise ExternalQualityError(
                f"external manifest {path} is missing required columns: {', '.join(missing)}"
            )
        forbidden = sorted(FORBIDDEN_EXTERNAL_COLUMNS.intersection(reader.fieldnames))
        if forbidden:
            raise ExternalQualityError(
                "external manifest must not contain ThorVG GL proof/reference columns: "
                + ", ".join(forbidden)
            )

        cases = []
        seen = {}
        for row_number, raw in enumerate(reader, start=2):
            if None in raw:
                raise ExternalQualityError(
                    f"external manifest row {row_number} has more values than its header"
                )
            if all(value is None or not value.strip() for value in raw.values()):
                continue
            mode = _required_text(raw, "mode", row_number).lower()
            if mode != external_mode:
                raise ExternalQualityError(
                    f"external manifest row {row_number}: mode is {mode!r}; expected {external_mode!r}"
                )
            scene = _required_text(raw, "scene", row_number)
            scale = _required_text(raw, "scale", row_number)
            offset_x = _finite_float(raw, "offset_x", row_number)
            offset_y = _finite_float(raw, "offset_y", row_number)
            candidate_png = _required_text(raw, "candidate_png", row_number)
            key = (scene, scale, offset_x, offset_y)
            if key in seen:
                raise ExternalQualityError(
                    f"external manifest rows {seen[key]} and {row_number} duplicate case {key}"
                )
            seen[key] = row_number
            candidate_path = pathlib.Path(candidate_png)
            if not candidate_path.is_absolute():
                candidate_path = path.parent / candidate_path
            cases.append(
                ExternalCase(
                    row_number=row_number,
                    mode=mode,
                    scene=scene,
                    scale=scale,
                    offset_x=offset_x,
                    offset_y=offset_y,
                    candidate_png=candidate_png,
                    reference_png="",
                    candidate_path=candidate_path.resolve(),
                    reference_path=pathlib.Path(),
                )
            )

    if not cases:
        raise ExternalQualityError(f"external manifest {path} contains no data rows")
    expected = _planned_keys()
    actual = set(seen)
    missing = sorted(expected - actual)
    unexpected = sorted(actual - expected)
    if missing or unexpected:
        details = []
        if missing:
            details.append(f"missing {len(missing)} planned case(s), first={missing[0]}")
        if unexpected:
            details.append(
                f"contains {len(unexpected)} unexpected case(s), first={unexpected[0]}"
            )
        raise ExternalQualityError("external full-suite matrix mismatch: " + "; ".join(details))
    return tuple(cases)


def read_reference_map(
    path: pathlib.Path,
) -> dict[tuple[str, str, float, float], tuple[str, pathlib.Path]]:
    path = pathlib.Path(path).resolve()
    try:
        manifest_cases = product.read_manifest(path)
    except product.QualityGateError as error:
        raise ExternalQualityError(str(error)) from error

    references = {}
    for case in manifest_cases:
        if case.mode != product.DIAGNOSTIC_MODE:
            continue
        invariant_failures = product._case_invariant_failures(case)
        if invariant_failures:
            raise ExternalQualityError(
                f"reference manifest row {case.row_number} failed its noaa route proof: "
                + "; ".join(invariant_failures)
            )
        if pathlib.Path(case.reference_png).name != "ssaa8.png":
            raise ExternalQualityError(
                f"reference manifest row {case.row_number}: reference_png must name ssaa8.png"
            )
        key = (case.scene, case.scale, case.offset_x, case.offset_y)
        value = (case.reference_png, case.reference_path)
        previous = references.setdefault(key, value)
        if previous[1] != value[1]:
            raise ExternalQualityError(
                f"reference manifest has inconsistent SSAA8 paths for case {key}"
            )

    expected = _planned_keys()
    actual = set(references)
    missing = sorted(expected - actual)
    unexpected = sorted(actual - expected)
    if missing or unexpected:
        details = []
        if missing:
            details.append(f"missing {len(missing)} SSAA8 reference(s), first={missing[0]}")
        if unexpected:
            details.append(
                f"contains {len(unexpected)} unexpected SSAA8 reference(s), first={unexpected[0]}"
            )
        raise ExternalQualityError("reference matrix mismatch: " + "; ".join(details))
    return references


def join_references(
    cases: Sequence[ExternalCase],
    references: dict[tuple[str, str, float, float], tuple[str, pathlib.Path]],
) -> tuple[ExternalCase, ...]:
    joined = []
    for case in cases:
        key = (case.scene, case.scale, case.offset_x, case.offset_y)
        reference_png, reference_path = references[key]
        joined.append(
            dataclasses.replace(
                case,
                reference_png=reference_png,
                reference_path=reference_path,
            )
        )
    return tuple(joined)


def _metric_failure(
    metrics: report.Metrics, min_ssim: float, min_psnr_db: float
) -> str:
    failures = []
    if metrics.ssim < min_ssim:
        failures.append(f"SSIM {metrics.ssim:.6f} < {min_ssim:.6f}")
    if metrics.psnr_db < min_psnr_db:
        failures.append(
            f"PSNR {product._format_psnr(metrics.psnr_db)} < {min_psnr_db:.3f} dB"
        )
    return "; ".join(failures)


def evaluate_rows(
    cases: Sequence[ExternalCase], thresholds: product.Thresholds
) -> tuple[RowResult, ...]:
    decoded = {}

    def image(path: pathlib.Path) -> report.PngImage:
        if path not in decoded:
            decoded[path] = report.decode_png(path)
        return decoded[path]

    rows = []
    for case in cases:
        try:
            metrics = report.compute_metrics(
                image(case.candidate_path), image(case.reference_path)
            )
        except (report.ReportError, OSError, ValueError) as error:
            raise ExternalQualityError(
                f"external manifest row {case.row_number} ({case.mode}, {case.scene}, "
                f"scale={case.scale}, offset={product._format_offset(case.offset_x, case.offset_y)}): "
                f"{error}"
            ) from error
        rows.append(
            RowResult(
                case=case,
                metrics=metrics,
                visual_failure=_metric_failure(
                    metrics, thresholds.row_min_ssim, thresholds.row_min_psnr_db
                ),
            )
        )
    return tuple(rows)


def group_aggregates(
    rows: Sequence[RowResult], key, thresholds: product.Thresholds, gate_mode: bool
) -> tuple[Aggregate, ...]:
    groups = OrderedDict()
    for row in rows:
        groups.setdefault(key(row), []).append(row)
    aggregates = []
    for group_key, members in groups.items():
        combined = report.combine_metrics([member.metrics for member in members])
        scene = group_key[1] if len(group_key) > 1 else None
        scale = group_key[2] if len(group_key) > 2 else None
        failures = []
        if gate_mode:
            failed_rows = sum(bool(member.visual_failure) for member in members)
            if failed_rows:
                failures.append(f"{failed_rows} required row(s) failed the row visual gate")
            aggregate_failure = _metric_failure(
                combined, thresholds.mode_min_ssim, thresholds.mode_min_psnr_db
            )
            if aggregate_failure:
                failures.append(aggregate_failure)
        failure = "; ".join(failures)
        aggregates.append(
            Aggregate(
                mode=group_key[0],
                scene=scene,
                scale=scale,
                row_count=len(members),
                metrics=combined,
                gate_status=("fail" if failure else "pass") if gate_mode else "reported",
                failure=failure,
            )
        )
    return tuple(aggregates)


def _metric_dict(metrics: report.Metrics) -> dict:
    return {
        "ssim": metrics.ssim,
        "psnr_db": "infinity" if math.isinf(metrics.psnr_db) else metrics.psnr_db,
        "mean_absolute_error_8bit": metrics.mean_absolute_error,
        "max_absolute_error_8bit": metrics.max_absolute_error,
    }


def _aggregate_dict(aggregate: Aggregate) -> dict:
    value = {
        "mode": aggregate.mode,
        "row_count": aggregate.row_count,
        "manifest_row_count": aggregate.row_count,
        "invariant_invalid_row_count": 0,
        "metrics": _metric_dict(aggregate.metrics),
        "gate_status": aggregate.gate_status,
    }
    if aggregate.scene is not None:
        value["scene"] = aggregate.scene
    if aggregate.scale is not None:
        value["scale"] = aggregate.scale
    if aggregate.failure:
        value["failure"] = aggregate.failure
    return value


def write_results_tsv(path: pathlib.Path, rows: Sequence[RowResult]) -> None:
    fields = list(product.MANIFEST_COLUMNS) + [
        "evaluation_stage",
        "included_in_visual_aggregates",
        "ssim",
        "psnr_db",
        "mean_absolute_error_8bit",
        "max_absolute_error_8bit",
        "invariant_status",
        "visual_status",
        "gate_status",
        "failures",
    ]
    try:
        with path.open("w", encoding="utf-8", newline="") as output:
            writer = csv.DictWriter(output, fieldnames=fields, delimiter="\t")
            writer.writeheader()
            for row in rows:
                case = row.case
                writer.writerow(
                    {
                        "mode": case.mode,
                        "scene": case.scene,
                        "scale": case.scale,
                        "offset_x": product._format_number(case.offset_x),
                        "offset_y": product._format_number(case.offset_y),
                        "candidate_png": case.candidate_png,
                        "reference_png": case.reference_png,
                        "selected_mode": "",
                        "root_samples": "",
                        "route_valid": "",
                        "expected_route_count": "",
                        "actual_route_count": "",
                        "total_route_count": "",
                        "fallback_count": "",
                        "evaluation_stage": "complete",
                        "included_in_visual_aggregates": "yes",
                        "ssim": product._tsv_metric(row.metrics.ssim),
                        "psnr_db": product._tsv_metric(row.metrics.psnr_db),
                        "mean_absolute_error_8bit": product._tsv_metric(
                            row.metrics.mean_absolute_error
                        ),
                        "max_absolute_error_8bit": product._tsv_metric(
                            row.metrics.max_absolute_error
                        ),
                        "invariant_status": "not-applicable",
                        "visual_status": row.visual_status,
                        "gate_status": row.visual_status,
                        "failures": row.visual_failure,
                    }
                )
    except OSError as error:
        raise ExternalQualityError(f"cannot write results TSV {path}: {error}") from error


def write_summary_json(
    path: pathlib.Path,
    manifest_path: pathlib.Path,
    reference_manifest: pathlib.Path,
    external_mode: str,
    thresholds: product.Thresholds,
    rows: Sequence[RowResult],
    by_mode: Sequence[Aggregate],
    by_scene: Sequence[Aggregate],
    by_scene_scale: Sequence[Aggregate],
) -> dict:
    row_failures = [row for row in rows if row.visual_failure]
    mode_failures = [aggregate for aggregate in by_mode if aggregate.failure]
    failures = [
        {
            "gate": "row-visual",
            "scope": "row",
            "row": row.case.row_number,
            "mode": row.case.mode,
            "scene": row.case.scene,
            "scale": row.case.scale,
            "offset": {"x": row.case.offset_x, "y": row.case.offset_y},
            "message": row.visual_failure,
        }
        for row in row_failures
    ] + [
        {
            "gate": "mode-visual",
            "scope": "mode",
            "row": None,
            "mode": aggregate.mode,
            "scene": None,
            "scale": None,
            "offset": None,
            "message": aggregate.failure,
        }
        for aggregate in mode_failures
    ]
    summary = {
        "schema_version": 1,
        "evaluation_profile": "external-renderer-product-aa",
        "manifest": str(manifest_path.resolve()),
        "reference_manifest": str(reference_manifest.resolve()),
        "external_mode": external_mode,
        "status": "fail" if failures else "pass",
        "evaluation_stage": "complete",
        "full_suite_required": True,
        "gate_order": [
            "external-matrix-and-reference",
            "row-visual",
            "mode-visual",
        ],
        "route_and_sample_invariants": "not-applicable to external renderer",
        "visual_evaluation": {
            "status": "complete",
            "evaluated_rows": len(rows),
            "not_run_rows": 0,
        },
        "diagnostic_mode": None,
        "gated_modes": [external_mode],
        "metric_conventions": {
            "alpha": "straight-alpha PNG composited over white",
            "psnr": "RGB, peak=255",
            "ssim": (
                "Rec.709 luminance, valid uniform windows up to 11x11, "
                "population moments, K1=0.01, K2=0.03, L=255"
            ),
            "aggregation": (
                "arithmetic mean of row SSIM and MAE; PSNR reconstructed from "
                "the arithmetic mean of row MSE; maximum of row max error"
            ),
        },
        "thresholds": {
            "defaults": product._threshold_dict(product.Thresholds()),
            "active": product._threshold_dict(thresholds),
            "rationale": product.THRESHOLD_RATIONALE,
        },
        "counts": {
            "rows": len(rows),
            "gated_rows": len(rows),
            "diagnostic_rows": 0,
            "failures": len(failures),
            "invariant_failures": 0,
            "row_visual_failures": len(row_failures),
            "mode_visual_failures": len(mode_failures),
            "visually_evaluated_rows": len(rows),
            "visual_not_run_rows": 0,
        },
        "rows": [
            {
                "manifest_row": row.case.row_number,
                "mode": row.case.mode,
                "scene": row.case.scene,
                "scale": row.case.scale,
                "offset": {"x": row.case.offset_x, "y": row.case.offset_y},
                "candidate_png": row.case.candidate_png,
                "reference_png": row.case.reference_png,
                "selected_mode": None,
                "root_samples": None,
                "route_valid": None,
                "expected_route_count": None,
                "actual_route_count": None,
                "total_route_count": None,
                "fallback_count": None,
                "metrics": _metric_dict(row.metrics),
                "invariant_status": "not-applicable",
                "visual_status": row.visual_status,
                "included_in_visual_aggregates": True,
                "gate_status": row.visual_status,
                "failures": [row.visual_failure] if row.visual_failure else [],
            }
            for row in rows
        ],
        "aggregates": {
            "by_mode": [_aggregate_dict(item) for item in by_mode],
            "by_mode_scene": [_aggregate_dict(item) for item in by_scene],
            "by_mode_scene_scale": [
                _aggregate_dict(item) for item in by_scene_scale
            ],
        },
        "failures": failures,
    }
    try:
        path.write_text(
            json.dumps(summary, indent=2, allow_nan=False) + "\n", encoding="utf-8"
        )
    except OSError as error:
        raise ExternalQualityError(f"cannot write summary JSON {path}: {error}") from error
    return summary


def build_argument_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "Evaluate one external renderer's 48 product-AA images against the "
            "SSAA8 references proven by an aa_product_poc manifest."
        ),
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument("manifest", help="external six-column candidate manifest")
    parser.add_argument("--external-mode", required=True)
    parser.add_argument("--reference-manifest", required=True)
    parser.add_argument("--output-dir")
    parser.add_argument(
        "--row-min-ssim",
        type=product._ssim_threshold,
        default=product.DEFAULT_ROW_MIN_SSIM,
    )
    parser.add_argument(
        "--row-min-psnr",
        dest="row_min_psnr_db",
        type=product._psnr_threshold,
        default=product.DEFAULT_ROW_MIN_PSNR_DB,
        metavar="DB",
    )
    parser.add_argument(
        "--mode-min-ssim",
        type=product._ssim_threshold,
        default=product.DEFAULT_MODE_MIN_SSIM,
    )
    parser.add_argument(
        "--mode-min-psnr",
        dest="mode_min_psnr_db",
        type=product._psnr_threshold,
        default=product.DEFAULT_MODE_MIN_PSNR_DB,
        metavar="DB",
    )
    return parser


def main(argv: Optional[Sequence[str]] = None) -> int:
    args = build_argument_parser().parse_args(argv)
    external_mode = args.external_mode.strip().lower()
    if not external_mode:
        print("evaluate_external_aa_product.py: error: external mode cannot be empty", file=sys.stderr)
        return 1
    if external_mode in product.KNOWN_MODES:
        print(
            "evaluate_external_aa_product.py: error: external mode must not name a ThorVG GL mode",
            file=sys.stderr,
        )
        return 1

    manifest_path = pathlib.Path(args.manifest).resolve()
    reference_manifest = pathlib.Path(args.reference_manifest).resolve()
    output_dir = (
        pathlib.Path(args.output_dir).resolve()
        if args.output_dir
        else manifest_path.parent / "external-evaluation"
    )
    thresholds = product.Thresholds(
        row_min_ssim=args.row_min_ssim,
        row_min_psnr_db=args.row_min_psnr_db,
        mode_min_ssim=args.mode_min_ssim,
        mode_min_psnr_db=args.mode_min_psnr_db,
    )

    try:
        cases = read_external_manifest(manifest_path, external_mode)
        references = read_reference_map(reference_manifest)
        cases = join_references(cases, references)
        rows = evaluate_rows(cases, thresholds)
        by_mode = group_aggregates(
            rows, lambda row: (row.case.mode,), thresholds, gate_mode=True
        )
        by_scene = group_aggregates(
            rows,
            lambda row: (row.case.mode, row.case.scene),
            thresholds,
            gate_mode=False,
        )
        by_scene_scale = group_aggregates(
            rows,
            lambda row: (row.case.mode, row.case.scene, row.case.scale),
            thresholds,
            gate_mode=False,
        )
        output_dir.mkdir(parents=True, exist_ok=True)
        results_path = output_dir / "quality-results.tsv"
        summary_path = output_dir / "quality-summary.json"
        write_results_tsv(results_path, rows)
        summary = write_summary_json(
            summary_path,
            manifest_path,
            reference_manifest,
            external_mode,
            thresholds,
            rows,
            by_mode,
            by_scene,
            by_scene_scale,
        )
    except (ExternalQualityError, OSError) as error:
        print(f"evaluate_external_aa_product.py: error: {error}", file=sys.stderr)
        return 1

    print(f"Wrote {results_path}")
    print(f"Wrote {summary_path}")
    aggregate = by_mode[0]
    print(
        f"{external_mode}: SSIM={aggregate.metrics.ssim:.6f}, "
        f"PSNR={product._format_psnr(aggregate.metrics.psnr_db)}, "
        f"row-failures={sum(bool(row.visual_failure) for row in rows)}"
    )
    if summary["status"] == "pass":
        print(f"Quality gate: PASS ({len(rows)} gated rows)")
        return 0
    print(
        f"Quality gate: FAIL ({summary['counts']['failures']} failures)",
        file=sys.stderr,
    )
    for failure in summary["failures"]:
        print(
            f"  {failure['gate']} {failure['scene'] or failure['mode']}: "
            f"{failure['message']}",
            file=sys.stderr,
        )
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
