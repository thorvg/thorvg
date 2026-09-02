#!/usr/bin/env python3
"""Evaluate and export the original AAA comparison scene by characteristic.

The render inputs remain the original combined 800 x 480 comparison fixture.
This tool is deliberately feature-first: it scores each of the eight established
characteristic regions independently and does not emit a whole-frame ranking or
apply the product-suite quality gate.

Minimal reference-manifest columns::

    scale  offset_x  offset_y  reference_png

Minimal candidate-manifest columns::

    mode  scale  offset_x  offset_y  candidate_png

Columns are tab-separated. Extra provenance columns are allowed. Relative PNG
paths are resolved against the directory containing their manifest.
"""

from __future__ import annotations

import argparse
import csv
import dataclasses
import json
import math
import os
import pathlib
import re
import shutil
import sys
import tempfile
from typing import Iterable, Mapping, Optional, Sequence

import generate_aa_report as report


OFFSETS = ((0.0, 0.0), (0.125, 0.375), (0.5, 0.5), (0.875, 0.625))
VISUAL_OFFSET = (0.125, 0.375)


@dataclasses.dataclass(frozen=True)
class ScaleSpec:
    key: str
    label: str
    factor: float
    width: int
    height: int


SCALES = (
    ScaleSpec("quarter", "25%（200×120）", 0.25, 200, 120),
    ScaleSpec("half", "50%（400×240）", 0.5, 400, 240),
    ScaleSpec("original", "原始尺寸（800×480）", 1.0, 800, 480),
)
SCALE_BY_KEY = {scale.key: scale for scale in SCALES}


@dataclasses.dataclass(frozen=True)
class CharacteristicSpec:
    key: str
    label: str
    description: str
    base_roi: tuple[int, int, int, int]
    base_zoom_roi: tuple[int, int, int, int]


# The characteristic ROIs are the established cells from generate_aa_report.py.
# Zoom ROIs are fixed 32 x 32 authored-pixel regions around a representative
# edge or join. They are never chosen from candidate error data.
CHARACTERISTICS = (
    CharacteristicSpec(
        "slanted-edges",
        "長對角邊緣",
        "長對角線的像素覆蓋階梯。",
        (0, 0, 200, 210),
        (96, 56, 128, 88),
    ),
    CharacteristicSpec(
        "corner-joins",
        "銳角與鈍角",
        "銳角與鈍角的覆蓋品質。",
        (200, 0, 391, 210),
        (272, 20, 304, 52),
    ),
    CharacteristicSpec(
        "circle-curvature",
        "圓形",
        "固定曲率邊緣的覆蓋品質。",
        (391, 0, 593, 210),
        (524, 36, 556, 68),
    ),
    CharacteristicSpec(
        "changing-curvature",
        "曲率變化",
        "曲率改變時的邊緣覆蓋品質。",
        (593, 0, 800, 210),
        (664, 32, 696, 64),
    ),
    CharacteristicSpec(
        "shallow-edge-motion",
        "近水平邊緣",
        "近水平邊緣在次像素位移下的變化。",
        (0, 210, 207, 480),
        (96, 320, 128, 352),
    ),
    CharacteristicSpec(
        "mixed-endpoints",
        "直線與曲線交會",
        "直線段與三次貝茲曲線的交界處。",
        (207, 210, 400, 480),
        (220, 256, 252, 288),
    ),
    CharacteristicSpec(
        "cubic-join-seams",
        "曲線接合",
        "連續三次貝茲曲線的共用端點。",
        (400, 210, 629, 480),
        (504, 288, 536, 320),
    ),
    CharacteristicSpec(
        "translucent-overdraw",
        "半透明邊緣",
        "半透明三次曲線邊界及其閉合接合處。",
        (629, 210, 800, 480),
        (636, 352, 668, 384),
    ),
)


@dataclasses.dataclass(frozen=True, order=True)
class CaseKey:
    scale: str
    offset_x: float
    offset_y: float


@dataclasses.dataclass(frozen=True)
class Frame:
    row_number: int
    key: CaseKey
    declared_path: str
    path: pathlib.Path
    mode: Optional[str] = None


@dataclasses.dataclass(frozen=True)
class MetricRow:
    renderer: str
    mode: str
    characteristic: CharacteristicSpec
    case: CaseKey
    target_width: int
    target_height: int
    roi: tuple[int, int, int, int]
    candidate_png: str
    reference_png: str
    metrics: report.Metrics


class DiagnosticError(RuntimeError):
    """A user-facing manifest, image, evaluation, or export error."""


def planned_keys() -> tuple[CaseKey, ...]:
    return tuple(
        CaseKey(scale.key, offset_x, offset_y)
        for scale in SCALES
        for offset_x, offset_y in OFFSETS
    )


def _required_text(raw: Mapping[str, Optional[str]], field: str, row: int) -> str:
    value = raw.get(field)
    if value is None or not value.strip():
        raise DiagnosticError(f"manifest row {row}: {field} cannot be empty")
    return value.strip()


def _finite_float(raw: Mapping[str, Optional[str]], field: str, row: int) -> float:
    text = _required_text(raw, field, row)
    try:
        value = float(text)
    except ValueError as error:
        raise DiagnosticError(
            f"manifest row {row}: {field} must be a number, got {text!r}"
        ) from error
    if not math.isfinite(value):
        raise DiagnosticError(f"manifest row {row}: {field} must be finite")
    return value


def _resolve_declared_path(manifest: pathlib.Path, declared: str) -> pathlib.Path:
    path = pathlib.Path(declared)
    if not path.is_absolute():
        path = manifest.parent / path
    return path.resolve()


def read_manifest(
    path: pathlib.Path,
    *,
    path_column: str,
    expected_mode: Optional[str] = None,
) -> dict[CaseKey, Frame]:
    """Read one strict 12-frame manifest.

    Extra columns are allowed so renderer-specific provenance can travel beside
    the common matrix fields without being trusted by this evaluator.
    """

    path = pathlib.Path(path).resolve()
    required = {"scale", "offset_x", "offset_y", path_column}
    if expected_mode is not None:
        required.add("mode")
    try:
        source = path.open("r", encoding="utf-8-sig", newline="")
    except OSError as error:
        raise DiagnosticError(f"cannot read manifest {path}: {error}") from error

    frames: dict[CaseKey, Frame] = {}
    with source:
        reader = csv.DictReader(source, delimiter="\t")
        if reader.fieldnames is None:
            raise DiagnosticError(f"manifest {path} is empty")
        missing_columns = sorted(required.difference(reader.fieldnames))
        if missing_columns:
            raise DiagnosticError(
                f"manifest {path} is missing required columns: "
                + ", ".join(missing_columns)
            )
        has_scene = "scene" in reader.fieldnames
        for row_number, raw in enumerate(reader, start=2):
            if None in raw:
                raise DiagnosticError(
                    f"manifest row {row_number} has more values than its header"
                )
            if all(value is None or not value.strip() for value in raw.values()):
                continue
            scale = _required_text(raw, "scale", row_number)
            if has_scene:
                scene = _required_text(raw, "scene", row_number).lower()
                if scene != "comparison":
                    raise DiagnosticError(
                        f"manifest row {row_number}: scene is {scene!r}; "
                        "expected 'comparison'"
                    )
            offset_x = _finite_float(raw, "offset_x", row_number)
            offset_y = _finite_float(raw, "offset_y", row_number)
            key = CaseKey(scale, offset_x, offset_y)
            if key in frames:
                raise DiagnosticError(
                    f"manifest rows {frames[key].row_number} and {row_number} "
                    f"duplicate case {key}"
                )
            mode = None
            if expected_mode is not None:
                mode = _required_text(raw, "mode", row_number).lower()
                if mode != expected_mode.lower():
                    raise DiagnosticError(
                        f"manifest row {row_number}: mode is {mode!r}; "
                        f"expected {expected_mode.lower()!r}"
                    )
            declared = _required_text(raw, path_column, row_number)
            frames[key] = Frame(
                row_number=row_number,
                key=key,
                declared_path=declared,
                path=_resolve_declared_path(path, declared),
                mode=mode,
            )

    expected = set(planned_keys())
    actual = set(frames)
    missing = sorted(expected - actual)
    unexpected = sorted(actual - expected)
    if missing or unexpected:
        details = []
        if missing:
            details.append(f"missing {len(missing)} case(s), first={missing[0]}")
        if unexpected:
            details.append(
                f"contains {len(unexpected)} unexpected case(s), first={unexpected[0]}"
            )
        raise DiagnosticError(f"manifest {path} matrix mismatch: " + "; ".join(details))
    return frames


def scaled_roi(
    roi: tuple[int, int, int, int], scale: ScaleSpec
) -> tuple[int, int, int, int]:
    x0, y0, x1, y1 = roi
    scaled = (
        math.floor(x0 * scale.factor),
        math.floor(y0 * scale.factor),
        math.ceil(x1 * scale.factor),
        math.ceil(y1 * scale.factor),
    )
    sx0, sy0, sx1, sy1 = scaled
    if not (0 <= sx0 < sx1 <= scale.width and 0 <= sy0 < sy1 <= scale.height):
        raise DiagnosticError(
            f"scaled ROI {scaled} is outside {scale.width}x{scale.height} for {scale.key}"
        )
    return scaled


class ImageCache:
    def __init__(self) -> None:
        self._images: dict[pathlib.Path, report.PngImage] = {}

    def get(self, path: pathlib.Path) -> report.PngImage:
        path = pathlib.Path(path).resolve()
        if path not in self._images:
            if not path.is_file():
                raise DiagnosticError(f"PNG does not exist or is not a file: {path}")
            try:
                self._images[path] = report.decode_png(path)
            except (OSError, ValueError, report.ReportError) as error:
                raise DiagnosticError(f"cannot decode PNG {path}: {error}") from error
        return self._images[path]


def validate_frames(
    named_frames: Mapping[str, Mapping[CaseKey, Frame]], cache: ImageCache
) -> None:
    expected_keys = planned_keys()
    for key in expected_keys:
        scale = SCALE_BY_KEY[key.scale]
        dimensions = set()
        for source_name, frames in named_frames.items():
            frame = frames[key]
            image = cache.get(frame.path)
            dimensions.add((image.width, image.height))
            if (image.width, image.height) != (scale.width, scale.height):
                raise DiagnosticError(
                    f"{source_name} manifest row {frame.row_number} ({key}) points to "
                    f"{image.width}x{image.height}; expected {scale.width}x{scale.height}"
                )
        if len(dimensions) != 1:
            raise DiagnosticError(f"comparison images have different dimensions for {key}")


def evaluate(
    references: Mapping[CaseKey, Frame],
    candidates: Sequence[tuple[str, str, Mapping[CaseKey, Frame]]],
    cache: ImageCache,
) -> tuple[MetricRow, ...]:
    rows = []
    for characteristic in CHARACTERISTICS:
        for scale in SCALES:
            scale_cases = tuple(
                case for case in planned_keys() if case.scale == scale.key
            )
            search_roi = scaled_roi(characteristic.base_roi, scale)
            roi_sources = []
            for case in scale_cases:
                roi_sources.append(cache.get(references[case].path))
                for _renderer, _mode, frames in candidates:
                    roi_sources.append(cache.get(frames[case].path))
            try:
                metric_roi = report.content_roi_within(
                    tuple(roi_sources), search_roi, padding=4
                )
            except (ValueError, report.ReportError) as error:
                raise DiagnosticError(
                    f"cannot derive shared metric ROI for {characteristic.key}/"
                    f"{scale.key}: {error}"
                ) from error

            for case in scale_cases:
                reference_frame = references[case]
                reference_image = cache.get(reference_frame.path)
                for renderer, mode, frames in candidates:
                    candidate_frame = frames[case]
                    try:
                        metrics = report.compute_metrics(
                            cache.get(candidate_frame.path),
                            reference_image,
                            metric_roi,
                        )
                    except (ValueError, report.ReportError) as error:
                        raise DiagnosticError(
                            f"cannot evaluate {renderer}, {characteristic.key}, "
                            f"{case}: {error}"
                        ) from error
                    rows.append(
                        MetricRow(
                            renderer=renderer,
                            mode=mode,
                            characteristic=characteristic,
                            case=case,
                            target_width=scale.width,
                            target_height=scale.height,
                            roi=metric_roi,
                            candidate_png=candidate_frame.declared_path,
                            reference_png=reference_frame.declared_path,
                            metrics=metrics,
                        )
                    )
    expected_count = len(CHARACTERISTICS) * len(planned_keys()) * len(candidates)
    if len(rows) != expected_count:
        raise DiagnosticError(
            f"internal row-count mismatch: produced {len(rows)}, expected {expected_count}"
        )
    return tuple(rows)


def _format_number(value: float) -> str:
    text = f"{value:.3f}".rstrip("0").rstrip(".")
    return text if text else "0"


def _format_metric(value: float, digits: int = 9) -> str:
    return "infinity" if math.isinf(value) else f"{value:.{digits}f}"


def _format_psnr(value: float) -> str:
    return "∞" if math.isinf(value) else f"{value:.3f} dB"


def write_results_tsv(path: pathlib.Path, rows: Sequence[MetricRow]) -> None:
    fields = (
        "renderer",
        "mode",
        "characteristic",
        "scale",
        "target_width",
        "target_height",
        "offset_x",
        "offset_y",
        "roi_x",
        "roi_y",
        "roi_width",
        "roi_height",
        "candidate_png",
        "reference_png",
        "ssim",
        "psnr_db",
    )
    with path.open("w", encoding="utf-8", newline="") as output:
        writer = csv.DictWriter(output, fieldnames=fields, delimiter="\t")
        writer.writeheader()
        for row in rows:
            x0, y0, x1, y1 = row.roi
            writer.writerow(
                {
                    "renderer": row.renderer,
                    "mode": row.mode,
                    "characteristic": row.characteristic.key,
                    "scale": row.case.scale,
                    "target_width": row.target_width,
                    "target_height": row.target_height,
                    "offset_x": _format_number(row.case.offset_x),
                    "offset_y": _format_number(row.case.offset_y),
                    "roi_x": x0,
                    "roi_y": y0,
                    "roi_width": x1 - x0,
                    "roi_height": y1 - y0,
                    "candidate_png": row.candidate_png,
                    "reference_png": row.reference_png,
                    "ssim": _format_metric(row.metrics.ssim),
                    "psnr_db": _format_metric(row.metrics.psnr_db),
                }
            )


def _metric_json(metrics: report.Metrics) -> dict:
    return {
        "ssim": metrics.ssim,
        "psnr_db": "infinity" if math.isinf(metrics.psnr_db) else metrics.psnr_db,
    }


def write_summary_json(
    path: pathlib.Path,
    rows: Sequence[MetricRow],
    reference_manifest: pathlib.Path,
    candidate_manifests: Mapping[str, pathlib.Path],
) -> None:
    payload = {
        "schema_version": 1,
        "evaluation_profile": "original-combined-fixture-feature-diagnostic",
        "evaluation_stage": "complete",
        "quality_gate": "not-applied",
        "global_mixed_feature_conclusion": "not-computed",
        "matrix": {
            "fixture": "original 800x480 eight-characteristic comparison scene",
            "scales": [
                {
                    "key": scale.key,
                    "factor": scale.factor,
                    "width": scale.width,
                    "height": scale.height,
                }
                for scale in SCALES
            ],
            "offsets": [{"x": x, "y": y} for x, y in OFFSETS],
            "frames_per_source": len(planned_keys()),
            "characteristics": [item.key for item in CHARACTERISTICS],
            "metric_rows": len(rows),
        },
        "metric_conventions": {
            "alpha": "straight-alpha PNG composited over white",
            "psnr": "RGB, peak=255",
            "ssim": (
                "Rec.709 luminance, valid uniform windows up to 11x11, "
                "population moments, K1=0.01, K2=0.03, L=255"
            ),
            "roi": (
                "one shared tight visible-content union per characteristic and scale, "
                "derived across SSAA8, ThorVG, Vello, and all four offsets with "
                "four target-pixel padding"
            ),
            "aggregation": (
                "only the four offsets within the same characteristic and scale; "
                "no cross-feature or cross-scale aggregate"
            ),
        },
        "visuals": {
            "offset": {"x": VISUAL_OFFSET[0], "y": VISUAL_OFFSET[1]},
            "full_frame": "white-matted source frame without overlays",
            "characteristic_crop": "exact scaled characteristic ROI without overlays",
            "pixel_zoom": (
                "fixed authored 32x32 feature ROI, nearest-neighbor enlarged to 256x256"
            ),
        },
        "inputs": {
            "reference_manifest": str(pathlib.Path(reference_manifest).resolve()),
            "candidate_manifests": {
                renderer: str(pathlib.Path(manifest).resolve())
                for renderer, manifest in candidate_manifests.items()
            },
        },
        "rows": [
            {
                "renderer": row.renderer,
                "mode": row.mode,
                "characteristic": row.characteristic.key,
                "scale": row.case.scale,
                "target": {"width": row.target_width, "height": row.target_height},
                "offset": {"x": row.case.offset_x, "y": row.case.offset_y},
                "roi": {
                    "x": row.roi[0],
                    "y": row.roi[1],
                    "width": row.roi[2] - row.roi[0],
                    "height": row.roi[3] - row.roi[1],
                },
                "candidate_png": row.candidate_png,
                "reference_png": row.reference_png,
                "metrics": _metric_json(row.metrics),
            }
            for row in rows
        ],
        "feature_scale_aggregates": [
            {
                "renderer": renderer,
                "characteristic": characteristic.key,
                "scale": scale.key,
                "offset_count": len(OFFSETS),
                "metrics": _metric_json(
                    report.combine_metrics(
                        [
                            row.metrics
                            for row in rows
                            if row.renderer == renderer
                            and row.characteristic.key == characteristic.key
                            and row.case.scale == scale.key
                        ]
                    )
                ),
            }
            for characteristic in CHARACTERISTICS
            for scale in SCALES
            for renderer in ("thorvg", "vello")
        ],
    }
    path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")


def _nearest_rgb(
    rgb: bytes, width: int, height: int, factor: int
) -> tuple[int, int, bytes]:
    if factor <= 0:
        raise ValueError("nearest-neighbor factor must be positive")
    output_width = width * factor
    output_height = height * factor
    output = bytearray(output_width * output_height * 3)
    for source_y in range(height):
        row = bytearray()
        source_start = source_y * width * 3
        for source_x in range(width):
            pixel = source_start + source_x * 3
            row.extend(rgb[pixel : pixel + 3] * factor)
        for duplicate in range(factor):
            destination = (source_y * factor + duplicate) * output_width * 3
            output[destination : destination + len(row)] = row
    return output_width, output_height, bytes(output)


def _write_zoom(path: pathlib.Path, image: report.PngImage) -> None:
    if image.width != image.height or 256 % image.width != 0:
        raise DiagnosticError(
            f"fixed zoom crop must be square and divide 256, got {image.width}x{image.height}"
        )
    width, height, rgb = _nearest_rgb(
        report.composite_over_white(image), image.width, image.height, 256 // image.width
    )
    report.encode_png_rgb(path, width, height, rgb)


def _case_for_offset(scale: ScaleSpec, offset: tuple[float, float]) -> CaseKey:
    key = CaseKey(scale.key, offset[0], offset[1])
    if key not in set(planned_keys()):
        raise DiagnosticError(f"visual case is outside the planned matrix: {key}")
    return key


def write_visuals(
    output_dir: pathlib.Path,
    references: Mapping[CaseKey, Frame],
    candidates: Sequence[tuple[str, str, Mapping[CaseKey, Frame]]],
    cache: ImageCache,
) -> dict[tuple[str, str, str, str], pathlib.Path]:
    """Write raw-derived presentation images and return their relative paths."""

    sources = [("ssaa8", "SSAA8 reference", references)] + [
        (renderer, renderer, frames) for renderer, _mode, frames in candidates
    ]
    paths: dict[tuple[str, str, str, str], pathlib.Path] = {}
    visuals = output_dir / "visuals"
    for scale in SCALES:
        case = _case_for_offset(scale, VISUAL_OFFSET)
        for source_key, _label, frames in sources:
            image = cache.get(frames[case].path)
            relative = pathlib.Path("visuals") / "full" / scale.key / f"{source_key}.png"
            destination = output_dir / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            report.write_png_image(destination, image)
            paths[("full", "", scale.key, source_key)] = relative

        for characteristic in CHARACTERISTICS:
            exact_roi = scaled_roi(characteristic.base_roi, scale)
            zoom_roi = scaled_roi(characteristic.base_zoom_roi, scale)
            for source_key, _label, frames in sources:
                image = cache.get(frames[case].path)
                root = pathlib.Path("visuals") / characteristic.key / scale.key
                crop_relative = root / f"{source_key}-crop.png"
                zoom_relative = root / f"{source_key}-zoom.png"
                crop_destination = output_dir / crop_relative
                crop_destination.parent.mkdir(parents=True, exist_ok=True)
                report.write_png_image(
                    crop_destination, report.crop_image(image, exact_roi)
                )
                _write_zoom(
                    output_dir / zoom_relative, report.crop_image(image, zoom_roi)
                )
                paths[("crop", characteristic.key, scale.key, source_key)] = crop_relative
                paths[("zoom", characteristic.key, scale.key, source_key)] = zoom_relative
    return paths


def _image_table(
    paths: Sequence[tuple[str, pathlib.Path]],
) -> list[str]:
    labels = [label for label, _ in paths]
    relative = [path.as_posix() for _, path in paths]
    return [
        "| " + " | ".join(labels) + " |",
        "| " + " | ".join("---" for _ in paths) + " |",
        "| "
        + " | ".join(
            f"![{label}]({path})" for label, path in zip(labels, relative)
        )
        + " |",
    ]


def render_markdown(
    rows: Sequence[MetricRow],
    visuals: Mapping[tuple[str, str, str, str], pathlib.Path],
    thorvg_mode: str,
    vello_mode: str,
) -> str:
    by_key = {
        (
            row.characteristic.key,
            row.case.scale,
            row.case.offset_x,
            row.case.offset_y,
            row.renderer,
        ): row
        for row in rows
    }
    if len(by_key) != len(rows):
        raise DiagnosticError("metric rows contain duplicate report keys")

    display_sources = (
        ("ssaa8", "SSAA8 參考"),
        ("thorvg", "目前 ThorVG WebGPU"),
        ("vello", "Vello WebGPU"),
    )
    lines = [
        "# 目前 ThorVG WebGPU 與 Vello — 依特徵分離的 AAA 診斷",
        "",
        "渲染輸入是原始的 800×480、八個圖形比較 fixture，內容完全不變。每張表只在該特徵的 ROI 內與對應的 SSAA8 畫面比較；不計算整張畫面的分數，也不跨特徵排名。",
        "",
        "這裡的「目前 ThorVG WebGPU」是指由目前工作樹建置的 renderer，不是新的 fixture，也不是替代測試套件。",
        "",
        f"ThorVG 模式：`{thorvg_mode}`。Vello 模式：`{vello_mode}`。",
        "",
        "測試矩陣只增加尺寸變化：三種尺寸（`200×120`、`400×240`、`800×480`）乘上四組目標像素 offset。圖片固定顯示 offset `(0.125, 0.375)`；四組 offset 的數值都保留在表中。",
        "",
        "## 依特徵整理的結果",
        "",
        "這只是分離結果的索引，不是混合分數。`12/12` 表示在每個尺寸與 offset 組合中，SSIM 與 PSNR 都判定同一個 renderer 較接近 SSAA8。",
        "",
        "| 特徵 | 較接近 SSAA8 | 一致案例 |",
        "| --- | --- | ---: |",
    ]

    characteristic_results: dict[str, tuple[str, int]] = {}
    for characteristic in CHARACTERISTICS:
        thorvg_wins = 0
        vello_wins = 0
        for scale in SCALES:
            for offset_x, offset_y in OFFSETS:
                thorvg = by_key[
                    (characteristic.key, scale.key, offset_x, offset_y, "thorvg")
                ].metrics
                vello = by_key[
                    (characteristic.key, scale.key, offset_x, offset_y, "vello")
                ].metrics
                if thorvg.ssim > vello.ssim and thorvg.psnr_db > vello.psnr_db:
                    thorvg_wins += 1
                elif vello.ssim > thorvg.ssim and vello.psnr_db > thorvg.psnr_db:
                    vello_wins += 1
        if thorvg_wins == len(SCALES) * len(OFFSETS):
            result = ("目前 ThorVG WebGPU", thorvg_wins)
        elif vello_wins == len(SCALES) * len(OFFSETS):
            result = ("Vello WebGPU", vello_wins)
        else:
            result = ("結果不固定；請查看個別案例", thorvg_wins + vello_wins)
        characteristic_results[characteristic.key] = result
        lines.append(
            f"| {characteristic.label} | {result[0]} | "
            f"{result[1]}/{len(SCALES) * len(OFFSETS)} |"
        )

    for characteristic in CHARACTERISTICS:
        preferred, agreement = characteristic_results[characteristic.key]
        lines.extend(
            [
                "",
                f"## {characteristic.label}",
                "",
                characteristic.description,
                "",
                f"結果：在兩個指標判斷一致的案例中，{preferred} 有 "
                f"{agreement}/{len(SCALES) * len(OFFSETS)} 個案例較接近 SSAA8。",
                "",
                "四個 offset 的彙總：",
                "",
                "| 尺寸 | ThorVG SSIM | ThorVG PSNR | Vello SSIM | Vello PSNR |",
                "| --- | ---: | ---: | ---: | ---: |",
            ]
        )
        for scale in SCALES:
            thorvg_aggregate = report.combine_metrics(
                [
                    by_key[
                        (characteristic.key, scale.key, offset_x, offset_y, "thorvg")
                    ].metrics
                    for offset_x, offset_y in OFFSETS
                ]
            )
            vello_aggregate = report.combine_metrics(
                [
                    by_key[
                        (characteristic.key, scale.key, offset_x, offset_y, "vello")
                    ].metrics
                    for offset_x, offset_y in OFFSETS
                ]
            )
            lines.append(
                f"| {scale.label} | {thorvg_aggregate.ssim:.6f} "
                f"| {_format_psnr(thorvg_aggregate.psnr_db)} "
                f"| {vello_aggregate.ssim:.6f} "
                f"| {_format_psnr(vello_aggregate.psnr_db)} |"
            )

        lines.extend(
            [
                "",
                "各 offset：",
                "",
                "| 尺寸 | Offset | ThorVG SSIM | ThorVG PSNR | Vello SSIM | Vello PSNR |",
                "| --- | --- | ---: | ---: | ---: | ---: |",
            ]
        )
        for scale in SCALES:
            for offset_x, offset_y in OFFSETS:
                thorvg = by_key[
                    (characteristic.key, scale.key, offset_x, offset_y, "thorvg")
                ]
                vello = by_key[
                    (characteristic.key, scale.key, offset_x, offset_y, "vello")
                ]
                lines.append(
                    f"| {scale.label} | `({_format_number(offset_x)}, {_format_number(offset_y)})` "
                    f"| {thorvg.metrics.ssim:.6f} | {_format_psnr(thorvg.metrics.psnr_db)} "
                    f"| {vello.metrics.ssim:.6f} | {_format_psnr(vello.metrics.psnr_db)} |"
                )

        for scale in SCALES:
            lines.extend(["", f"### {scale.label}"])
            roi = scaled_roi(characteristic.base_roi, scale)
            lines.extend(
                [
                    "",
                    f"特徵原始裁切 `{roi}`",
                    "",
                    *_image_table(
                        [
                            (
                                label,
                                visuals[
                                    ("crop", characteristic.key, scale.key, source_key)
                                ],
                            )
                            for source_key, label in display_sources
                        ],
                    ),
                ]
            )
            zoom = scaled_roi(characteristic.base_zoom_roi, scale)
            lines.extend(
                [
                    "",
                    f"固定像素放大 `{zoom}`",
                    "",
                    *_image_table(
                        [
                            (
                                label,
                                visuals[
                                    ("zoom", characteristic.key, scale.key, source_key)
                                ],
                            )
                            for source_key, label in display_sources
                        ],
                    ),
                ]
            )

    lines.extend(["", "## 完整場景"])
    for scale in SCALES:
        lines.extend(
            [
                "",
                f"### {scale.label}",
                "",
                *_image_table(
                    [
                        (label, visuals[("full", "", scale.key, source_key)])
                        for source_key, label in display_sources
                    ]
                ),
            ]
        )

    lines.extend(
        [
            "",
            "## 資料",
            "",
            "- [全部 192 筆依特徵分離的量測資料](diagnostic-results.tsv)",
            "- [機器可讀的測試矩陣與資料列](diagnostic-summary.json)",
        ]
    )
    return "\n".join(lines) + "\n"


_IMAGE_LINK = re.compile(r"!\[[^\]]*\]\(([^)]+)\)")


def validate_export(
    output_dir: pathlib.Path,
    markdown: str,
    rows: Sequence[MetricRow],
    visual_paths: Iterable[pathlib.Path],
) -> None:
    expected_rows = len(CHARACTERISTICS) * len(planned_keys()) * 2
    if len(rows) != expected_rows:
        raise DiagnosticError(
            f"export has {len(rows)} metric rows; expected {expected_rows}"
        )
    expected_links = len(SCALES) * 3 + len(CHARACTERISTICS) * len(SCALES) * 2 * 3
    links = _IMAGE_LINK.findall(markdown)
    if len(links) != expected_links:
        raise DiagnosticError(
            f"Markdown has {len(links)} image links; expected {expected_links}"
        )
    allowed = {path.as_posix() for path in visual_paths}
    for link in links:
        if link not in allowed:
            raise DiagnosticError(f"Markdown links an unexpected visual: {link}")
        target = (output_dir / link).resolve()
        try:
            target.relative_to(output_dir.resolve())
        except ValueError as error:
            raise DiagnosticError(f"Markdown visual escapes the report directory: {link}") from error
        if not target.is_file():
            raise DiagnosticError(f"Markdown visual is missing: {link}")
        try:
            report.decode_png(target)
        except (OSError, ValueError, report.ReportError) as error:
            raise DiagnosticError(f"Markdown visual is not a valid PNG: {link}: {error}") from error

    expected_unique = set(allowed)
    actual_unique = {
        path.relative_to(output_dir).as_posix()
        for path in (output_dir / "visuals").rglob("*.png")
    }
    if actual_unique != expected_unique:
        missing = sorted(expected_unique - actual_unique)
        unexpected = sorted(actual_unique - expected_unique)
        raise DiagnosticError(
            f"visual inventory mismatch: missing={missing[:1]}, unexpected={unexpected[:1]}"
        )


def _prepare_output(path: pathlib.Path) -> tuple[pathlib.Path, bool]:
    path = path.resolve()
    path.parent.mkdir(parents=True, exist_ok=True)
    output_was_empty = False
    if path.exists():
        if not path.is_dir():
            raise DiagnosticError(f"output path is not a directory: {path}")
        try:
            if next(path.iterdir(), None) is not None:
                raise DiagnosticError(f"output directory is not empty: {path}")
        except OSError as error:
            raise DiagnosticError(f"cannot inspect output directory {path}: {error}") from error
        output_was_empty = True
    staging = pathlib.Path(
        tempfile.mkdtemp(prefix=f".{path.name}-staging-", dir=path.parent)
    )
    return staging, output_was_empty


def run(args: argparse.Namespace) -> pathlib.Path:
    reference_manifest = pathlib.Path(args.reference_manifest).resolve()
    thorvg_manifest = pathlib.Path(args.thorvg_manifest).resolve()
    vello_manifest = pathlib.Path(args.vello_manifest).resolve()
    references = read_manifest(reference_manifest, path_column="reference_png")
    thorvg = read_manifest(
        thorvg_manifest,
        path_column="candidate_png",
        expected_mode=args.thorvg_mode,
    )
    vello = read_manifest(
        vello_manifest,
        path_column="candidate_png",
        expected_mode=args.vello_mode,
    )
    cache = ImageCache()
    validate_frames(
        {"SSAA8": references, "current ThorVG WebGPU": thorvg, "Vello": vello},
        cache,
    )
    rows = evaluate(
        references,
        (
            ("thorvg", args.thorvg_mode, thorvg),
            ("vello", args.vello_mode, vello),
        ),
        cache,
    )

    requested = pathlib.Path(args.output_dir).resolve()
    staging, output_was_empty = _prepare_output(requested)
    try:
        write_results_tsv(staging / "diagnostic-results.tsv", rows)
        write_summary_json(
            staging / "diagnostic-summary.json",
            rows,
            reference_manifest,
            {"thorvg": thorvg_manifest, "vello": vello_manifest},
        )
        visuals = write_visuals(
            staging,
            references,
            (
                ("thorvg", args.thorvg_mode, thorvg),
                ("vello", args.vello_mode, vello),
            ),
            cache,
        )
        markdown = render_markdown(rows, visuals, args.thorvg_mode, args.vello_mode)
        (staging / "comparison.md").write_text(markdown, encoding="utf-8")
        validate_export(staging, markdown, rows, visuals.values())

        if output_was_empty:
            try:
                requested.rmdir()
            except OSError as error:
                raise DiagnosticError(
                    f"output directory stopped being empty during export: {requested}: {error}"
                ) from error
        os.replace(staging, requested)
    except Exception:
        shutil.rmtree(staging, ignore_errors=True)
        raise
    return requested / "comparison.md"


def build_argument_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reference-manifest", required=True)
    parser.add_argument("--thorvg-manifest", required=True)
    parser.add_argument("--vello-manifest", required=True)
    parser.add_argument("--output-dir", required=True)
    parser.add_argument("--thorvg-mode", default="thorvg-webgpu-msaa4")
    parser.add_argument("--vello-mode", default="vello-area")
    return parser


def main(argv: Optional[Sequence[str]] = None) -> int:
    parser = build_argument_parser()
    args = parser.parse_args(argv)
    try:
        output = run(args)
    except (DiagnosticError, OSError, ValueError, report.ReportError) as error:
        print(f"evaluate_webgpu_diagnostic.py: error: {error}", file=sys.stderr)
        return 2
    print(f"Wrote {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
