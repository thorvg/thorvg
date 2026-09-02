#!/usr/bin/env python3

from __future__ import annotations

import csv
import pathlib
import tempfile
import unittest

import evaluate_webgpu_diagnostic as diagnostic
import generate_aa_report as report


def write_manifest(
    path: pathlib.Path,
    *,
    path_column: str,
    mode: str | None = None,
    omit_last: bool = False,
) -> None:
    fields = ["scale", "offset_x", "offset_y", path_column, "scene"]
    if mode is not None:
        fields.insert(0, "mode")
    keys = list(diagnostic.planned_keys())
    if omit_last:
        keys.pop()
    with path.open("w", encoding="utf-8", newline="") as output:
        writer = csv.DictWriter(output, fieldnames=fields, delimiter="\t")
        writer.writeheader()
        for index, key in enumerate(keys):
            row = {
                "scale": key.scale,
                "offset_x": key.offset_x,
                "offset_y": key.offset_y,
                path_column: f"frames/{index}.png",
                "scene": "comparison",
            }
            if mode is not None:
                row["mode"] = mode
            writer.writerow(row)


def fake_rows() -> tuple[diagnostic.MetricRow, ...]:
    values = []
    metrics = report.Metrics(0.999, 42.0, 0.1, 3.0)
    for characteristic in diagnostic.CHARACTERISTICS:
        for case in diagnostic.planned_keys():
            scale = diagnostic.SCALE_BY_KEY[case.scale]
            roi = diagnostic.scaled_roi(characteristic.base_roi, scale)
            for renderer, mode in (
                ("thorvg", "thorvg-webgpu-msaa4"),
                ("vello", "vello-area"),
            ):
                values.append(
                    diagnostic.MetricRow(
                        renderer=renderer,
                        mode=mode,
                        characteristic=characteristic,
                        case=case,
                        target_width=scale.width,
                        target_height=scale.height,
                        roi=roi,
                        candidate_png=f"{renderer}.png",
                        reference_png="ssaa8.png",
                        metrics=metrics,
                    )
                )
    return tuple(values)


def fake_visuals() -> dict[tuple[str, str, str, str], pathlib.Path]:
    values = {}
    sources = ("ssaa8", "thorvg", "vello")
    for scale in diagnostic.SCALES:
        for source in sources:
            values[("full", "", scale.key, source)] = (
                pathlib.Path("visuals") / "full" / scale.key / f"{source}.png"
            )
        for characteristic in diagnostic.CHARACTERISTICS:
            for source in sources:
                root = pathlib.Path("visuals") / characteristic.key / scale.key
                values[("crop", characteristic.key, scale.key, source)] = (
                    root / f"{source}-crop.png"
                )
                values[("zoom", characteristic.key, scale.key, source)] = (
                    root / f"{source}-zoom.png"
                )
    return values


class DiagnosticContractTests(unittest.TestCase):
    def test_matrix_and_scaled_rois(self) -> None:
        self.assertEqual(len(diagnostic.planned_keys()), 12)
        self.assertEqual(len(diagnostic.CHARACTERISTICS), 8)
        self.assertEqual(
            [item.key for item in diagnostic.CHARACTERISTICS],
            [item.key for item in report.CHARACTERISTICS],
        )
        self.assertEqual(
            [item.base_roi for item in diagnostic.CHARACTERISTICS],
            [item.search_roi for item in report.CHARACTERISTICS],
        )
        self.assertEqual(
            [scale.key for scale in diagnostic.SCALES],
            ["quarter", "half", "original"],
        )
        for scale in diagnostic.SCALES:
            for characteristic in diagnostic.CHARACTERISTICS:
                x0, y0, x1, y1 = diagnostic.scaled_roi(
                    characteristic.base_roi, scale
                )
                self.assertTrue(0 <= x0 < x1 <= scale.width)
                self.assertTrue(0 <= y0 < y1 <= scale.height)
                zoom = diagnostic.scaled_roi(characteristic.base_zoom_roi, scale)
                self.assertEqual(zoom[2] - zoom[0], zoom[3] - zoom[1])
                self.assertEqual(256 % (zoom[2] - zoom[0]), 0)

    def test_manifest_accepts_exact_matrix_and_extra_columns(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            reference = root / "reference.tsv"
            candidate = root / "candidate.tsv"
            write_manifest(reference, path_column="reference_png")
            write_manifest(
                candidate,
                path_column="candidate_png",
                mode="thorvg-webgpu-msaa4",
            )
            references = diagnostic.read_manifest(
                reference, path_column="reference_png"
            )
            candidates = diagnostic.read_manifest(
                candidate,
                path_column="candidate_png",
                expected_mode="thorvg-webgpu-msaa4",
            )
            self.assertEqual(set(references), set(diagnostic.planned_keys()))
            self.assertEqual(set(candidates), set(diagnostic.planned_keys()))
            self.assertTrue(all(frame.path.is_absolute() for frame in references.values()))

    def test_manifest_rejects_incomplete_matrix(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            manifest = pathlib.Path(temporary) / "reference.tsv"
            write_manifest(manifest, path_column="reference_png", omit_last=True)
            with self.assertRaisesRegex(diagnostic.DiagnosticError, "missing 1 case"):
                diagnostic.read_manifest(manifest, path_column="reference_png")

    def test_manifest_rejects_duplicate_case(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            manifest = pathlib.Path(temporary) / "reference.tsv"
            write_manifest(manifest, path_column="reference_png")
            rows = manifest.read_text(encoding="utf-8").splitlines()
            rows[-1] = rows[1]
            manifest.write_text("\n".join(rows) + "\n", encoding="utf-8")
            with self.assertRaisesRegex(diagnostic.DiagnosticError, "duplicate case"):
                diagnostic.read_manifest(manifest, path_column="reference_png")

    def test_manifest_rejects_wrong_named_scene(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            manifest = pathlib.Path(temporary) / "reference.tsv"
            write_manifest(manifest, path_column="reference_png")
            document = manifest.read_text(encoding="utf-8")
            manifest.write_text(
                document.replace("comparison", "flat-core", 1), encoding="utf-8"
            )
            with self.assertRaisesRegex(diagnostic.DiagnosticError, "expected 'comparison'"):
                diagnostic.read_manifest(manifest, path_column="reference_png")

    def test_markdown_is_feature_first_and_complete(self) -> None:
        rows = fake_rows()
        visuals = fake_visuals()
        markdown = diagnostic.render_markdown(
            rows, visuals, "thorvg-webgpu-msaa4", "vello-area"
        )
        self.assertEqual(
            len(diagnostic._IMAGE_LINK.findall(markdown)),
            len(diagnostic.SCALES) * 3
            + len(diagnostic.CHARACTERISTICS) * len(diagnostic.SCALES) * 2 * 3,
        )
        for characteristic in diagnostic.CHARACTERISTICS:
            self.assertIn(f"## {characteristic.label}", markdown)
        self.assertIn("全部 192 筆依特徵分離的量測資料", markdown)
        self.assertNotIn("heatmap", markdown.lower())
        self.assertNotIn("difference map", markdown.lower())
        self.assertNotIn("worst", markdown.lower())

    def test_export_validator_checks_every_link_and_png(self) -> None:
        rows = fake_rows()
        visuals = fake_visuals()
        markdown = diagnostic.render_markdown(
            rows, visuals, "thorvg-webgpu-msaa4", "vello-area"
        )
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            for relative in set(visuals.values()):
                destination = root / relative
                destination.parent.mkdir(parents=True, exist_ok=True)
                report.encode_png_rgb(destination, 1, 1, bytes((255, 255, 255)))
            diagnostic.validate_export(root, markdown, rows, visuals.values())
            missing = root / next(iter(visuals.values()))
            missing.unlink()
            with self.assertRaisesRegex(diagnostic.DiagnosticError, "missing"):
                diagnostic.validate_export(root, markdown, rows, visuals.values())


if __name__ == "__main__":
    unittest.main()
