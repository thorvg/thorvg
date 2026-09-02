#!/usr/bin/env python3
"""Render the legacy AAA comparison scene at diagnostic display scales."""

from __future__ import annotations

import argparse
import csv
import pathlib
import subprocess
from typing import Sequence

import generate_aa_report as report


SCALES = (
    ("quarter", 0.25, 200, 120),
    ("half", 0.5, 400, 240),
    ("original", 1.0, 800, 480),
)

OFFSETS = (
    (0.0, 0.0),
    (0.125, 0.375),
    (0.5, 0.5),
    (0.875, 0.625),
)


def _number_key(value: float) -> str:
    return f"{value:.3f}".replace("-", "m").replace(".", "p")


def _format_number(value: float) -> str:
    return f"{value:.3f}".rstrip("0").rstrip(".")


def render(ssaa8_bin: pathlib.Path, output_dir: pathlib.Path) -> pathlib.Path:
    ssaa8_bin = ssaa8_bin.resolve()
    output_dir = output_dir.resolve()
    if not ssaa8_bin.is_file():
        raise report.ReportError(f"SSAA8 executable does not exist: {ssaa8_bin}")
    output_dir.mkdir(parents=True, exist_ok=True)
    manifest_path = output_dir / "comparison-reference-manifest.tsv"

    with manifest_path.open("w", encoding="utf-8", newline="") as destination:
        manifest = csv.writer(destination, delimiter="\t", lineterminator="\n")
        manifest.writerow(
            ("scene", "scale", "offset_x", "offset_y", "reference_png")
        )
        for scale_name, render_scale, width, height in SCALES:
            for offset_x, offset_y in OFFSETS:
                offset_dir = (
                    f"offset-x{_number_key(offset_x)}-y{_number_key(offset_y)}"
                )
                relative_dir = (
                    pathlib.Path("diagnostic")
                    / "comparison"
                    / scale_name
                    / offset_dir
                )
                absolute_dir = output_dir / relative_dir
                absolute_dir.mkdir(parents=True, exist_ok=True)
                command = (
                    str(ssaa8_bin),
                    "--scene",
                    "comparison",
                    "--render-scale",
                    _format_number(render_scale),
                    "--offset-x",
                    _format_number(offset_x),
                    "--offset-y",
                    _format_number(offset_y),
                    "--output-dir",
                    str(absolute_dir),
                )
                try:
                    subprocess.run(command, check=True)
                except (OSError, subprocess.CalledProcessError) as error:
                    raise report.ReportError(
                        f"SSAA8 render failed for {scale_name} offset "
                        f"({offset_x}, {offset_y}): {error}"
                    ) from error
                reference = absolute_dir / "ssaa8.png"
                image = report.decode_png(reference)
                if (image.width, image.height) != (width, height):
                    raise report.ReportError(
                        f"{reference} is {image.width}x{image.height}; expected "
                        f"{width}x{height}"
                    )
                manifest.writerow(
                    (
                        "comparison",
                        scale_name,
                        _format_number(offset_x),
                        _format_number(offset_y),
                        (relative_dir / "ssaa8.png").as_posix(),
                    )
                )
    return manifest_path


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ssaa8-bin", required=True, type=pathlib.Path)
    parser.add_argument("--output-dir", required=True, type=pathlib.Path)
    args = parser.parse_args(argv)
    try:
        manifest = render(args.ssaa8_bin, args.output_dir)
    except report.ReportError as error:
        parser.error(str(error))
    print(f"Wrote {manifest}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
