#!/usr/bin/env python3
"""Unit tests for the dependency-free AA comparison report generator."""

import contextlib
import html.parser
import io
import json
import math
import pathlib
import struct
import sys
import tempfile
import types
import unittest
import zlib
from unittest import mock


sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import generate_aa_report as report  # noqa: E402


def png_chunk(chunk_type, payload):
    crc = zlib.crc32(chunk_type)
    crc = zlib.crc32(payload, crc) & 0xFFFFFFFF
    return struct.pack(">I", len(payload)) + chunk_type + payload + struct.pack(">I", crc)


def filtered_rgb_png(width, rows, filters):
    """Create an RGB PNG whose rows exercise explicitly selected filters."""

    raw = bytearray()
    previous = bytes(width * 3)
    for row, filter_type in zip(rows, filters):
        raw.append(filter_type)
        encoded_row = bytearray(len(row))
        for index, value in enumerate(row):
            left = row[index - 3] if index >= 3 else 0
            up = previous[index]
            upper_left = previous[index - 3] if index >= 3 else 0
            if filter_type == 0:
                predictor = 0
            elif filter_type == 1:
                predictor = left
            elif filter_type == 2:
                predictor = up
            elif filter_type == 3:
                predictor = (left + up) // 2
            else:
                predictor = report._paeth(left, up, upper_left)
            encoded_row[index] = (value - predictor) & 0xFF
        raw.extend(encoded_row)
        previous = row
    ihdr = struct.pack(">IIBBBBB", width, len(rows), 8, 2, 0, 0, 0)
    return (
        report.PNG_SIGNATURE
        + png_chunk(b"IHDR", ihdr)
        + png_chunk(b"IDAT", zlib.compress(bytes(raw)))
        + png_chunk(b"IEND", b"")
    )


def solid_image(width, height, rgba):
    return report.PngImage(width, height, bytes(rgba) * (width * height))


class LocalLinkParser(html.parser.HTMLParser):
    def __init__(self):
        super().__init__()
        self.links = []

    def handle_starttag(self, tag, attributes):
        values = dict(attributes)
        if tag == "a" and "href" in values:
            self.links.append(values["href"])
        if tag == "img" and "src" in values:
            self.links.append(values["src"])


class PngTests(unittest.TestCase):
    def test_decodes_actual_lodepng_shape_and_all_filters(self):
        width = 3
        rows = [
            bytes(((17 * y + 23 * x + channel * 41) & 0xFF) for x in range(width) for channel in range(3))
            for y in range(5)
        ]
        encoded = filtered_rgb_png(width, rows, [0, 1, 2, 3, 4])
        with tempfile.TemporaryDirectory() as temporary:
            path = pathlib.Path(temporary) / "filtered.png"
            path.write_bytes(encoded)
            image = report.decode_png(path)
        self.assertEqual((image.width, image.height), (3, 5))
        expected = bytearray()
        for row in rows:
            for pixel in range(width):
                expected.extend(row[pixel * 3 : pixel * 3 + 3])
                expected.append(255)
        self.assertEqual(image.rgba, bytes(expected))

    def test_rgb_encoder_round_trip(self):
        rgb = bytes((0, 1, 2, 63, 127, 255, 250, 240, 230, 9, 8, 7))
        with tempfile.TemporaryDirectory() as temporary:
            path = pathlib.Path(temporary) / "round-trip.png"
            report.encode_png_rgb(path, 2, 2, rgb)
            image = report.decode_png(path)
        self.assertEqual((image.width, image.height), (2, 2))
        self.assertEqual(report.composite_over_white(image), rgb)

    def test_decodes_palette_transparency(self):
        ihdr = struct.pack(">IIBBBBB", 2, 1, 8, 3, 0, 0, 0)
        encoded = (
            report.PNG_SIGNATURE
            + png_chunk(b"IHDR", ihdr)
            + png_chunk(b"PLTE", bytes((255, 0, 0, 0, 128, 255)))
            + png_chunk(b"tRNS", bytes((255, 64)))
            + png_chunk(b"IDAT", zlib.compress(bytes((0, 0, 1))))
            + png_chunk(b"IEND", b"")
        )
        with tempfile.TemporaryDirectory() as temporary:
            path = pathlib.Path(temporary) / "palette.png"
            path.write_bytes(encoded)
            image = report.decode_png(path)
        self.assertEqual(image.rgba, bytes((255, 0, 0, 255, 0, 128, 255, 64)))

    def test_rejects_bad_crc(self):
        encoded = bytearray(filtered_rgb_png(1, [bytes((1, 2, 3))], [0]))
        encoded[-1] ^= 1
        with tempfile.TemporaryDirectory() as temporary:
            path = pathlib.Path(temporary) / "bad.png"
            path.write_bytes(encoded)
            with self.assertRaisesRegex(report.ReportError, "CRC mismatch"):
                report.decode_png(path)


class MetricTests(unittest.TestCase):
    def test_identical_images_have_perfect_metrics(self):
        image = solid_image(13, 12, (31, 127, 241, 255))
        metrics = report.compute_metrics(image, image)
        self.assertEqual(metrics.ssim, 1.0)
        self.assertTrue(math.isinf(metrics.psnr_db))
        self.assertEqual(metrics.mean_absolute_error, 0.0)
        self.assertEqual(metrics.max_absolute_error, 0.0)

    def test_black_versus_white_has_known_psnr_and_ssim(self):
        black = solid_image(11, 11, (0, 0, 0, 255))
        white = solid_image(11, 11, (255, 255, 255, 255))
        metrics = report.compute_metrics(black, white)
        expected_ssim = (0.01**2) / (1.0 + 0.01**2)
        self.assertAlmostEqual(metrics.psnr_db, 0.0, places=12)
        self.assertAlmostEqual(metrics.ssim, expected_ssim, places=12)
        self.assertEqual(metrics.mean_absolute_error, 255.0)
        self.assertEqual(metrics.max_absolute_error, 255.0)

    def test_alpha_is_composited_over_white(self):
        transparent_red = solid_image(3, 2, (255, 0, 0, 0))
        opaque_white = solid_image(3, 2, (255, 255, 255, 255))
        metrics = report.compute_metrics(transparent_red, opaque_white)
        self.assertEqual(metrics.ssim, 1.0)
        self.assertTrue(math.isinf(metrics.psnr_db))

    def test_shared_roi_uses_union_of_every_method(self):
        white = bytearray((255, 255, 255, 255) * 20)
        left = bytearray(white)
        right = bytearray(white)
        left[(1 * 5 + 1) * 4 : (1 * 5 + 1) * 4 + 3] = b"\x00\x00\x00"
        right[(2 * 5 + 3) * 4 : (2 * 5 + 3) * 4 + 3] = b"\x00\x00\x00"
        images = (
            report.PngImage(5, 4, bytes(left)),
            report.PngImage(5, 4, bytes(right)),
        )
        self.assertEqual(report.common_content_roi(images, padding=0), (1, 1, 4, 3))
        self.assertEqual(report.common_content_roi(images, padding=1), (0, 0, 5, 4))

    def test_dimension_mismatch_fails_instead_of_aligning_implicitly(self):
        with self.assertRaisesRegex(report.ReportError, "different dimensions"):
            report.compute_metrics(
                solid_image(2, 2, (0, 0, 0, 255)),
                solid_image(3, 2, (0, 0, 0, 255)),
            )

    def test_roi_ignores_differences_outside_its_declared_scope(self):
        first = bytearray((255, 255, 255, 255) * 9)
        second = bytearray(first)
        second[0:3] = b"\x00\x00\x00"
        metrics = report.compute_metrics(
            report.PngImage(3, 3, bytes(first)),
            report.PngImage(3, 3, bytes(second)),
            roi=(1, 1, 3, 3),
        )
        self.assertEqual(metrics.ssim, 1.0)
        self.assertTrue(math.isinf(metrics.psnr_db))

    def test_combined_metrics_average_error_before_converting_to_psnr(self):
        first = report.Metrics(0.8, 20.0, 2.0, 7.0)
        second = report.Metrics(1.0, 30.0, 4.0, 5.0)
        combined = report.combine_metrics((first, second))
        first_mse = 255.0**2 / (10.0 ** (first.psnr_db / 10.0))
        second_mse = 255.0**2 / (10.0 ** (second.psnr_db / 10.0))
        expected_psnr = 10.0 * math.log10(
            255.0**2 / ((first_mse + second_mse) / 2.0)
        )
        self.assertAlmostEqual(combined.ssim, 0.9)
        self.assertAlmostEqual(combined.psnr_db, expected_psnr)
        self.assertNotAlmostEqual(combined.psnr_db, 25.0)
        self.assertEqual(combined.mean_absolute_error, 3.0)
        self.assertEqual(combined.max_absolute_error, 7.0)

    def test_content_roi_within_search_uses_all_images_and_full_image_padding(self):
        white = bytearray((255, 255, 255, 255) * (20 * 12))
        left = bytearray(white)
        right = bytearray(white)
        left[(5 * 20 + 5) * 4 : (5 * 20 + 5) * 4 + 3] = b"\x00\x00\x00"
        right[(7 * 20 + 9) * 4 : (7 * 20 + 9) * 4 + 3] = b"\x00\x00\x00"
        images = (
            report.PngImage(20, 12, bytes(left)),
            report.PngImage(20, 12, bytes(right)),
        )
        self.assertEqual(
            report.content_roi_within(images, (4, 4, 10, 9), padding=2),
            (3, 3, 12, 10),
        )

    def test_crop_image_uses_half_open_coordinates(self):
        rgba = bytes(
            component
            for y in range(3)
            for x in range(4)
            for component in (x, y, x + y, 255)
        )
        cropped = report.crop_image(report.PngImage(4, 3, rgba), (1, 1, 4, 3))
        self.assertEqual((cropped.width, cropped.height), (3, 2))
        self.assertEqual(cropped.rgba[:4], bytes((1, 1, 2, 255)))
        self.assertEqual(cropped.rgba[-4:], bytes((3, 2, 5, 255)))


class OutputTests(unittest.TestCase):
    def test_difference_map_is_black_for_exact_match(self):
        image = solid_image(2, 2, (10, 20, 30, 255))
        with tempfile.TemporaryDirectory() as temporary:
            path = pathlib.Path(temporary) / "diff.png"
            report.write_difference_map(path, image, image, gain=8.0)
            difference = report.decode_png(path)
        self.assertEqual(report.composite_over_white(difference), bytes(2 * 2 * 3))

    def test_overview_links_summary_and_the_eight_characteristics(self):
        overview = report.render_report_html("AA report", "now")

        self.assertEqual(overview.count('class="characteristic-link"'), 8)
        self.assertEqual(overview.count('class="summary-link"'), 1)
        self.assertIn('href="pages/summary.html"', overview)
        self.assertIn("Overall summary", overview)
        self.assertIn("Each page compares five AA methods", overview)
        for characteristic in report.CHARACTERISTICS:
            self.assertIn(f'href="pages/{characteristic.key}.html"', overview)
            self.assertIn(html.escape(characteristic.label), overview)
            self.assertIn(html.escape(characteristic.description), overview)
        for absent in (
            'class="controls"',
            'id="comparison-guide"',
            'class="case"',
            'class="method-card',
            'class="metric-strip"',
            '<img',
            '<table',
            '<script',
        ):
            self.assertNotIn(absent, overview)
        self.assertNotIn("https://", overview)

    def test_summary_shows_one_reference_picture_and_all_values(self):
        method = report.METHODS[0]
        moves = (
            report.SummaryMove(
                "offset-00", 0.0, 0.0,
                report.Metrics(0.98, 40.0, 0.2, 3.0),
            ),
            report.SummaryMove(
                "offset-01", 0.125, 0.375,
                report.Metrics(0.97, 38.0, 0.3, 4.0),
            ),
        )
        comparison = report.SummaryComparison(
            method,
            report.combine_metrics(tuple(move.metrics for move in moves)),
            moves,
        )
        summary = report.render_summary_html(
            "AA report",
            "now",
            pathlib.Path("images/summary/ssaa8.png"),
            (0.0, 0.0),
            (comparison,),
            (
                report.SummaryCharacteristic(
                    report.CHARACTERISTICS[0],
                    (comparison,),
                ),
            ),
        )

        self.assertEqual(summary.count("<img "), 1)
        self.assertIn('src="../images/summary/ssaa8.png"', summary)
        self.assertIn("Only the reference picture is shown", summary)
        self.assertIn(method.label, summary)
        self.assertIn("General comparison", summary)
        self.assertIn("Comparison by visual test", summary)
        self.assertIn(report.CHARACTERISTICS[0].label, summary)
        self.assertIn(
            f'href="{report.CHARACTERISTICS[0].key}.html"', summary
        )
        self.assertIn("Full-scene values for each tiny move", summary)
        self.assertIn("Every visual-test value for each tiny move", summary)
        self.assertIn("0.980000", summary)
        self.assertIn("0.970000", summary)
        for candidate in report.METHODS:
            self.assertNotIn(f"images/summary/{candidate.output_name}", summary)
        self.assertNotIn("<script", summary)
        self.assertNotIn("https://", summary)

    def test_detail_uses_simple_metaphors_and_has_no_split_mode(self):
        metrics = report.Metrics(0.99, 42.0, 0.25, 3.0)
        flat_direct = next(
            method for method in report.METHODS if method.key == "flat-direct"
        )
        comparison = report.Comparison(
            method=flat_direct,
            image_path=pathlib.Path("images/characteristics/shape/case/flat.png"),
            diff_path=pathlib.Path("diffs/characteristics/shape/case/flat.png"),
            roi_metrics=metrics,
            full_metrics=metrics,
        )
        case = report.CharacteristicCase(
            key="offset-00",
            offset_x=0.125,
            offset_y=0.375,
            width=100,
            height=80,
            source_crop=(10, 20, 110, 100),
            metric_roi=(14, 24, 106, 96),
            reference_path=pathlib.Path("images/characteristics/shape/case/ssaa8.png"),
            reference_diff_path=pathlib.Path("diffs/characteristics/shape/case/ssaa8.png"),
            comparisons=(comparison,),
        )
        detail = report.render_characteristic_html(
            "AA report",
            report.CHARACTERISTICS[0],
            (case,),
            8.0,
            "now",
            report.CHARACTERISTICS[-1],
            report.CHARACTERISTICS[1],
        )

        self.assertIn('data-mode-button="original"', detail)
        self.assertIn('data-mode-button="diff"', detail)
        self.assertIn('data-mode-button="blink"', detail)
        self.assertIn('data-mode-button="original" aria-pressed="true">Result</button>', detail)
        self.assertNotIn("split", detail.lower())
        self.assertIn("const modes={'1':'original','2':'diff','3':'blink'}", detail)
        self.assertNotIn("'4':", detail)
        self.assertIn('aria-describedby="comparison-guide"', detail)
        self.assertIn('id="comparison-guide"', detail)
        self.assertIn("Offset (tiny move)", detail)
        self.assertIn(
            "We nudge the shape a fraction of a pixel to see whether its edge quality "
            "stays steady.",
            detail,
        )
        self.assertIn("What the method actually drew", detail)
        self.assertIn("A heat map of disagreements with SSAA8", detail)
        self.assertIn(
            "Black means the same; colors moving from blue toward red and white mean "
            "a bigger mismatch",
            detail,
        )
        self.assertIn("brightens tiny differences but does not change the scores", detail)
        self.assertIn("Like flipping between two drawings. Flicker reveals what changed", detail)
        self.assertIn("A look-alike score", detail)
        self.assertIn("Closer to 1", detail)
        self.assertIn("A pixel-error score", detail)
        self.assertIn("Higher means less error; ∞ means an exact match", detail)
        self.assertIn(
            "MSAA 4×</dt><dd>Checks four spots inside each pixel, then blends them into one edge",
            detail,
        )
        self.assertIn("Our high-detail yardstick: render bigger, then shrink", detail)
        self.assertIn("Useful, but not perfect truth", detail)
        self.assertIn("Tiny move 1 of 1", detail)
        self.assertIn('aria-label="Tiny move sections"', detail)
        self.assertNotIn("source-space", detail)
        self.assertNotIn("Metric ROI", detail)
        self.assertNotIn("<table", detail)
        self.assertNotIn("Diff gain", detail)
        self.assertIn(">SSIM</dt>", detail)
        self.assertIn(">PSNR</dt>", detail)
        self.assertIn('class="image-viewport" tabindex="0" role="img" aria-label=', detail)
        self.assertIn('data-method-label="Flat direct"', detail)
        self.assertIn('data-reference-card="false"', detail)
        self.assertIn('data-reference-card="true"', detail)
        self.assertIn(
            "Candidate result. characteristic Long diagonal edges, offset (0.125, 0.375) px.",
            detail,
        )
        self.assertIn("original: 'Candidate result'", detail)
        self.assertIn("original: 'Reference result'", detail)
        self.assertEqual(detail.count('class="layer '), detail.count('aria-hidden="true"'))
        self.assertIn("image-rendering:pixelated", detail)
        self.assertNotIn("https://", detail)

    def test_offset_parser_rejects_non_finite_values(self):
        self.assertEqual(report.parse_offset(".125, .5"), (0.125, 0.5))
        with self.assertRaises(Exception):
            report.parse_offset("nan,0")

    def test_default_title_describes_the_goal(self):
        self.assertEqual(
            report.build_argument_parser().parse_args([]).title,
            "Anti-aliasing quality",
        )

    def test_candidate_inventory_includes_msaa4_and_keeps_ssaa8_as_reference(self):
        self.assertEqual(len(report.METHODS), 5)
        self.assertEqual(len(report.DEFAULT_OFFSETS) * (len(report.METHODS) + 1), 24)
        msaa4 = next(method for method in report.METHODS if method.key == "msaa4")
        self.assertEqual(msaa4.label, "MSAA 4×")
        self.assertEqual(msaa4.executable, "aa_msaa4_poc")
        self.assertEqual(msaa4.output_name, "msaa4.png")
        self.assertEqual(msaa4.comparison_args, ("--scene", "comparison"))
        self.assertIn("four spots inside each pixel", msaa4.description)
        self.assertEqual(report.REFERENCE.key, "ssaa8")
        self.assertNotIn(report.REFERENCE, report.METHODS)

    def test_renderer_commands_use_the_shared_comparison_scene(self):
        def successful_run(command, **unused):
            output_dir = pathlib.Path(command[command.index("--output-dir") + 1])
            name = {
                "candidate": "flat-direct.png",
                "msaa4": "msaa4.png",
                "reference": "ssaa8.png",
            }[output_dir.name]
            (output_dir / name).write_bytes(b"placeholder")
            return type(
                "Completed",
                (),
                {"returncode": 0, "stdout": "rendered\n", "stderr": ""},
            )()

        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            executable = root / "renderer"
            executable.write_bytes(b"")
            with mock.patch.object(report.subprocess, "run", side_effect=successful_run) as run:
                flat_direct = next(
                    method for method in report.METHODS if method.key == "flat-direct"
                )
                _, candidate_record = report._run_renderer(
                    flat_direct, executable, root / "candidate", (0.125, 0.5), 3.0
                )
                msaa4 = next(method for method in report.METHODS if method.key == "msaa4")
                _, msaa4_record = report._run_renderer(
                    msaa4, executable, root / "msaa4", (0.125, 0.5), 3.0
                )
                _, reference_record = report._run_renderer(
                    report.REFERENCE, executable, root / "reference", (0.125, 0.5), 3.0
                )
        self.assertIn("--comparison", candidate_record["command"])
        self.assertNotIn("--scene", candidate_record["command"])
        for record in (msaa4_record, reference_record):
            self.assertNotIn("--comparison", record["command"])
            scene_index = record["command"].index("--scene")
            self.assertEqual(record["command"][scene_index + 1], "comparison")
        self.assertEqual(candidate_record["command"][-4:], ["--offset-x", "0.125", "--offset-y", "0.5"])
        self.assertEqual(run.call_count, 3)

    def test_end_to_end_report_assembly_with_renderers_stubbed(self):
        perfect_metrics = report.Metrics(1.0, math.inf, 0.0, 0.0)

        def fake_renderer(method, executable, output_dir, offset, timeout):
            del executable, timeout
            output_dir.mkdir(parents=True)
            width, height = report.SCENE_WIDTH, report.SCENE_HEIGHT
            rgb = bytearray((255, 255, 255) * (width * height))
            centers = ((100, 100), (290, 100), (490, 100), (690, 100),
                       (100, 340), (290, 340), (510, 340), (700, 340))
            for x, y in centers:
                start = (y * width + x) * 3
                rgb[start : start + 3] = bytes((100, 120, 140))
            if method != report.REFERENCE:
                value = 20 + 10 * [item.key for item in report.METHODS].index(method.key)
                rgb[(100 * width + 100) * 3 : (100 * width + 100) * 3 + 3] = bytes((value, value, value))
            if method.key == "msaa4":
                rgb[(180 * width + 180) * 3 : (180 * width + 180) * 3 + 3] = bytes((40, 60, 80))
            destination = output_dir / method.output_name
            report.encode_png_rgb(destination, width, height, bytes(rgb))
            return destination, {
                "method": method.key,
                "command": [method.executable, *method.comparison_args],
                "returncode": 0,
                "stdout": "",
                "stderr": "",
            }

        def fake_difference(path, candidate, reference, gain):
            del reference, gain
            report.encode_png_rgb(
                path, candidate.width, candidate.height,
                bytes(candidate.width * candidate.height * 3),
            )

        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            arguments = types.SimpleNamespace(
                build_dir=str(root / "build"),
                output_dir=str(root / "report"),
                offset=[(0.0, 0.0)],
                timeout=3.0,
                roi_padding=1,
                diff_gain=8.0,
                title="Stub report",
            )
            with mock.patch.object(report, "_find_executable", return_value=root / "renderer"), mock.patch.object(
                report, "_run_renderer", side_effect=fake_renderer
            ), mock.patch.object(
                report, "compute_metrics", return_value=perfect_metrics
            ), mock.patch.object(
                report, "write_difference_map", side_effect=fake_difference
            ):
                with contextlib.redirect_stdout(io.StringIO()):
                    report_path = report.generate_report(arguments)
            self.assertTrue(report_path.is_file())
            self.assertTrue((root / "report" / "report.json").is_file())
            manifest = json.loads((root / "report" / "report.json").read_text(encoding="utf-8"))
            self.assertEqual(manifest["reference"], "ssaa8")
            self.assertEqual(manifest["schema_version"], 4)
            self.assertNotIn("cases", manifest)
            self.assertEqual(len(manifest["characteristics"]), 8)
            self.assertEqual(len(manifest["commands"]), len(report.METHODS) + 1)
            commands = {record["method"]: record["command"] for record in manifest["commands"]}
            self.assertEqual(commands["msaa4"][1:3], ["--scene", "comparison"])
            self.assertEqual(commands["ssaa8"][1:3], ["--scene", "comparison"])
            self.assertIn("--comparison", commands["flat-direct"])
            self.assertFalse((root / "report" / "images" / "offset-00").exists())
            self.assertFalse((root / "report" / "diffs" / "offset-00").exists())
            summary_manifest = manifest["summary"]
            self.assertEqual(summary_manifest["page"], "pages/summary.html")
            self.assertEqual(
                summary_manifest["reference_image"], "images/summary/ssaa8.png"
            )
            self.assertEqual(
                [comparison["method"] for comparison in summary_manifest["comparisons"]],
                [method.key for method in report.METHODS],
            )
            self.assertTrue(
                (root / "report" / summary_manifest["reference_image"]).is_file()
            )
            self.assertEqual(
                list((root / "report" / "images" / "summary").iterdir()),
                [root / "report" / "images" / "summary" / "ssaa8.png"],
            )
            summary_page = root / "report" / summary_manifest["page"]
            summary_html = summary_page.read_text(encoding="utf-8")
            self.assertEqual(summary_html.count("<img "), 1)
            self.assertIn('src="../images/summary/ssaa8.png"', summary_html)
            self.assertIn("Comparison by visual test", summary_html)
            self.assertIn("Every visual-test value for each tiny move", summary_html)
            self.assertEqual(len(summary_manifest["characteristics"]), 8)
            self.assertEqual(
                summary_html.count('class="scene-name"'), len(report.CHARACTERISTICS)
            )
            self.assertEqual(
                summary_html.count('class="method-name"'),
                len(report.CHARACTERISTICS) * len(report.METHODS),
            )
            for comparison in summary_manifest["comparisons"]:
                self.assertIn("overall_metrics", comparison)
                self.assertEqual(len(comparison["moves"]), 1)
                self.assertNotIn("image", comparison)
            for characteristic_summary in summary_manifest["characteristics"]:
                self.assertEqual(
                    [item["method"] for item in characteristic_summary["comparisons"]],
                    [method.key for method in report.METHODS],
                )
                for comparison in characteristic_summary["comparisons"]:
                    self.assertIn("overall_metrics", comparison)
                    self.assertEqual(len(comparison["moves"]), 1)
                    self.assertNotIn("image", comparison)
            for characteristic in manifest["characteristics"]:
                page = root / "report" / characteristic["page"]
                self.assertTrue(page.is_file())
                visual_crop = characteristic["visual_crop"]
                metric_roi = characteristic["metric_roi"]
                self.assertGreaterEqual(visual_crop["width"], metric_roi["width"])
                self.assertGreaterEqual(visual_crop["height"], metric_roi["height"])
                self.assertIn("half-open [x, x + width)", characteristic["coordinate_convention"])
                self.assertEqual(len(characteristic["cases"]), 1)
                detail_case = characteristic["cases"][0]
                self.assertEqual(len(detail_case["comparisons"]), 5)
                self.assertEqual(
                    [comparison["method"] for comparison in detail_case["comparisons"]],
                    [method.key for method in report.METHODS],
                )
                dimensions = detail_case["dimensions"]
                detail = page.read_text(encoding="utf-8")
                self.assertEqual(
                    detail.count('<article class="method-card'), len(report.METHODS) + 1
                )
                self.assertLess(
                    detail.index("<h3>SSAA8 reference</h3>"),
                    detail.index("<h3>MSAA 4×</h3>"),
                )
                self.assertLess(
                    detail.index("<h3>MSAA 4×</h3>"),
                    detail.index("<h3>Flat direct</h3>"),
                )
                for comparison in detail_case["comparisons"]:
                    image = report.decode_png(root / "report" / comparison["image"])
                    difference = report.decode_png(root / "report" / comparison["difference_image"])
                    self.assertEqual((image.width, image.height), (dimensions["width"], dimensions["height"]))
                    self.assertEqual((difference.width, difference.height), (dimensions["width"], dimensions["height"]))
                    self.assertIn("characteristic_metrics", comparison)
                    self.assertIn(f'src="../{comparison["image"]}"', detail)
                    self.assertIn(f'src="../{comparison["difference_image"]}"', detail)

            slanted = next(
                characteristic
                for characteristic in manifest["characteristics"]
                if characteristic["key"] == "slanted-edges"
            )
            self.assertGreaterEqual(
                slanted["visual_crop"]["x"] + slanted["visual_crop"]["width"], 193
            )
            self.assertGreaterEqual(
                slanted["metric_roi"]["x"] + slanted["metric_roi"]["width"], 182
            )
            run_log = (root / "report" / "run-log.txt").read_text(encoding="utf-8")
            self.assertIn("[offset-00] msaa4", run_log)
            self.assertIn("$ aa_msaa4_poc --scene comparison", run_log)

            overview = report_path.read_text(encoding="utf-8")
            self.assertEqual(overview.count('class="characteristic-link"'), 8)
            self.assertEqual(overview.count('class="summary-link"'), 1)
            self.assertIn('href="pages/summary.html"', overview)
            self.assertNotIn("<img", overview)
            self.assertNotIn("<script", overview)
            for characteristic in report.CHARACTERISTICS:
                self.assertIn(f'href="pages/{characteristic.key}.html"', overview)
            for page in [report_path, *(root / "report" / "pages").glob("*.html")]:
                parser = LocalLinkParser()
                document = page.read_text(encoding="utf-8")
                parser.feed(document)
                self.assertNotIn("https://", document)
                if page != report_path:
                    self.assertNotIn("split", document.lower())
                for link in parser.links:
                    target = link.split("#", 1)[0]
                    if target:
                        self.assertTrue((page.parent / target).resolve().is_file(), (page, link))

    def test_failed_generation_is_retry_safe(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            arguments = types.SimpleNamespace(
                build_dir=str(root / "build"), output_dir=str(root / "report"),
                offset=[(0.0, 0.0)], timeout=3.0, roi_padding=4,
                diff_gain=8.0, title="Failure report",
            )
            with mock.patch.object(report, "_find_executable", return_value=root / "renderer"), mock.patch.object(
                report, "_run_renderer", side_effect=report.ReportError("renderer failed")
            ):
                with self.assertRaisesRegex(report.ReportError, "renderer failed"):
                    with contextlib.redirect_stdout(io.StringIO()):
                        report.generate_report(arguments)
            self.assertTrue((root / "report").is_dir())
            self.assertEqual(list((root / "report").iterdir()), [])
            self.assertEqual(list(root.glob(".report-staging-*")), [])

    def test_generation_rejects_non_shared_scene_dimensions(self):
        def small_renderer(method, executable, output_dir, offset, timeout):
            del executable, offset, timeout
            output_dir.mkdir(parents=True)
            destination = output_dir / method.output_name
            report.encode_png_rgb(destination, 12, 12, bytes((255, 255, 255) * 144))
            return destination, {
                "method": method.key, "command": [method.executable],
                "returncode": 0, "stdout": "", "stderr": "",
            }

        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            arguments = types.SimpleNamespace(
                build_dir=str(root / "build"), output_dir=str(root / "report"),
                offset=[(0.0, 0.0)], timeout=3.0, roi_padding=4,
                diff_gain=8.0, title="Small report",
            )
            with mock.patch.object(report, "_find_executable", return_value=root / "renderer"), mock.patch.object(
                report, "_run_renderer", side_effect=small_renderer
            ):
                with self.assertRaisesRegex(report.ReportError, "800x480"):
                    with contextlib.redirect_stdout(io.StringIO()):
                        report.generate_report(arguments)
            self.assertEqual(list((root / "report").iterdir()), [])


if __name__ == "__main__":
    unittest.main()
