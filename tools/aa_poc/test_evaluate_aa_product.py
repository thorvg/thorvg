#!/usr/bin/env python3
"""Focused tests for the product AA quality gate."""

import contextlib
import csv
import io
import json
import math
import pathlib
import struct
import sys
import tempfile
import unittest
import zlib


sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import evaluate_aa_product as gate  # noqa: E402
import generate_aa_report as report  # noqa: E402


def write_rgb_png(path, width, height, color):
    path.parent.mkdir(parents=True, exist_ok=True)
    report.encode_png_rgb(path, width, height, bytes(color) * (width * height))


def write_rgba_png(path, width, height, color):
    path.parent.mkdir(parents=True, exist_ok=True)
    scanlines = bytearray()
    row = bytes(color) * width
    for _ in range(height):
        scanlines.append(0)
        scanlines.extend(row)
    ihdr = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    path.write_bytes(
        report.PNG_SIGNATURE
        + report._png_chunk(b"IHDR", ihdr)
        + report._png_chunk(b"IDAT", zlib.compress(bytes(scanlines)))
        + report._png_chunk(b"IEND", b"")
    )


def manifest_row(
    mode,
    candidate="images/candidate.png",
    reference="images/reference.png",
    *,
    scene="curves",
    scale="component",
    offset_x="0",
    offset_y="0",
    root_samples=None,
    selected_mode=None,
    route_valid="1",
    expected_routes="2",
    actual_routes="2",
    total_routes=None,
    fallbacks="0",
):
    if root_samples is None:
        root_samples = "4" if mode == "msaa4" else "1"
    if selected_mode is None:
        selected_mode = mode
    if total_routes is None:
        total_routes = expected_routes
    return {
        "mode": mode,
        "scene": scene,
        "scale": scale,
        "offset_x": offset_x,
        "offset_y": offset_y,
        "candidate_png": candidate,
        "reference_png": reference,
        "selected_mode": selected_mode,
        "root_samples": root_samples,
        "route_valid": route_valid,
        "expected_route_count": expected_routes,
        "actual_route_count": actual_routes,
        "total_route_count": total_routes,
        "fallback_count": fallbacks,
    }


def write_manifest(path, rows, columns=gate.MANIFEST_COLUMNS):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as output:
        writer = csv.DictWriter(output, fieldnames=columns, delimiter="\t")
        writer.writeheader()
        writer.writerows(rows)


def run_main(arguments):
    stdout = io.StringIO()
    stderr = io.StringIO()
    with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
        status = gate.main(arguments)
    return status, stdout.getvalue(), stderr.getvalue()


class ManifestTests(unittest.TestCase):
    def test_image_paths_are_relative_to_manifest_not_current_directory(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            run_dir = root / "nested" / "run"
            write_rgb_png(run_dir / "images" / "candidate.png", 11, 11, (17, 31, 63))
            write_rgb_png(run_dir / "images" / "reference.png", 11, 11, (17, 31, 63))
            manifest = run_dir / "quality-manifest.tsv"
            write_manifest(manifest, [manifest_row("flat-direct")])

            cases = gate.read_manifest(manifest)
            self.assertEqual(cases[0].candidate_path, (run_dir / "images" / "candidate.png").resolve())
            self.assertEqual(cases[0].reference_path, (run_dir / "images" / "reference.png").resolve())
            evaluation = gate.evaluate_manifest(manifest, gate.Thresholds())

        self.assertTrue(evaluation.passed)
        self.assertTrue(math.isinf(evaluation.rows[0].metrics.psnr_db))

    def test_missing_required_column_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            manifest = pathlib.Path(temporary) / "quality-manifest.tsv"
            columns = tuple(
                column for column in gate.MANIFEST_COLUMNS if column != "fallback_count"
            )
            write_manifest(manifest, [], columns=columns)
            with self.assertRaisesRegex(gate.QualityGateError, "fallback_count"):
                gate.read_manifest(manifest)

    def test_duplicate_cases_and_incomplete_full_suite_are_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            manifest = pathlib.Path(temporary) / "quality-manifest.tsv"
            row = manifest_row("flat-direct")
            write_manifest(manifest, [row, dict(row)])
            with self.assertRaisesRegex(gate.QualityGateError, "duplicate case"):
                gate.evaluate_manifest(manifest, gate.Thresholds())

            write_manifest(manifest, [row])
            with self.assertRaisesRegex(gate.QualityGateError, "full-suite matrix mismatch"):
                gate.evaluate_manifest(
                    manifest, gate.Thresholds(), require_full_suite=True
                )


class InvariantGateTests(unittest.TestCase):
    def test_runner_route_proof_fields_are_required_by_the_gate(self):
        with tempfile.TemporaryDirectory() as temporary:
            manifest = pathlib.Path(temporary) / "quality-manifest.tsv"
            write_manifest(
                manifest,
                [
                    manifest_row(
                        "hybrid",
                        selected_mode="curve-direct",
                        route_valid="0",
                        total_routes="3",
                    )
                ],
            )
            evaluation = gate.evaluate_manifest(manifest, gate.Thresholds())

        messages = [failure.message for failure in evaluation.failures]
        self.assertTrue(any("route_valid is 0" in message for message in messages))
        self.assertTrue(any("selected_mode is curve-direct" in message for message in messages))
        self.assertTrue(any("total_route_count is 3" in message for message in messages))
    def test_invariants_fail_before_any_png_is_decoded(self):
        with tempfile.TemporaryDirectory() as temporary:
            run_dir = pathlib.Path(temporary)
            manifest = run_dir / "quality-manifest.tsv"
            write_manifest(
                manifest,
                [
                    manifest_row(
                        "msaa4",
                        root_samples="1",
                        expected_routes="2",
                        actual_routes="3",
                        fallbacks="2",
                    ),
                    manifest_row(
                        "noaa",
                        candidate="missing/noaa.png",
                        reference="missing/reference.png",
                        offset_x="0.5",
                        fallbacks="1",
                    ),
                ],
            )

            status, stdout, stderr = run_main([str(manifest)])
            summary = json.loads((run_dir / "quality-summary.json").read_text())
            with (run_dir / "quality-results.tsv").open(newline="") as source:
                results = list(csv.DictReader(source, delimiter="\t"))

        self.assertEqual(status, 1)
        self.assertEqual(summary["evaluation_stage"], "invariant-gate")
        self.assertEqual(summary["counts"]["invariant_failures"], 4)
        self.assertEqual(summary["counts"]["row_visual_failures"], 0)
        self.assertFalse(any(row["ssim"] for row in results))
        self.assertTrue(all(row["visual_status"] == "not-run" for row in results))
        self.assertIn("Quality gate failures:", stderr)
        self.assertIn("fallback_count is 1", stderr)
        self.assertIn("root_samples is 1; expected 4 for msaa4", stderr)
        self.assertIn("Visual evaluation was not run", stderr)
        self.assertNotIn("cannot read PNG", stderr)
        self.assertIn("defaults: every gated row SSIM", stdout)

    def test_continue_valid_rows_evaluates_only_valid_rows_and_keeps_mode_invalid(self):
        with tempfile.TemporaryDirectory() as temporary:
            run_dir = pathlib.Path(temporary)
            write_rgb_png(run_dir / "images" / "valid.png", 11, 11, (17, 31, 63))
            write_rgb_png(run_dir / "images" / "reference.png", 11, 11, (17, 31, 63))
            manifest = run_dir / "quality-manifest.tsv"
            write_manifest(
                manifest,
                [
                    manifest_row(
                        "flat-direct",
                        candidate="missing/invalid.png",
                        reference="missing/reference.png",
                        actual_routes="1",
                    ),
                    manifest_row(
                        "flat-direct",
                        candidate="images/valid.png",
                        reference="images/reference.png",
                        offset_x="0.5",
                    ),
                    manifest_row(
                        "curve-direct",
                        candidate="images/valid.png",
                        reference="images/reference.png",
                        offset_x="0.75",
                    ),
                ],
            )

            status, _, stderr = run_main(
                [str(manifest), "--continue-valid-rows"]
            )
            summary = json.loads((run_dir / "quality-summary.json").read_text())
            with (run_dir / "quality-results.tsv").open(newline="") as source:
                results = list(csv.DictReader(source, delimiter="\t"))
            aggregates = {
                item["mode"]: item for item in summary["aggregates"]["by_mode"]
            }

        self.assertEqual(status, 1)
        self.assertEqual(summary["evaluation_stage"], "partial-visual")
        self.assertEqual(summary["visual_evaluation"]["status"], "partial")
        self.assertEqual(summary["visual_evaluation"]["evaluated_rows"], 2)
        self.assertEqual(summary["visual_evaluation"]["not_run_rows"], 1)
        self.assertEqual(results[0]["visual_status"], "not-run")
        self.assertEqual(results[0]["included_in_visual_aggregates"], "no")
        self.assertFalse(results[0]["ssim"])
        self.assertEqual(results[1]["included_in_visual_aggregates"], "yes")
        self.assertTrue(results[1]["ssim"])
        self.assertTrue(
            all(row["evaluation_stage"] == "partial-visual" for row in results)
        )
        self.assertEqual(aggregates["flat-direct"]["manifest_row_count"], 2)
        self.assertEqual(aggregates["flat-direct"]["row_count"], 1)
        self.assertEqual(
            aggregates["flat-direct"]["invariant_invalid_row_count"], 1
        )
        self.assertEqual(aggregates["flat-direct"]["gate_status"], "invalid")
        self.assertEqual(aggregates["flat-direct"]["metrics"]["ssim"], 1.0)
        self.assertEqual(aggregates["curve-direct"]["gate_status"], "pass")
        self.assertIn("Their modes remain ineligible", stderr)
        self.assertNotIn("cannot read PNG", stderr)

    def test_continue_valid_rows_reports_invalid_only_mode_without_aggregate_metrics(self):
        with tempfile.TemporaryDirectory() as temporary:
            run_dir = pathlib.Path(temporary)
            write_rgb_png(run_dir / "images" / "valid.png", 11, 11, (9, 19, 29))
            manifest = run_dir / "quality-manifest.tsv"
            write_manifest(
                manifest,
                [
                    manifest_row(
                        "flat-mask",
                        candidate="missing/invalid.png",
                        reference="missing/reference.png",
                        fallbacks="1",
                    ),
                    manifest_row(
                        "hybrid",
                        candidate="images/valid.png",
                        reference="images/valid.png",
                        offset_x="0.5",
                    ),
                ],
            )

            evaluation = gate.evaluate_manifest(
                manifest,
                gate.Thresholds(),
                continue_valid_rows=True,
            )
            aggregates = {item.mode: item for item in evaluation.by_mode}

        self.assertFalse(evaluation.passed)
        self.assertIsNone(aggregates["flat-mask"].metrics)
        self.assertEqual(aggregates["flat-mask"].row_count, 0)
        self.assertEqual(aggregates["flat-mask"].gate_status, "invalid")
        self.assertEqual(aggregates["hybrid"].gate_status, "pass")


class VisualGateTests(unittest.TestCase):
    def test_noaa_is_reported_but_exempt_while_gated_mode_fails(self):
        with tempfile.TemporaryDirectory() as temporary:
            run_dir = pathlib.Path(temporary)
            write_rgb_png(run_dir / "images" / "black.png", 11, 11, (0, 0, 0))
            write_rgb_png(run_dir / "images" / "white.png", 11, 11, (255, 255, 255))
            manifest = run_dir / "quality-manifest.tsv"
            write_manifest(
                manifest,
                [
                    manifest_row("flat-direct", candidate="images/black.png", reference="images/white.png"),
                    manifest_row(
                        "noaa",
                        candidate="images/black.png",
                        reference="images/white.png",
                        offset_x="0.5",
                    ),
                ],
            )

            status, _, stderr = run_main([str(manifest)])
            summary = json.loads((run_dir / "quality-summary.json").read_text())
            failures = summary["failures"]
            aggregates = {
                item["mode"]: item for item in summary["aggregates"]["by_mode"]
            }

        self.assertEqual(status, 1)
        self.assertEqual(summary["evaluation_stage"], "complete")
        self.assertEqual(summary["counts"]["row_visual_failures"], 1)
        self.assertEqual(summary["counts"]["mode_visual_failures"], 1)
        self.assertEqual({failure["mode"] for failure in failures}, {"flat-direct"})
        self.assertEqual(aggregates["flat-direct"]["gate_status"], "fail")
        self.assertEqual(aggregates["noaa"]["gate_status"], "diagnostic")
        self.assertAlmostEqual(aggregates["noaa"]["metrics"]["psnr_db"], 0.0)
        self.assertIn("row-visual", stderr)
        self.assertIn("mode-visual", stderr)

    def test_threshold_overrides_are_explicit_and_do_not_change_defaults(self):
        with tempfile.TemporaryDirectory() as temporary:
            run_dir = pathlib.Path(temporary)
            write_rgb_png(run_dir / "images" / "black.png", 11, 11, (0, 0, 0))
            write_rgb_png(run_dir / "images" / "white.png", 11, 11, (255, 255, 255))
            manifest = run_dir / "quality-manifest.tsv"
            write_manifest(
                manifest,
                [manifest_row("hybrid", candidate="images/black.png", reference="images/white.png")],
            )

            status, stdout, stderr = run_main(
                [
                    str(manifest),
                    "--row-min-ssim",
                    "0",
                    "--row-min-psnr",
                    "0",
                    "--mode-min-ssim",
                    "0",
                    "--mode-min-psnr",
                    "0",
                ]
            )
            summary = json.loads((run_dir / "quality-summary.json").read_text())

        self.assertEqual(status, 0, stderr)
        self.assertEqual(summary["status"], "pass")
        self.assertEqual(summary["thresholds"]["defaults"]["row_min_ssim"], 0.98)
        self.assertEqual(summary["thresholds"]["active"]["row_min_ssim"], 0.0)
        self.assertIn("defaults: every gated row SSIM >= 0.980", stdout)
        self.assertIn("active:   every gated row SSIM >= 0", stdout)
        self.assertIn("anchored below the existing MSAA4 baseline", stdout)

    def test_aggregation_matches_report_metric_combination_at_each_level(self):
        with tempfile.TemporaryDirectory() as temporary:
            run_dir = pathlib.Path(temporary)
            write_rgb_png(run_dir / "images" / "black.png", 11, 11, (0, 0, 0))
            write_rgb_png(run_dir / "images" / "gray.png", 11, 11, (32, 32, 32))
            write_rgb_png(run_dir / "images" / "white.png", 11, 11, (255, 255, 255))
            manifest = run_dir / "quality-manifest.tsv"
            write_manifest(
                manifest,
                [
                    manifest_row("curve-mask", candidate="images/black.png", reference="images/white.png"),
                    manifest_row(
                        "curve-mask",
                        candidate="images/gray.png",
                        reference="images/white.png",
                        offset_x="0.5",
                    ),
                ],
            )
            thresholds = gate.Thresholds(0.0, 0.0, 0.0, 0.0)
            evaluation = gate.evaluate_manifest(manifest, thresholds)
            row_metrics = [row.metrics for row in evaluation.rows]
            expected = report.combine_metrics(row_metrics)

        self.assertEqual(len(evaluation.by_mode), 1)
        self.assertEqual(len(evaluation.by_mode_scene), 1)
        self.assertEqual(len(evaluation.by_mode_scene_scale), 1)
        self.assertAlmostEqual(evaluation.by_mode[0].metrics.ssim, expected.ssim)
        self.assertAlmostEqual(evaluation.by_mode[0].metrics.psnr_db, expected.psnr_db)
        self.assertAlmostEqual(
            evaluation.by_mode_scene_scale[0].metrics.mean_absolute_error,
            expected.mean_absolute_error,
        )
        self.assertEqual(
            evaluation.by_mode_scene_scale[0].metrics.max_absolute_error,
            expected.max_absolute_error,
        )

    def test_one_failed_required_row_makes_the_mode_ineligible(self):
        with tempfile.TemporaryDirectory() as temporary:
            run_dir = pathlib.Path(temporary)
            write_rgb_png(run_dir / "images" / "black.png", 11, 11, (0, 0, 0))
            write_rgb_png(run_dir / "images" / "near-black.png", 11, 11, (2, 2, 2))
            manifest = run_dir / "quality-manifest.tsv"
            write_manifest(
                manifest,
                [
                    manifest_row(
                        "curve-direct",
                        candidate="images/black.png",
                        reference="images/near-black.png",
                    ),
                    manifest_row(
                        "curve-direct",
                        candidate="images/black.png",
                        reference="images/black.png",
                        offset_x="0.5",
                    ),
                ],
            )
            evaluation = gate.evaluate_manifest(
                manifest,
                gate.Thresholds(
                    row_min_ssim=1.0,
                    row_min_psnr_db=100.0,
                    mode_min_ssim=0.0,
                    mode_min_psnr_db=0.0,
                ),
            )

        self.assertEqual(evaluation.by_mode[0].gate_status, "fail")
        self.assertIn("required row(s) failed", evaluation.by_mode[0].failure)

    def test_alpha_compositing_uses_straight_alpha_white_matte(self):
        with tempfile.TemporaryDirectory() as temporary:
            run_dir = pathlib.Path(temporary)
            write_rgba_png(run_dir / "images" / "half-red.png", 11, 11, (255, 0, 0, 128))
            write_rgb_png(run_dir / "images" / "matte.png", 11, 11, (255, 127, 127))
            manifest = run_dir / "quality-manifest.tsv"
            write_manifest(
                manifest,
                [
                    manifest_row(
                        "flat-mask",
                        candidate="images/half-red.png",
                        reference="images/matte.png",
                    )
                ],
            )
            evaluation = gate.evaluate_manifest(manifest, gate.Thresholds())

        metrics = evaluation.rows[0].metrics
        self.assertEqual(metrics.ssim, 1.0)
        self.assertTrue(math.isinf(metrics.psnr_db))
        self.assertTrue(evaluation.passed)


if __name__ == "__main__":
    unittest.main()
