#!/usr/bin/env python3
"""Focused tests for the external-renderer product AA quality gate."""

import contextlib
import csv
import io
import json
import pathlib
import sys
import tempfile
import unittest


sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import evaluate_aa_product as product  # noqa: E402
import evaluate_external_aa_product as external  # noqa: E402
import generate_aa_report as report  # noqa: E402


def write_manifest(path, columns, rows):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as output:
        writer = csv.DictWriter(output, fieldnames=columns, delimiter="\t")
        writer.writeheader()
        writer.writerows(rows)


def planned_rows():
    for scene in product.FULL_SCENES:
        for scale in product.FULL_SCALES:
            for offset_x, offset_y in product.FULL_OFFSETS:
                yield scene, scale, offset_x, offset_y


def external_rows(candidate="images/candidate.png"):
    return [
        {
            "mode": "vello-area",
            "scene": scene,
            "scale": scale,
            "offset_x": offset_x,
            "offset_y": offset_y,
            "candidate_png": candidate,
        }
        for scene, scale, offset_x, offset_y in planned_rows()
    ]


def reference_rows(*, fallback_count="0"):
    return [
        {
            "mode": "noaa",
            "scene": scene,
            "scale": scale,
            "offset_x": offset_x,
            "offset_y": offset_y,
            "candidate_png": "images/noaa.png",
            "reference_png": "images/ssaa8.png",
            "selected_mode": "noaa",
            "root_samples": "1",
            "route_valid": "1",
            "expected_route_count": "1",
            "actual_route_count": "1",
            "total_route_count": "1",
            "fallback_count": fallback_count,
        }
        for scene, scale, offset_x, offset_y in planned_rows()
    ]


def run_main(arguments):
    stdout = io.StringIO()
    stderr = io.StringIO()
    with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
        status = external.main(arguments)
    return status, stdout.getvalue(), stderr.getvalue()


class ExternalManifestTests(unittest.TestCase):
    def test_full_matrix_resolves_candidates_relative_to_manifest(self):
        with tempfile.TemporaryDirectory() as temporary:
            run_dir = pathlib.Path(temporary) / "nested" / "run"
            manifest = run_dir / "vello-quality-manifest.tsv"
            write_manifest(manifest, external.EXTERNAL_COLUMNS, external_rows())

            cases = external.read_external_manifest(manifest, "vello-area")

        self.assertEqual(len(cases), 48)
        self.assertEqual(
            cases[0].candidate_path,
            (run_dir / "images" / "candidate.png").resolve(),
        )

    def test_incomplete_matrix_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            manifest = pathlib.Path(temporary) / "vello-quality-manifest.tsv"
            write_manifest(
                manifest,
                external.EXTERNAL_COLUMNS,
                external_rows()[:-1],
            )
            with self.assertRaisesRegex(external.ExternalQualityError, "matrix mismatch"):
                external.read_external_manifest(manifest, "vello-area")

    def test_gl_proof_and_reference_columns_are_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            manifest = pathlib.Path(temporary) / "vello-quality-manifest.tsv"
            columns = external.EXTERNAL_COLUMNS + ("reference_png", "root_samples")
            rows = external_rows()
            for row in rows:
                row["reference_png"] = "untrusted.png"
                row["root_samples"] = "1"
            write_manifest(manifest, columns, rows)
            with self.assertRaisesRegex(
                external.ExternalQualityError, "must not contain ThorVG GL proof"
            ):
                external.read_external_manifest(manifest, "vello-area")


class ReferenceManifestTests(unittest.TestCase):
    def test_reference_route_proof_is_required_before_image_decode(self):
        with tempfile.TemporaryDirectory() as temporary:
            reference_manifest = pathlib.Path(temporary) / "quality-manifest.tsv"
            write_manifest(
                reference_manifest,
                product.MANIFEST_COLUMNS,
                reference_rows(fallback_count="1"),
            )
            with self.assertRaisesRegex(external.ExternalQualityError, "route proof"):
                external.read_reference_map(reference_manifest)


class EndToEndTests(unittest.TestCase):
    def test_identical_full_matrix_passes_without_claiming_gl_invariants(self):
        with tempfile.TemporaryDirectory() as temporary:
            run_dir = pathlib.Path(temporary)
            images = run_dir / "images"
            images.mkdir()
            report.encode_png_rgb(
                images / "candidate.png", 11, 11, bytes((17, 31, 63)) * 121
            )
            report.encode_png_rgb(
                images / "ssaa8.png", 11, 11, bytes((17, 31, 63)) * 121
            )
            manifest = run_dir / "vello-quality-manifest.tsv"
            reference_manifest = run_dir / "quality-manifest.tsv"
            output_dir = run_dir / "evaluation"
            write_manifest(manifest, external.EXTERNAL_COLUMNS, external_rows())
            write_manifest(
                reference_manifest, product.MANIFEST_COLUMNS, reference_rows()
            )

            status, stdout, stderr = run_main(
                [
                    str(manifest),
                    "--external-mode",
                    "vello-area",
                    "--reference-manifest",
                    str(reference_manifest),
                    "--output-dir",
                    str(output_dir),
                ]
            )
            summary = json.loads((output_dir / "quality-summary.json").read_text())

        self.assertEqual(status, 0, stderr)
        self.assertIn("Quality gate: PASS", stdout)
        self.assertEqual(summary["counts"]["rows"], 48)
        self.assertEqual(summary["counts"]["failures"], 0)
        self.assertEqual(summary["route_and_sample_invariants"], "not-applicable to external renderer")
        self.assertEqual(summary["aggregates"]["by_mode"][0]["metrics"]["ssim"], 1.0)

    def test_thorvg_mode_name_cannot_be_recast_as_external(self):
        status, _, stderr = run_main(
            [
                "missing.tsv",
                "--external-mode",
                "msaa4",
                "--reference-manifest",
                "missing-reference.tsv",
            ]
        )
        self.assertEqual(status, 1)
        self.assertIn("must not name a ThorVG GL mode", stderr)


if __name__ == "__main__":
    unittest.main()
