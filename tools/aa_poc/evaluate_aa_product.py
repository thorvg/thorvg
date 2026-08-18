#!/usr/bin/env python3
"""Evaluate the product AA render suite and enforce its quality gates.

The renderer writes ``quality-manifest.tsv``.  This tool first validates that
every image was produced through the requested route with the requested root
sample count.  By default, only a structurally valid run is decoded and
compared with the SSAA reference.  ``--continue-valid-rows`` can recover visual
evidence from valid rows in a partially invalid run without making any invalid
mode eligible.  PNG decoding and metric conventions intentionally come from
``generate_aa_report.py``.  The runner converts premultiplied GL readback to
straight-alpha PNG after any box resolve, so the shared white-matte SSIM and
PSNR implementation applies directly.
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
from typing import Callable, Iterable, Optional, Sequence, Tuple

import generate_aa_report as report


MANIFEST_COLUMNS = (
    "mode",
    "scene",
    "scale",
    "offset_x",
    "offset_y",
    "candidate_png",
    "reference_png",
    "selected_mode",
    "root_samples",
    "route_valid",
    "expected_route_count",
    "actual_route_count",
    "total_route_count",
    "fallback_count",
)

DIAGNOSTIC_MODE = "noaa"
GATED_MODES = (
    "msaa4",
    "flat-direct",
    "curve-direct",
    "flat-mask",
    "curve-mask",
    "hybrid",
)
KNOWN_MODES = frozenset((DIAGNOSTIC_MODE,) + GATED_MODES)
FULL_SCENES = ("flat-core", "curve-core", "mixed-product-tile", "transparency-core")
FULL_SCALES = ("icon", "component", "large")
FULL_OFFSETS = ((0.0, 0.0), (0.125, 0.375), (0.5, 0.5), (0.875, 0.625))

DEFAULT_ROW_MIN_SSIM = 0.98
DEFAULT_ROW_MIN_PSNR_DB = 30.0
DEFAULT_MODE_MIN_SSIM = 0.995
DEFAULT_MODE_MIN_PSNR_DB = 35.0
THRESHOLD_RATIONALE = (
    "The per-row SSIM/PSNR floor is deliberately loose so one difficult scene "
    "or transform reveals a real regression without defining the normal score. "
    "The stricter mode aggregate is anchored below the existing MSAA4 baseline. "
    "These defaults were selected before evaluating this product-suite run."
)


class QualityGateError(RuntimeError):
    """A user-facing manifest or evaluation error."""


@dataclasses.dataclass(frozen=True)
class Thresholds:
    row_min_ssim: float = DEFAULT_ROW_MIN_SSIM
    row_min_psnr_db: float = DEFAULT_ROW_MIN_PSNR_DB
    mode_min_ssim: float = DEFAULT_MODE_MIN_SSIM
    mode_min_psnr_db: float = DEFAULT_MODE_MIN_PSNR_DB


@dataclasses.dataclass(frozen=True)
class ManifestCase:
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
    selected_mode: str
    root_samples: int
    route_valid: bool
    expected_route_count: int
    actual_route_count: int
    total_route_count: int
    fallback_count: int


@dataclasses.dataclass(frozen=True)
class Failure:
    gate: str
    scope: str
    message: str
    mode: str = ""
    scene: str = ""
    scale: Optional[str] = None
    offset_x: Optional[float] = None
    offset_y: Optional[float] = None
    row_number: Optional[int] = None


@dataclasses.dataclass(frozen=True)
class RowResult:
    case: ManifestCase
    invariant_failures: Tuple[str, ...]
    metrics: Optional[report.Metrics]
    visual_status: str
    visual_failure: str = ""

    @property
    def invariant_status(self) -> str:
        return "fail" if self.invariant_failures else "pass"

    @property
    def gate_status(self) -> str:
        if self.invariant_failures or self.visual_failure:
            return "fail"
        if self.case.mode == DIAGNOSTIC_MODE:
            return "diagnostic"
        if self.metrics is None:
            return "not-run"
        return "pass"


@dataclasses.dataclass(frozen=True)
class Aggregate:
    mode: str
    scene: Optional[str]
    scale: Optional[str]
    row_count: int
    manifest_row_count: int
    invariant_invalid_row_count: int
    metrics: Optional[report.Metrics]
    gate_status: str
    failure: str = ""
    visual_failure: str = ""


@dataclasses.dataclass(frozen=True)
class Evaluation:
    manifest_path: pathlib.Path
    thresholds: Thresholds
    rows: Tuple[RowResult, ...]
    by_mode: Tuple[Aggregate, ...]
    by_mode_scene: Tuple[Aggregate, ...]
    by_mode_scene_scale: Tuple[Aggregate, ...]
    failures: Tuple[Failure, ...]
    visual_evaluation_performed: bool
    continued_after_invariant_failures: bool
    full_suite_required: bool

    @property
    def passed(self) -> bool:
        return not self.failures


def _required_text(raw: dict, field: str, row_number: int) -> str:
    value = raw.get(field)
    if value is None or not value.strip():
        raise QualityGateError(f"manifest row {row_number}: {field} cannot be empty")
    return value.strip()


def _finite_float(raw: dict, field: str, row_number: int) -> float:
    text = _required_text(raw, field, row_number)
    try:
        value = float(text)
    except ValueError as error:
        raise QualityGateError(
            f"manifest row {row_number}: {field} must be a number, got {text!r}"
        ) from error
    if not math.isfinite(value):
        raise QualityGateError(
            f"manifest row {row_number}: {field} must be finite, got {text!r}"
        )
    return value


def _nonnegative_integer(raw: dict, field: str, row_number: int) -> int:
    text = _required_text(raw, field, row_number)
    try:
        value = int(text, 10)
    except ValueError as error:
        raise QualityGateError(
            f"manifest row {row_number}: {field} must be an integer, got {text!r}"
        ) from error
    if value < 0:
        raise QualityGateError(
            f"manifest row {row_number}: {field} cannot be negative, got {value}"
        )
    return value


def _resolve_image_path(manifest_dir: pathlib.Path, value: str) -> pathlib.Path:
    path = pathlib.Path(value)
    if not path.is_absolute():
        path = manifest_dir / path
    return path.resolve()


def _binary_integer(raw: dict, field: str, row_number: int) -> bool:
    value = _nonnegative_integer(raw, field, row_number)
    if value not in (0, 1):
        raise QualityGateError(
            f"manifest row {row_number}: {field} must be 0 or 1, got {value}"
        )
    return bool(value)


def read_manifest(path: pathlib.Path) -> Tuple[ManifestCase, ...]:
    """Read and validate the fixed runner manifest schema.

    Candidate and reference paths are resolved from the manifest directory,
    rather than from the evaluator's current working directory.
    """

    path = pathlib.Path(path).resolve()
    try:
        source = path.open("r", encoding="utf-8-sig", newline="")
    except OSError as error:
        raise QualityGateError(f"cannot read manifest {path}: {error}") from error

    manifest_dir = path.parent
    with source:
        reader = csv.DictReader(source, delimiter="\t")
        if reader.fieldnames is None:
            raise QualityGateError(f"manifest {path} is empty")
        missing = [column for column in MANIFEST_COLUMNS if column not in reader.fieldnames]
        if missing:
            raise QualityGateError(
                f"manifest {path} is missing required columns: {', '.join(missing)}"
            )

        cases = []
        for row_number, raw in enumerate(reader, start=2):
            if None in raw:
                raise QualityGateError(
                    f"manifest row {row_number} has more values than its header"
                )
            if all(value is None or not value.strip() for value in raw.values()):
                continue

            mode = _required_text(raw, "mode", row_number).lower()
            if mode not in KNOWN_MODES:
                supported = ", ".join((DIAGNOSTIC_MODE,) + GATED_MODES)
                raise QualityGateError(
                    f"manifest row {row_number}: unknown mode {mode!r}; expected one of {supported}"
                )
            scene = _required_text(raw, "scene", row_number)
            scale = _required_text(raw, "scale", row_number)
            offset_x = _finite_float(raw, "offset_x", row_number)
            offset_y = _finite_float(raw, "offset_y", row_number)
            candidate_png = _required_text(raw, "candidate_png", row_number)
            reference_png = _required_text(raw, "reference_png", row_number)
            selected_mode = _required_text(raw, "selected_mode", row_number).lower()
            if selected_mode not in KNOWN_MODES:
                raise QualityGateError(
                    f"manifest row {row_number}: unknown selected_mode {selected_mode!r}"
                )
            root_samples = _nonnegative_integer(raw, "root_samples", row_number)
            route_valid = _binary_integer(raw, "route_valid", row_number)
            expected_routes = _nonnegative_integer(
                raw, "expected_route_count", row_number
            )
            actual_routes = _nonnegative_integer(raw, "actual_route_count", row_number)
            total_routes = _nonnegative_integer(raw, "total_route_count", row_number)
            fallback_count = _nonnegative_integer(raw, "fallback_count", row_number)
            cases.append(
                ManifestCase(
                    row_number=row_number,
                    mode=mode,
                    scene=scene,
                    scale=scale,
                    offset_x=offset_x,
                    offset_y=offset_y,
                    candidate_png=candidate_png,
                    reference_png=reference_png,
                    candidate_path=_resolve_image_path(manifest_dir, candidate_png),
                    reference_path=_resolve_image_path(manifest_dir, reference_png),
                    selected_mode=selected_mode,
                    root_samples=root_samples,
                    route_valid=route_valid,
                    expected_route_count=expected_routes,
                    actual_route_count=actual_routes,
                    total_route_count=total_routes,
                    fallback_count=fallback_count,
                )
            )

    if not cases:
        raise QualityGateError(f"manifest {path} contains no data rows")
    return tuple(cases)


def _validate_case_matrix(
    cases: Sequence[ManifestCase], *, require_full_suite: bool
) -> None:
    seen = {}
    references = {}
    for case in cases:
        key = (case.mode, case.scene, case.scale, case.offset_x, case.offset_y)
        if key in seen:
            raise QualityGateError(
                f"manifest rows {seen[key]} and {case.row_number} duplicate case {key}"
            )
        seen[key] = case.row_number

        reference_key = (case.scene, case.scale, case.offset_x, case.offset_y)
        previous = references.setdefault(reference_key, case.reference_path)
        if previous != case.reference_path:
            raise QualityGateError(
                f"manifest row {case.row_number}: reference_png differs within "
                f"scene/scale/offset case {reference_key}"
            )

    if not require_full_suite:
        return
    expected = {
        (mode, scene, scale, offset_x, offset_y)
        for mode in (DIAGNOSTIC_MODE,) + GATED_MODES
        for scene in FULL_SCENES
        for scale in FULL_SCALES
        for offset_x, offset_y in FULL_OFFSETS
    }
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
        raise QualityGateError("full-suite matrix mismatch: " + "; ".join(details))


def _case_invariant_failures(case: ManifestCase) -> Tuple[str, ...]:
    failures = []
    if not case.route_valid:
        failures.append("route_valid is 0; runner route assertion failed")
    if case.selected_mode != case.mode:
        failures.append(
            f"selected_mode is {case.selected_mode}; requested {case.mode}"
        )
    if case.fallback_count != 0:
        failures.append(f"fallback_count is {case.fallback_count}; expected 0")
    if case.actual_route_count != case.expected_route_count:
        failures.append(
            f"actual_route_count is {case.actual_route_count}; "
            f"expected {case.expected_route_count}"
        )
    if case.total_route_count != case.expected_route_count:
        failures.append(
            f"total_route_count is {case.total_route_count}; "
            f"expected {case.expected_route_count}"
        )
    expected_samples = 4 if case.mode == "msaa4" else 1
    if case.root_samples != expected_samples:
        failures.append(
            f"root_samples is {case.root_samples}; expected {expected_samples} "
            f"for {case.mode}"
        )
    return tuple(failures)


def _row_failure(case: ManifestCase, gate: str, message: str) -> Failure:
    return Failure(
        gate=gate,
        scope="row",
        row_number=case.row_number,
        mode=case.mode,
        scene=case.scene,
        scale=case.scale,
        offset_x=case.offset_x,
        offset_y=case.offset_y,
        message=message,
    )


def _metric_failures(
    metrics: report.Metrics, min_ssim: float, min_psnr_db: float
) -> Tuple[str, ...]:
    failures = []
    if metrics.ssim < min_ssim:
        failures.append(f"SSIM {metrics.ssim:.6f} < {min_ssim:.6f}")
    if metrics.psnr_db < min_psnr_db:
        failures.append(f"PSNR {_format_psnr(metrics.psnr_db)} < {min_psnr_db:.3f} dB")
    return tuple(failures)


def _load_metrics(cases: Iterable[ManifestCase]) -> dict[int, report.Metrics]:
    decoded = {}

    def image(path: pathlib.Path) -> report.PngImage:
        if path not in decoded:
            decoded[path] = report.decode_png(path)
        return decoded[path]

    metrics = {}
    for case in cases:
        try:
            metrics[case.row_number] = report.compute_metrics(
                image(case.candidate_path), image(case.reference_path)
            )
        except (report.ReportError, OSError, ValueError) as error:
            raise QualityGateError(
                f"manifest row {case.row_number} ({case.mode}, {case.scene}, "
                f"scale={case.scale}, "
                f"offset={_format_offset(case.offset_x, case.offset_y)}): {error}"
            ) from error
    return metrics


def _group_aggregates(
    rows: Sequence[RowResult],
    key: Callable[[RowResult], tuple],
    thresholds: Thresholds,
    gate_modes: bool,
) -> Tuple[Aggregate, ...]:
    groups = OrderedDict()
    for row in rows:
        groups.setdefault(key(row), []).append(row)

    aggregates = []
    for group_key, members in groups.items():
        mode = group_key[0]
        scene = group_key[1] if len(group_key) > 1 else None
        scale = group_key[2] if len(group_key) > 2 else None
        valid_members = [
            member
            for member in members
            if not member.invariant_failures and member.metrics is not None
        ]
        invalid_count = sum(bool(member.invariant_failures) for member in members)
        combined = (
            report.combine_metrics([member.metrics for member in valid_members])
            if valid_members
            else None
        )
        eligibility_failure = (
            f"excluded {invalid_count} invariant-invalid row(s); candidate ineligible"
            if invalid_count
            else ""
        )
        visual_failures = []
        row_visual_failure_count = sum(bool(member.visual_failure) for member in members)
        if mode != DIAGNOSTIC_MODE and gate_modes and row_visual_failure_count:
            visual_failures.append(
                f"{row_visual_failure_count} required row(s) failed the row visual gate"
            )
        if mode != DIAGNOSTIC_MODE and gate_modes and combined is not None:
            aggregate_failure = "; ".join(
                _metric_failures(
                    combined, thresholds.mode_min_ssim, thresholds.mode_min_psnr_db
                )
            )
            if aggregate_failure:
                visual_failures.append(aggregate_failure)
        visual_failure = "; ".join(visual_failures)

        if eligibility_failure:
            status = "invalid"
        elif mode == DIAGNOSTIC_MODE:
            status = "diagnostic"
        elif gate_modes and visual_failure:
            status = "fail"
        elif gate_modes:
            status = "pass"
        else:
            status = "reported"
        failure = "; ".join(
            item for item in (eligibility_failure, visual_failure) if item
        )
        aggregates.append(
            Aggregate(
                mode=mode,
                scene=scene,
                scale=scale,
                row_count=len(valid_members),
                manifest_row_count=len(members),
                invariant_invalid_row_count=invalid_count,
                metrics=combined,
                gate_status=status,
                failure=failure,
                visual_failure=visual_failure,
            )
        )
    return tuple(aggregates)


def evaluate_manifest(
    path: pathlib.Path,
    thresholds: Thresholds,
    *,
    continue_valid_rows: bool = False,
    require_full_suite: bool = False,
) -> Evaluation:
    """Evaluate one manifest.

    Invariant-first evaluation remains the default.  Recovery mode decodes only
    invariant-valid rows and excludes every invalid row from all aggregates.
    """

    manifest_path = pathlib.Path(path).resolve()
    cases = read_manifest(manifest_path)
    _validate_case_matrix(cases, require_full_suite=require_full_suite)
    invariant_failures = {
        case.row_number: _case_invariant_failures(case) for case in cases
    }
    failures = []
    for case in cases:
        for message in invariant_failures[case.row_number]:
            failures.append(_row_failure(case, "invariant", message))

    if failures and not continue_valid_rows:
        rows = tuple(
            RowResult(
                case=case,
                invariant_failures=invariant_failures[case.row_number],
                metrics=None,
                visual_status="not-run",
            )
            for case in cases
        )
        return Evaluation(
            manifest_path=manifest_path,
            thresholds=thresholds,
            rows=rows,
            by_mode=(),
            by_mode_scene=(),
            by_mode_scene_scale=(),
            failures=tuple(failures),
            visual_evaluation_performed=False,
            continued_after_invariant_failures=False,
            full_suite_required=require_full_suite,
        )

    valid_cases = tuple(
        case for case in cases if not invariant_failures[case.row_number]
    )
    metrics_by_row = _load_metrics(valid_cases)
    rows = []
    for case in cases:
        if invariant_failures[case.row_number]:
            rows.append(
                RowResult(
                    case=case,
                    invariant_failures=invariant_failures[case.row_number],
                    metrics=None,
                    visual_status="not-run",
                )
            )
            continue
        metrics = metrics_by_row[case.row_number]
        if case.mode == DIAGNOSTIC_MODE:
            visual_status = "diagnostic"
            visual_failure = ""
        else:
            failed_metrics = _metric_failures(
                metrics, thresholds.row_min_ssim, thresholds.row_min_psnr_db
            )
            visual_failure = "; ".join(failed_metrics)
            visual_status = "fail" if visual_failure else "pass"
            if visual_failure:
                failures.append(_row_failure(case, "row-visual", visual_failure))
        rows.append(
            RowResult(
                case=case,
                invariant_failures=(),
                metrics=metrics,
                visual_status=visual_status,
                visual_failure=visual_failure,
            )
        )

    by_mode = _group_aggregates(
        rows, lambda row: (row.case.mode,), thresholds, gate_modes=True
    )
    by_mode_scene = _group_aggregates(
        rows,
        lambda row: (row.case.mode, row.case.scene),
        thresholds,
        gate_modes=False,
    )
    by_mode_scene_scale = _group_aggregates(
        rows,
        lambda row: (row.case.mode, row.case.scene, row.case.scale),
        thresholds,
        gate_modes=False,
    )
    for aggregate in by_mode:
        if aggregate.visual_failure:
            failures.append(
                Failure(
                    gate="mode-visual",
                    scope="mode",
                    mode=aggregate.mode,
                    message=aggregate.visual_failure,
                )
            )

    return Evaluation(
        manifest_path=manifest_path,
        thresholds=thresholds,
        rows=tuple(rows),
        by_mode=by_mode,
        by_mode_scene=by_mode_scene,
        by_mode_scene_scale=by_mode_scene_scale,
        failures=tuple(failures),
        visual_evaluation_performed=bool(valid_cases),
        continued_after_invariant_failures=(
            any(invariant_failures.values()) and continue_valid_rows
        ),
        full_suite_required=require_full_suite,
    )


def _format_number(value: float) -> str:
    return f"{value:.12g}"


def _format_offset(x: Optional[float], y: Optional[float]) -> str:
    if x is None or y is None:
        return ""
    return f"({_format_number(x)}, {_format_number(y)})"


def _format_psnr(value: float) -> str:
    return "infinity" if math.isinf(value) else f"{value:.6f} dB"


def _tsv_metric(value: float) -> str:
    return "infinity" if math.isinf(value) else f"{value:.9f}"


def _metric_dict(metrics: report.Metrics) -> dict:
    return {
        "ssim": metrics.ssim,
        "psnr_db": "infinity" if math.isinf(metrics.psnr_db) else metrics.psnr_db,
        "mean_absolute_error_8bit": metrics.mean_absolute_error,
        "max_absolute_error_8bit": metrics.max_absolute_error,
    }


def _threshold_dict(thresholds: Thresholds) -> dict:
    return {
        "row_min_ssim": thresholds.row_min_ssim,
        "row_min_psnr_db": thresholds.row_min_psnr_db,
        "mode_min_ssim": thresholds.mode_min_ssim,
        "mode_min_psnr_db": thresholds.mode_min_psnr_db,
    }


def _failure_dict(failure: Failure) -> dict:
    return {
        "gate": failure.gate,
        "scope": failure.scope,
        "row": failure.row_number,
        "mode": failure.mode or None,
        "scene": failure.scene or None,
        "scale": failure.scale,
        "offset": (
            {"x": failure.offset_x, "y": failure.offset_y}
            if failure.offset_x is not None and failure.offset_y is not None
            else None
        ),
        "message": failure.message,
    }


def _aggregate_dict(aggregate: Aggregate) -> dict:
    value = {
        "mode": aggregate.mode,
        "row_count": aggregate.row_count,
        "manifest_row_count": aggregate.manifest_row_count,
        "invariant_invalid_row_count": aggregate.invariant_invalid_row_count,
        "metrics": (
            _metric_dict(aggregate.metrics)
            if aggregate.metrics is not None
            else None
        ),
        "gate_status": aggregate.gate_status,
    }
    if aggregate.scene is not None:
        value["scene"] = aggregate.scene
    if aggregate.scale is not None:
        value["scale"] = aggregate.scale
    if aggregate.failure:
        value["failure"] = aggregate.failure
    return value


def _row_dict(row: RowResult) -> dict:
    case = row.case
    return {
        "manifest_row": case.row_number,
        "mode": case.mode,
        "scene": case.scene,
        "scale": case.scale,
        "offset": {"x": case.offset_x, "y": case.offset_y},
        "candidate_png": case.candidate_png,
        "reference_png": case.reference_png,
        "selected_mode": case.selected_mode,
        "root_samples": case.root_samples,
        "route_valid": case.route_valid,
        "expected_route_count": case.expected_route_count,
        "actual_route_count": case.actual_route_count,
        "total_route_count": case.total_route_count,
        "fallback_count": case.fallback_count,
        "metrics": _metric_dict(row.metrics) if row.metrics is not None else None,
        "invariant_status": row.invariant_status,
        "visual_status": row.visual_status,
        "included_in_visual_aggregates": (
            not row.invariant_failures and row.metrics is not None
        ),
        "gate_status": row.gate_status,
        "failures": list(row.invariant_failures)
        + ([row.visual_failure] if row.visual_failure else []),
    }


def _evaluation_stage(evaluation: Evaluation) -> str:
    if evaluation.continued_after_invariant_failures:
        return "partial-visual" if evaluation.visual_evaluation_performed else "invariant-gate"
    return "complete" if evaluation.visual_evaluation_performed else "invariant-gate"


def _summary_dict(evaluation: Evaluation) -> dict:
    invariant_count = sum(
        1 for failure in evaluation.failures if failure.gate == "invariant"
    )
    row_visual_count = sum(
        1 for failure in evaluation.failures if failure.gate == "row-visual"
    )
    mode_visual_count = sum(
        1 for failure in evaluation.failures if failure.gate == "mode-visual"
    )
    return {
        "schema_version": 1,
        "manifest": str(evaluation.manifest_path),
        "status": "pass" if evaluation.passed else "fail",
        "evaluation_stage": _evaluation_stage(evaluation),
        "full_suite_required": evaluation.full_suite_required,
        "gate_order": ["route-and-sample-invariants", "row-visual", "mode-visual"],
        "visual_evaluation": {
            "status": (
                "partial"
                if evaluation.continued_after_invariant_failures
                and evaluation.visual_evaluation_performed
                else "complete"
                if evaluation.visual_evaluation_performed
                else "not-run"
            ),
            "continued_after_invariant_failures": (
                evaluation.continued_after_invariant_failures
            ),
            "evaluated_rows": sum(
                row.metrics is not None for row in evaluation.rows
            ),
            "not_run_rows": sum(row.metrics is None for row in evaluation.rows),
            "aggregates_exclude_invariant_invalid_rows": True,
        },
        "diagnostic_mode": DIAGNOSTIC_MODE,
        "gated_modes": list(GATED_MODES),
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
            "defaults": _threshold_dict(Thresholds()),
            "active": _threshold_dict(evaluation.thresholds),
            "rationale": THRESHOLD_RATIONALE,
        },
        "counts": {
            "rows": len(evaluation.rows),
            "gated_rows": sum(
                row.case.mode != DIAGNOSTIC_MODE for row in evaluation.rows
            ),
            "diagnostic_rows": sum(
                row.case.mode == DIAGNOSTIC_MODE for row in evaluation.rows
            ),
            "failures": len(evaluation.failures),
            "invariant_failures": invariant_count,
            "row_visual_failures": row_visual_count,
            "mode_visual_failures": mode_visual_count,
            "visually_evaluated_rows": sum(
                row.metrics is not None for row in evaluation.rows
            ),
            "visual_not_run_rows": sum(
                row.metrics is None for row in evaluation.rows
            ),
        },
        "rows": [_row_dict(row) for row in evaluation.rows],
        "aggregates": {
            "by_mode": [_aggregate_dict(item) for item in evaluation.by_mode],
            "by_mode_scene": [
                _aggregate_dict(item) for item in evaluation.by_mode_scene
            ],
            "by_mode_scene_scale": [
                _aggregate_dict(item) for item in evaluation.by_mode_scene_scale
            ],
        },
        "failures": [_failure_dict(failure) for failure in evaluation.failures],
    }


def write_results_tsv(path: pathlib.Path, evaluation: Evaluation) -> None:
    fields = list(MANIFEST_COLUMNS) + [
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
        with pathlib.Path(path).open("w", encoding="utf-8", newline="") as output:
            writer = csv.DictWriter(output, fieldnames=fields, delimiter="\t")
            writer.writeheader()
            for row in evaluation.rows:
                case = row.case
                metrics = row.metrics
                failures = list(row.invariant_failures)
                if row.visual_failure:
                    failures.append(row.visual_failure)
                writer.writerow(
                    {
                        "mode": case.mode,
                        "scene": case.scene,
                        "scale": case.scale,
                        "offset_x": _format_number(case.offset_x),
                        "offset_y": _format_number(case.offset_y),
                        "candidate_png": case.candidate_png,
                        "reference_png": case.reference_png,
                        "selected_mode": case.selected_mode,
                        "root_samples": case.root_samples,
                        "route_valid": "1" if case.route_valid else "0",
                        "expected_route_count": case.expected_route_count,
                        "actual_route_count": case.actual_route_count,
                        "total_route_count": case.total_route_count,
                        "fallback_count": case.fallback_count,
                        "evaluation_stage": _evaluation_stage(evaluation),
                        "included_in_visual_aggregates": (
                            "yes"
                            if not row.invariant_failures and metrics is not None
                            else "no"
                        ),
                        "ssim": _tsv_metric(metrics.ssim) if metrics else "",
                        "psnr_db": _tsv_metric(metrics.psnr_db) if metrics else "",
                        "mean_absolute_error_8bit": (
                            _tsv_metric(metrics.mean_absolute_error)
                            if metrics
                            else ""
                        ),
                        "max_absolute_error_8bit": (
                            _tsv_metric(metrics.max_absolute_error) if metrics else ""
                        ),
                        "invariant_status": row.invariant_status,
                        "visual_status": row.visual_status,
                        "gate_status": row.gate_status,
                        "failures": "; ".join(failures),
                    }
                )
    except OSError as error:
        raise QualityGateError(f"cannot write results TSV {path}: {error}") from error


def write_summary_json(path: pathlib.Path, evaluation: Evaluation) -> None:
    try:
        pathlib.Path(path).write_text(
            json.dumps(_summary_dict(evaluation), indent=2, allow_nan=False) + "\n",
            encoding="utf-8",
        )
    except OSError as error:
        raise QualityGateError(f"cannot write summary JSON {path}: {error}") from error


def write_outputs(
    output_dir: pathlib.Path, evaluation: Evaluation
) -> Tuple[pathlib.Path, pathlib.Path]:
    output_dir = pathlib.Path(output_dir)
    try:
        output_dir.mkdir(parents=True, exist_ok=True)
    except OSError as error:
        raise QualityGateError(f"cannot create output directory {output_dir}: {error}") from error
    results_path = output_dir / "quality-results.tsv"
    summary_path = output_dir / "quality-summary.json"
    write_results_tsv(results_path, evaluation)
    write_summary_json(summary_path, evaluation)
    return results_path, summary_path


def _print_thresholds(active: Thresholds) -> None:
    defaults = Thresholds()
    print("Quality thresholds (selected before this run):")
    print(
        "  defaults: every gated row SSIM >= "
        f"{defaults.row_min_ssim:.3f}, PSNR >= {defaults.row_min_psnr_db:.1f} dB; "
        f"each gated mode aggregate SSIM >= {defaults.mode_min_ssim:.3f}, "
        f"PSNR >= {defaults.mode_min_psnr_db:.1f} dB"
    )
    print(f"  rationale: {THRESHOLD_RATIONALE}")
    print(
        "  active:   every gated row SSIM >= "
        f"{active.row_min_ssim:.6g}, PSNR >= {active.row_min_psnr_db:.6g} dB; "
        f"each gated mode aggregate SSIM >= {active.mode_min_ssim:.6g}, "
        f"PSNR >= {active.mode_min_psnr_db:.6g} dB"
    )
    print("  noaa is diagnostic only for visual scores; invariants still apply.")


def _print_failure_table(failures: Sequence[Failure]) -> None:
    if not failures:
        return
    headers = ("gate", "scope", "row", "mode", "scene", "scale", "offset", "failure")
    values = [
        (
            failure.gate,
            failure.scope,
            str(failure.row_number) if failure.row_number is not None else "-",
            failure.mode or "-",
            failure.scene or "-",
            failure.scale if failure.scale is not None else "-",
            _format_offset(failure.offset_x, failure.offset_y) or "-",
            failure.message,
        )
        for failure in failures
    ]
    widths = [
        max(len(headers[index]), *(len(row[index]) for row in values))
        for index in range(len(headers))
    ]

    def line(row: Sequence[str]) -> str:
        return " | ".join(value.ljust(widths[index]) for index, value in enumerate(row))

    print("Quality gate failures:", file=sys.stderr)
    print(line(headers), file=sys.stderr)
    print("-+-".join("-" * width for width in widths), file=sys.stderr)
    for row in values:
        print(line(row), file=sys.stderr)


def _ssim_threshold(text: str) -> float:
    try:
        value = float(text)
    except ValueError as error:
        raise argparse.ArgumentTypeError("must be a number") from error
    if not math.isfinite(value) or not 0.0 <= value <= 1.0:
        raise argparse.ArgumentTypeError("must be finite and between 0 and 1")
    return value


def _psnr_threshold(text: str) -> float:
    try:
        value = float(text)
    except ValueError as error:
        raise argparse.ArgumentTypeError("must be a number") from error
    if not math.isfinite(value) or value < 0.0:
        raise argparse.ArgumentTypeError("must be finite and nonnegative")
    return value


def build_argument_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "Evaluate quality-manifest.tsv, write machine-readable results, and "
            "fail when AA routing, sampling, or preselected visual gates regress."
        ),
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument(
        "manifest",
        nargs="?",
        default="quality-manifest.tsv",
        help="runner manifest; image paths inside it are relative to this file",
    )
    parser.add_argument(
        "--output-dir",
        help="directory for quality-results.tsv and quality-summary.json (manifest directory by default)",
    )
    parser.add_argument(
        "--continue-valid-rows",
        action="store_true",
        help=(
            "after invariant failures, evaluate only invariant-valid rows; "
            "invalid rows and their modes remain failed and ineligible"
        ),
    )
    parser.add_argument(
        "--require-full-suite",
        action="store_true",
        help=(
            "require the complete 7-mode x 4-scene x 3-scale x 4-offset matrix"
        ),
    )
    parser.add_argument(
        "--row-min-ssim",
        type=_ssim_threshold,
        default=DEFAULT_ROW_MIN_SSIM,
        help="minimum SSIM for every visually gated manifest row",
    )
    parser.add_argument(
        "--row-min-psnr",
        dest="row_min_psnr_db",
        type=_psnr_threshold,
        default=DEFAULT_ROW_MIN_PSNR_DB,
        metavar="DB",
        help="minimum PSNR in dB for every visually gated manifest row",
    )
    parser.add_argument(
        "--mode-min-ssim",
        type=_ssim_threshold,
        default=DEFAULT_MODE_MIN_SSIM,
        help="minimum SSIM for each visually gated mode aggregate",
    )
    parser.add_argument(
        "--mode-min-psnr",
        dest="mode_min_psnr_db",
        type=_psnr_threshold,
        default=DEFAULT_MODE_MIN_PSNR_DB,
        metavar="DB",
        help="minimum PSNR in dB for each visually gated mode aggregate",
    )
    return parser


def main(argv: Optional[Sequence[str]] = None) -> int:
    args = build_argument_parser().parse_args(argv)
    manifest_path = pathlib.Path(args.manifest).resolve()
    output_dir = (
        pathlib.Path(args.output_dir).resolve()
        if args.output_dir
        else manifest_path.parent
    )
    thresholds = Thresholds(
        row_min_ssim=args.row_min_ssim,
        row_min_psnr_db=args.row_min_psnr_db,
        mode_min_ssim=args.mode_min_ssim,
        mode_min_psnr_db=args.mode_min_psnr_db,
    )
    _print_thresholds(thresholds)
    try:
        evaluation = evaluate_manifest(
            manifest_path,
            thresholds,
            continue_valid_rows=args.continue_valid_rows,
            require_full_suite=args.require_full_suite,
        )
        results_path, summary_path = write_outputs(output_dir, evaluation)
    except QualityGateError as error:
        print(f"evaluate_aa_product.py: error: {error}", file=sys.stderr)
        return 1

    print(f"Wrote {results_path}")
    print(f"Wrote {summary_path}")
    if evaluation.passed:
        gated_rows = sum(
            row.case.mode != DIAGNOSTIC_MODE for row in evaluation.rows
        )
        diagnostic_rows = len(evaluation.rows) - gated_rows
        print(
            f"Quality gate: PASS ({gated_rows} gated rows, "
            f"{diagnostic_rows} noaa diagnostic rows)"
        )
        return 0

    _print_failure_table(evaluation.failures)
    if evaluation.continued_after_invariant_failures:
        evaluated_rows = sum(row.metrics is not None for row in evaluation.rows)
        skipped_rows = len(evaluation.rows) - evaluated_rows
        print(
            f"Partial visual evaluation: evaluated {evaluated_rows} invariant-valid "
            f"rows; {skipped_rows} invariant-invalid rows were not run and were "
            "excluded from aggregates. Their modes remain ineligible.",
            file=sys.stderr,
        )
    elif not evaluation.visual_evaluation_performed:
        print(
            "Visual evaluation was not run because route/sample invariants failed.",
            file=sys.stderr,
        )
    print(f"Quality gate: FAIL ({len(evaluation.failures)} failures)", file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())
