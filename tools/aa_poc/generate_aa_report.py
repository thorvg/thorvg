#!/usr/bin/env python3
"""Render and compare five anti-aliasing methods in an offline HTML report.

The implementation intentionally uses only the Python standard library.  It
decodes the PNGs emitted by the POC binaries, computes metrics against SSAA8,
writes amplified difference maps, and creates a self-contained (apart from its
local image files) HTML viewer.
"""

from __future__ import annotations

import argparse
import dataclasses
import datetime as _datetime
import html
import json
import math
import os
import pathlib
import shlex
import shutil
import struct
import subprocess
import sys
import tempfile
import zlib
from typing import Iterable, Optional, Sequence, Tuple


PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
DEFAULT_OFFSETS = ((0.0, 0.0), (0.125, 0.375), (0.5, 0.5), (0.875, 0.625))
SCENE_WIDTH = 800
SCENE_HEIGHT = 480
BLINK_INTERVAL_MS = 650


class ReportError(RuntimeError):
    """A user-facing report generation error."""


@dataclasses.dataclass(frozen=True)
class Method:
    key: str
    label: str
    executable: str
    output_name: str
    description: str
    comparison_args: Tuple[str, ...] = ("--comparison",)


METHODS = (
    Method(
        "msaa4",
        "MSAA 4×",
        "aa_msaa4_poc",
        "msaa4.png",
        "Checks four spots inside each pixel, then blends them into one edge.",
        comparison_args=("--scene", "comparison"),
    ),
    Method(
        "flat-direct",
        "Flat direct",
        "aa_flat_direct_poc",
        "flat-direct.png",
        "Straightened edges drawn directly into the image.",
    ),
    Method(
        "curve-direct",
        "Curve direct",
        "aa_curve_direct_poc",
        "curve-direct.png",
        "Curved edges drawn directly into the image.",
    ),
    Method(
        "flat-mask",
        "Flat mask",
        "aa_flat_mask_poc",
        "flat-mask.png",
        "Straightened edges drawn through a coverage mask.",
    ),
    Method(
        "curve-mask",
        "Curve mask",
        "aa_curve_mask_poc",
        "curve-mask.png",
        "Curved edges drawn through a coverage mask.",
    ),
)

REFERENCE = Method(
    "ssaa8",
    "SSAA8 reference",
    "aa_ssaa8_poc",
    "ssaa8.png",
    "A high-detail image made by drawing 8× larger, then shrinking.",
    comparison_args=("--scene", "comparison"),
)


@dataclasses.dataclass(frozen=True)
class Characteristic:
    """One isolated visual characteristic in the shared comparison scene."""

    key: str
    label: str
    description: str
    search_roi: Tuple[int, int, int, int]


# The scene deliberately places one shape in each cell of a 4x2 layout.  These
# non-overlapping search regions are broad enough for every configured subpixel
# offset.  One fixed crop per characteristic is tightened to the union of
# visible pixels across every configured offset, all methods, and the reference.
CHARACTERISTICS = (
    Characteristic(
        "slanted-edges",
        "Long diagonal edges",
        "Do long diagonal edges stay smooth and even?",
        (0, 0, 200, 210),
    ),
    Characteristic(
        "corner-joins",
        "Sharp and wide corners",
        "Do corners keep their shape without thick spots or cut-offs?",
        (200, 0, 391, 210),
    ),
    Characteristic(
        "circle-curvature",
        "Circles",
        "Does a round edge stay evenly smooth?",
        (391, 0, 593, 210),
    ),
    Characteristic(
        "changing-curvature",
        "Changing curves",
        "Does a curve stay smooth as it bends more or less?",
        (593, 0, 800, 210),
    ),
    Characteristic(
        "shallow-edge-motion",
        "Nearly horizontal edges",
        "Does a nearly horizontal edge stay steady when nudged?",
        (0, 210, 207, 480),
    ),
    Characteristic(
        "mixed-endpoints",
        "Lines meeting curves",
        "Do straight lines and curves meet cleanly?",
        (207, 210, 400, 480),
    ),
    Characteristic(
        "cubic-join-seams",
        "Curve joins",
        "Do connected curves avoid cracks and overlaps?",
        (400, 210, 629, 480),
    ),
    Characteristic(
        "translucent-overdraw",
        "Half-transparent edges",
        "Does a half-transparent edge avoid dark spots from being drawn twice?",
        (629, 210, 800, 480),
    ),
)


@dataclasses.dataclass(frozen=True)
class PngImage:
    width: int
    height: int
    rgba: bytes

    def __post_init__(self) -> None:
        expected = self.width * self.height * 4
        if self.width <= 0 or self.height <= 0:
            raise ValueError("PNG dimensions must be positive")
        if len(self.rgba) != expected:
            raise ValueError(f"RGBA buffer has {len(self.rgba)} bytes; expected {expected}")


@dataclasses.dataclass(frozen=True)
class Metrics:
    ssim: float
    psnr_db: float
    mean_absolute_error: float
    max_absolute_error: float


@dataclasses.dataclass(frozen=True)
class SummaryMove:
    key: str
    offset_x: float
    offset_y: float
    metrics: Metrics


@dataclasses.dataclass(frozen=True)
class SummaryComparison:
    method: Method
    overall_metrics: Metrics
    moves: Tuple[SummaryMove, ...]


@dataclasses.dataclass(frozen=True)
class SummaryCharacteristic:
    characteristic: Characteristic
    comparisons: Tuple[SummaryComparison, ...]


@dataclasses.dataclass(frozen=True)
class Comparison:
    method: Method
    image_path: pathlib.Path
    diff_path: pathlib.Path
    roi_metrics: Metrics
    full_metrics: Metrics


@dataclasses.dataclass(frozen=True)
class CharacteristicCase:
    key: str
    offset_x: float
    offset_y: float
    width: int
    height: int
    source_crop: Tuple[int, int, int, int]
    metric_roi: Tuple[int, int, int, int]
    reference_path: pathlib.Path
    reference_diff_path: pathlib.Path
    comparisons: Tuple[Comparison, ...]


def _png_chunk(chunk_type: bytes, payload: bytes) -> bytes:
    crc = zlib.crc32(chunk_type)
    crc = zlib.crc32(payload, crc) & 0xFFFFFFFF
    return struct.pack(">I", len(payload)) + chunk_type + payload + struct.pack(">I", crc)


def _paeth(left: int, up: int, upper_left: int) -> int:
    estimate = left + up - upper_left
    distance_left = abs(estimate - left)
    distance_up = abs(estimate - up)
    distance_upper_left = abs(estimate - upper_left)
    if distance_left <= distance_up and distance_left <= distance_upper_left:
        return left
    if distance_up <= distance_upper_left:
        return up
    return upper_left


def decode_png(path: pathlib.Path) -> PngImage:
    """Decode a non-interlaced 8-bit PNG into straight RGBA bytes.

    All PNG color types that can use an 8-bit depth are accepted.  Supporting
    palette output is important because LodePNG may choose it automatically.
    """

    path = pathlib.Path(path)
    try:
        encoded = path.read_bytes()
    except OSError as error:
        raise ReportError(f"cannot read PNG {path}: {error}") from error
    if not encoded.startswith(PNG_SIGNATURE):
        raise ReportError(f"not a PNG file: {path}")

    position = len(PNG_SIGNATURE)
    width = height = bit_depth = color_type = interlace = None
    palette: Optional[bytes] = None
    transparency: Optional[bytes] = None
    compressed_parts = []
    saw_iend = False

    while position < len(encoded):
        if position + 12 > len(encoded):
            raise ReportError(f"truncated PNG chunk in {path}")
        length = struct.unpack(">I", encoded[position : position + 4])[0]
        chunk_type = encoded[position + 4 : position + 8]
        data_start = position + 8
        data_end = data_start + length
        crc_end = data_end + 4
        if crc_end > len(encoded):
            raise ReportError(f"truncated {chunk_type!r} chunk in {path}")
        payload = encoded[data_start:data_end]
        expected_crc = struct.unpack(">I", encoded[data_end:crc_end])[0]
        actual_crc = zlib.crc32(chunk_type)
        actual_crc = zlib.crc32(payload, actual_crc) & 0xFFFFFFFF
        if actual_crc != expected_crc:
            name = chunk_type.decode("ascii", "replace")
            raise ReportError(f"CRC mismatch in {name} chunk of {path}")
        position = crc_end

        if chunk_type == b"IHDR":
            if width is not None or len(payload) != 13:
                raise ReportError(f"invalid IHDR in {path}")
            width, height, bit_depth, color_type, compression, filtering, interlace = struct.unpack(
                ">IIBBBBB", payload
            )
            if width == 0 or height == 0:
                raise ReportError(f"invalid zero-sized PNG: {path}")
            if bit_depth != 8:
                raise ReportError(f"{path} uses unsupported PNG bit depth {bit_depth}; expected 8")
            if color_type not in (0, 2, 3, 4, 6):
                raise ReportError(f"{path} uses unsupported PNG color type {color_type}")
            if compression != 0 or filtering != 0 or interlace != 0:
                raise ReportError(f"{path} must be a non-interlaced PNG using standard compression/filtering")
        elif chunk_type == b"PLTE":
            palette = payload
        elif chunk_type == b"tRNS":
            transparency = payload
        elif chunk_type == b"IDAT":
            compressed_parts.append(payload)
        elif chunk_type == b"IEND":
            saw_iend = True
            break

    if width is None or not compressed_parts or not saw_iend:
        raise ReportError(f"incomplete PNG: {path}")
    channels = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[color_type]
    bytes_per_pixel = channels
    stride = width * channels
    expected_size = height * (stride + 1)
    try:
        filtered = zlib.decompress(b"".join(compressed_parts))
    except zlib.error as error:
        raise ReportError(f"invalid compressed PNG data in {path}: {error}") from error
    if len(filtered) != expected_size:
        raise ReportError(
            f"decoded PNG data in {path} has {len(filtered)} bytes; expected {expected_size}"
        )

    rows = bytearray(height * stride)
    source = 0
    for y in range(height):
        filter_type = filtered[source]
        source += 1
        if filter_type > 4:
            raise ReportError(f"unsupported PNG filter {filter_type} in {path}")
        row_start = y * stride
        previous_start = row_start - stride
        for x in range(stride):
            value = filtered[source + x]
            left = rows[row_start + x - bytes_per_pixel] if x >= bytes_per_pixel else 0
            up = rows[previous_start + x] if y else 0
            upper_left = (
                rows[previous_start + x - bytes_per_pixel]
                if y and x >= bytes_per_pixel
                else 0
            )
            if filter_type == 1:
                value = (value + left) & 0xFF
            elif filter_type == 2:
                value = (value + up) & 0xFF
            elif filter_type == 3:
                value = (value + ((left + up) // 2)) & 0xFF
            elif filter_type == 4:
                value = (value + _paeth(left, up, upper_left)) & 0xFF
            rows[row_start + x] = value
        source += stride

    rgba = bytearray(width * height * 4)
    if color_type == 3:
        if palette is None or len(palette) == 0 or len(palette) % 3:
            raise ReportError(f"invalid or missing palette in {path}")
        palette_entries = len(palette) // 3
        for index, palette_index in enumerate(rows):
            if palette_index >= palette_entries:
                raise ReportError(f"palette index {palette_index} is out of range in {path}")
            output = index * 4
            palette_offset = palette_index * 3
            rgba[output : output + 3] = palette[palette_offset : palette_offset + 3]
            rgba[output + 3] = (
                transparency[palette_index]
                if transparency is not None and palette_index < len(transparency)
                else 255
            )
    else:
        transparent_gray = None
        transparent_rgb = None
        if color_type == 0 and transparency is not None and len(transparency) >= 2:
            transparent_gray = struct.unpack(">H", transparency[:2])[0]
        elif color_type == 2 and transparency is not None and len(transparency) >= 6:
            transparent_rgb = struct.unpack(">HHH", transparency[:6])
        source = 0
        for pixel in range(width * height):
            output = pixel * 4
            if color_type == 0:
                gray = rows[source]
                source += 1
                rgba[output : output + 3] = bytes((gray, gray, gray))
                rgba[output + 3] = 0 if transparent_gray == gray else 255
            elif color_type == 2:
                red, green, blue = rows[source : source + 3]
                source += 3
                rgba[output : output + 3] = bytes((red, green, blue))
                rgba[output + 3] = 0 if transparent_rgb == (red, green, blue) else 255
            elif color_type == 4:
                gray, alpha = rows[source : source + 2]
                source += 2
                rgba[output : output + 4] = bytes((gray, gray, gray, alpha))
            else:  # color_type == 6
                rgba[output : output + 4] = rows[source : source + 4]
                source += 4
    return PngImage(width, height, bytes(rgba))


def encode_png_rgb(path: pathlib.Path, width: int, height: int, rgb: bytes) -> None:
    """Write an 8-bit non-interlaced RGB PNG with no external dependency."""

    if width <= 0 or height <= 0 or len(rgb) != width * height * 3:
        raise ValueError("invalid RGB image dimensions or buffer length")
    scanlines = bytearray()
    stride = width * 3
    for y in range(height):
        scanlines.append(0)
        start = y * stride
        scanlines.extend(rgb[start : start + stride])
    ihdr = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    encoded = bytearray(PNG_SIGNATURE)
    encoded.extend(_png_chunk(b"IHDR", ihdr))
    encoded.extend(_png_chunk(b"sRGB", b"\x00"))
    encoded.extend(_png_chunk(b"IDAT", zlib.compress(bytes(scanlines), 9)))
    encoded.extend(_png_chunk(b"IEND", b""))
    try:
        pathlib.Path(path).write_bytes(encoded)
    except OSError as error:
        raise ReportError(f"cannot write PNG {path}: {error}") from error


def composite_over_white(image: PngImage) -> bytes:
    """Return browser-like 8-bit RGB after straight-alpha compositing on white."""

    rgb = bytearray(image.width * image.height * 3)
    for pixel in range(image.width * image.height):
        source = pixel * 4
        destination = pixel * 3
        alpha = image.rgba[source + 3]
        inverse_alpha = 255 - alpha
        for channel in range(3):
            rgb[destination + channel] = (
                image.rgba[source + channel] * alpha + 255 * inverse_alpha + 127
            ) // 255
    return bytes(rgb)


def validate_same_size(images: Iterable[PngImage]) -> Tuple[int, int]:
    iterator = iter(images)
    try:
        first = next(iterator)
    except StopIteration as error:
        raise ValueError("at least one image is required") from error
    for image in iterator:
        if (image.width, image.height) != (first.width, first.height):
            raise ReportError(
                "comparison images have different dimensions: "
                f"{first.width}x{first.height} versus {image.width}x{image.height}"
            )
    return first.width, first.height


def common_content_roi(
    images: Sequence[PngImage], padding: int = 4
) -> Tuple[int, int, int, int]:
    """Return one shared, padded non-white content rectangle for all methods."""

    if padding < 0:
        raise ValueError("ROI padding cannot be negative")
    width, height = validate_same_size(images)
    composited = [composite_over_white(image) for image in images]
    min_x, min_y, max_x, max_y = width, height, -1, -1
    for y in range(height):
        for x in range(width):
            offset = (y * width + x) * 3
            if any(
                pixels[offset] != 255
                or pixels[offset + 1] != 255
                or pixels[offset + 2] != 255
                for pixels in composited
            ):
                min_x = min(min_x, x)
                min_y = min(min_y, y)
                max_x = max(max_x, x)
                max_y = max(max_y, y)
    if max_x < min_x:
        return (0, 0, width, height)
    return (
        max(0, min_x - padding),
        max(0, min_y - padding),
        min(width, max_x + padding + 1),
        min(height, max_y + padding + 1),
    )


def content_roi_within(
    images: Sequence[PngImage],
    search_roi: Tuple[int, int, int, int],
    padding: int = 4,
) -> Tuple[int, int, int, int]:
    """Find content inside ``search_roi`` and pad it within the full image.

    The result is derived from the union of all supplied renderings so one
    method can neither select a more favorable crop nor hide stray coverage.
    An empty region is rejected because it means the shared comparison scene
    no longer matches the report's named characteristic layout.
    """

    if padding < 0:
        raise ValueError("ROI padding cannot be negative")
    width, height = validate_same_size(images)
    probe = PngImage(width, height, bytes(width * height * 4))
    x0, y0, x1, y1 = _validate_roi(probe, search_roi)
    min_x, min_y, max_x, max_y = x1, y1, -1, -1
    for image in images:
        rgba = image.rgba
        opaque = rgba[3::4].count(255) == width * height
        for y in range(y0, y1):
            row_offset = (y * width + x0) * 4
            for x in range(x0, x1):
                offset = row_offset + (x - x0) * 4
                red, green, blue, alpha = rgba[offset : offset + 4]
                if opaque:
                    visible = red != 255 or green != 255 or blue != 255
                else:
                    inverse_alpha = 255 - alpha
                    visible = any(
                        (channel * alpha + 255 * inverse_alpha + 127) // 255 != 255
                        for channel in (red, green, blue)
                    )
                if not visible:
                    continue
                min_x = min(min_x, x)
                min_y = min(min_y, y)
                max_x = max(max_x, x)
                max_y = max(max_y, y)
    if max_x < min_x:
        raise ReportError(
            f"no visible content in characteristic search region {search_roi}"
        )
    return (
        max(0, min_x - padding),
        max(0, min_y - padding),
        min(width, max_x + padding + 1),
        min(height, max_y + padding + 1),
    )


def crop_image(image: PngImage, roi: Tuple[int, int, int, int]) -> PngImage:
    """Return an exact RGBA crop using half-open image coordinates."""

    x0, y0, x1, y1 = _validate_roi(image, roi)
    width = x1 - x0
    height = y1 - y0
    row_bytes = width * 4
    rgba = bytearray(row_bytes * height)
    for destination_y, source_y in enumerate(range(y0, y1)):
        source_start = (source_y * image.width + x0) * 4
        destination_start = destination_y * row_bytes
        rgba[destination_start : destination_start + row_bytes] = image.rgba[
            source_start : source_start + row_bytes
        ]
    return PngImage(width, height, bytes(rgba))


def write_png_image(path: pathlib.Path, image: PngImage) -> None:
    """Write a report image after applying the report's white matte."""

    encode_png_rgb(path, image.width, image.height, composite_over_white(image))


def _validate_roi(image: PngImage, roi: Optional[Tuple[int, int, int, int]]) -> Tuple[int, int, int, int]:
    if roi is None:
        return (0, 0, image.width, image.height)
    x0, y0, x1, y1 = roi
    if not (0 <= x0 < x1 <= image.width and 0 <= y0 < y1 <= image.height):
        raise ValueError(f"invalid ROI {roi} for {image.width}x{image.height} image")
    return roi


def _ssim_luminance(
    candidate_rgb: bytes,
    reference_rgb: bytes,
    image_width: int,
    roi: Tuple[int, int, int, int],
) -> float:
    """Mean local luminance SSIM using valid, uniform windows.

    The window is 11x11 when the region permits it and otherwise the largest
    odd square that fits.  Population moments match the weighted-moment form of
    the original SSIM definition.
    """

    x0, y0, x1, y1 = roi
    width = x1 - x0
    height = y1 - y0
    window = min(11, width, height)
    if window % 2 == 0:
        window -= 1
    window = max(window, 1)
    output_width = width - window + 1
    area = float(window * window)
    c1 = (0.01 * 255.0) ** 2
    c2 = (0.03 * 255.0) ** 2

    vertical = [[0.0] * output_width for _ in range(5)]
    ring = []
    total_ssim = 0.0
    window_count = 0

    for local_y in range(height):
        y = y0 + local_y
        candidate_row = [0.0] * width
        reference_row = [0.0] * width
        for local_x in range(width):
            offset = (y * image_width + x0 + local_x) * 3
            candidate_row[local_x] = (
                0.2126 * candidate_rgb[offset]
                + 0.7152 * candidate_rgb[offset + 1]
                + 0.0722 * candidate_rgb[offset + 2]
            )
            reference_row[local_x] = (
                0.2126 * reference_rgb[offset]
                + 0.7152 * reference_rgb[offset + 1]
                + 0.0722 * reference_rgb[offset + 2]
            )

        horizontal = [[0.0] * output_width for _ in range(5)]
        sum_x = sum(candidate_row[:window])
        sum_y = sum(reference_row[:window])
        sum_xx = sum(value * value for value in candidate_row[:window])
        sum_yy = sum(value * value for value in reference_row[:window])
        sum_xy = sum(
            candidate_row[index] * reference_row[index] for index in range(window)
        )
        for column in range(output_width):
            if column:
                remove = column - 1
                add = column + window - 1
                old_x = candidate_row[remove]
                old_y = reference_row[remove]
                new_x = candidate_row[add]
                new_y = reference_row[add]
                sum_x += new_x - old_x
                sum_y += new_y - old_y
                sum_xx += new_x * new_x - old_x * old_x
                sum_yy += new_y * new_y - old_y * old_y
                sum_xy += new_x * new_y - old_x * old_y
            horizontal[0][column] = sum_x
            horizontal[1][column] = sum_y
            horizontal[2][column] = sum_xx
            horizontal[3][column] = sum_yy
            horizontal[4][column] = sum_xy

        if len(ring) == window:
            old = ring.pop(0)
            for moment in range(5):
                vertical_moment = vertical[moment]
                old_moment = old[moment]
                for column in range(output_width):
                    vertical_moment[column] -= old_moment[column]
        ring.append(horizontal)
        for moment in range(5):
            vertical_moment = vertical[moment]
            new_moment = horizontal[moment]
            for column in range(output_width):
                vertical_moment[column] += new_moment[column]

        if len(ring) != window:
            continue
        for column in range(output_width):
            mean_x = vertical[0][column] / area
            mean_y = vertical[1][column] / area
            variance_x = max(0.0, vertical[2][column] / area - mean_x * mean_x)
            variance_y = max(0.0, vertical[3][column] / area - mean_y * mean_y)
            covariance = vertical[4][column] / area - mean_x * mean_y
            numerator = (2.0 * mean_x * mean_y + c1) * (2.0 * covariance + c2)
            denominator = (mean_x * mean_x + mean_y * mean_y + c1) * (
                variance_x + variance_y + c2
            )
            total_ssim += numerator / denominator
            window_count += 1
    return total_ssim / window_count


def compute_metrics(
    candidate: PngImage,
    reference: PngImage,
    roi: Optional[Tuple[int, int, int, int]] = None,
) -> Metrics:
    """Compute RGB PSNR/error metrics and local luminance SSIM."""

    validate_same_size((candidate, reference))
    roi = _validate_roi(candidate, roi)
    candidate_rgb = composite_over_white(candidate)
    reference_rgb = composite_over_white(reference)
    x0, y0, x1, y1 = roi
    if candidate_rgb == reference_rgb:
        return Metrics(
            ssim=1.0,
            psnr_db=math.inf,
            mean_absolute_error=0.0,
            max_absolute_error=0.0,
        )
    squared_error = 0.0
    absolute_error = 0.0
    maximum_error = 0
    sample_count = (x1 - x0) * (y1 - y0) * 3
    for y in range(y0, y1):
        row_start = (y * candidate.width + x0) * 3
        row_end = (y * candidate.width + x1) * 3
        for index in range(row_start, row_end):
            difference = abs(candidate_rgb[index] - reference_rgb[index])
            squared_error += difference * difference
            absolute_error += difference
            maximum_error = max(maximum_error, difference)
    mean_squared_error = squared_error / sample_count
    psnr = math.inf if mean_squared_error == 0.0 else 10.0 * math.log10(255.0**2 / mean_squared_error)
    return Metrics(
        ssim=(
            1.0
            if mean_squared_error == 0.0
            else _ssim_luminance(candidate_rgb, reference_rgb, candidate.width, roi)
        ),
        psnr_db=psnr,
        mean_absolute_error=absolute_error / sample_count,
        max_absolute_error=float(maximum_error),
    )


def combine_metrics(metrics: Sequence[Metrics]) -> Metrics:
    """Combine equal-sized move measurements without averaging PSNR decibels."""

    if not metrics:
        raise ValueError("at least one metric measurement is required")
    mean_squared_errors = [
        0.0
        if math.isinf(item.psnr_db)
        else 255.0**2 / (10.0 ** (item.psnr_db / 10.0))
        for item in metrics
    ]
    combined_mse = sum(mean_squared_errors) / len(mean_squared_errors)
    return Metrics(
        ssim=sum(item.ssim for item in metrics) / len(metrics),
        psnr_db=(
            math.inf
            if combined_mse == 0.0
            else 10.0 * math.log10(255.0**2 / combined_mse)
        ),
        mean_absolute_error=(
            sum(item.mean_absolute_error for item in metrics) / len(metrics)
        ),
        max_absolute_error=max(item.max_absolute_error for item in metrics),
    )


def _heat_color(value: float) -> Tuple[int, int, int]:
    """Map [0, 1] error magnitude to a black-blue-cyan-yellow-red-white ramp."""

    if value <= 0.0:
        return (0, 0, 0)
    stops = (
        (0.00, (0, 0, 0)),
        (0.10, (24, 24, 128)),
        (0.30, (0, 160, 255)),
        (0.55, (255, 235, 0)),
        (0.80, (255, 48, 0)),
        (1.00, (255, 255, 255)),
    )
    value = min(value, 1.0)
    for index in range(1, len(stops)):
        upper_position, upper_color = stops[index]
        lower_position, lower_color = stops[index - 1]
        if value <= upper_position:
            amount = (value - lower_position) / (upper_position - lower_position)
            return tuple(
                int(round(lower_color[channel] + amount * (upper_color[channel] - lower_color[channel])))
                for channel in range(3)
            )
    return stops[-1][1]


def write_difference_map(
    path: pathlib.Path, candidate: PngImage, reference: PngImage, gain: float
) -> None:
    if not math.isfinite(gain) or gain <= 0.0:
        raise ValueError("difference-map gain must be positive and finite")
    validate_same_size((candidate, reference))
    candidate_rgb = composite_over_white(candidate)
    reference_rgb = composite_over_white(reference)
    output = bytearray(candidate.width * candidate.height * 3)
    for pixel in range(candidate.width * candidate.height):
        offset = pixel * 3
        magnitude = max(
            abs(candidate_rgb[offset + channel] - reference_rgb[offset + channel])
            for channel in range(3)
        )
        color = _heat_color(magnitude * gain / 255.0)
        output[offset : offset + 3] = bytes(color)
    encode_png_rgb(path, candidate.width, candidate.height, bytes(output))


def parse_offset(text: str) -> Tuple[float, float]:
    parts = text.split(",")
    if len(parts) != 2:
        raise argparse.ArgumentTypeError("offset must be X,Y")
    try:
        x, y = (float(part.strip()) for part in parts)
    except ValueError as error:
        raise argparse.ArgumentTypeError("offset must contain two numbers") from error
    if not math.isfinite(x) or not math.isfinite(y):
        raise argparse.ArgumentTypeError("offset values must be finite")
    return (x, y)


def _format_offset(value: float) -> str:
    return "0" if value == 0.0 else format(value, ".9g")


def _find_executable(build_dir: pathlib.Path, name: str) -> pathlib.Path:
    candidates = tuple(
        directory / (name + suffix)
        for directory in (build_dir / "tools" / "aa_poc", build_dir)
        for suffix in ("", ".exe")
    )
    for candidate in candidates:
        if candidate.is_file() and os.access(candidate, os.X_OK):
            return candidate.resolve()
    locations = ", ".join(str(candidate) for candidate in candidates)
    raise ReportError(f"cannot find executable {name}; checked {locations}")


def _run_renderer(
    method: Method,
    executable: pathlib.Path,
    output_dir: pathlib.Path,
    offset: Tuple[float, float],
    timeout: float,
) -> Tuple[pathlib.Path, dict]:
    command = [str(executable), *method.comparison_args]
    command.extend(
        (
            "--output-dir",
            str(output_dir),
            "--offset-x",
            _format_offset(offset[0]),
            "--offset-y",
            _format_offset(offset[1]),
        )
    )
    output_dir.mkdir(parents=True)
    try:
        completed = subprocess.run(
            command,
            check=False,
            capture_output=True,
            text=True,
            timeout=timeout,
        )
    except subprocess.TimeoutExpired as error:
        raise ReportError(
            f"{method.label} timed out after {timeout:g} seconds: {shlex.join(command)}"
        ) from error
    except OSError as error:
        raise ReportError(f"cannot execute {executable}: {error}") from error
    record = {
        "method": method.key,
        "command": command,
        "returncode": completed.returncode,
        "stdout": completed.stdout,
        "stderr": completed.stderr,
    }
    if completed.returncode != 0:
        details = completed.stderr.strip() or completed.stdout.strip() or "no diagnostic output"
        raise ReportError(
            f"{method.label} failed with exit code {completed.returncode}:\n"
            f"  {shlex.join(command)}\n{details}"
        )
    result = output_dir / method.output_name
    if not result.is_file():
        raise ReportError(
            f"{method.label} succeeded but did not write expected image {result}"
        )
    return result, record


def _relative_url(path: pathlib.Path) -> str:
    return path.as_posix()


def _format_psnr(value: float) -> str:
    return "∞" if math.isinf(value) else f"{value:.3f} dB"


def _metric_dict(metrics: Metrics) -> dict:
    return {
        "ssim": metrics.ssim,
        "psnr_db": "infinity" if math.isinf(metrics.psnr_db) else metrics.psnr_db,
        "mean_absolute_error_8bit": metrics.mean_absolute_error,
        "max_absolute_error_8bit": metrics.max_absolute_error,
    }


def _comparison_guide_html(diff_gain: float) -> str:
    """Return the shared, visible guide used by every report page."""

    return f"""
    <section class="comparison-guide" id="comparison-guide" aria-labelledby="comparison-guide-heading">
      <p class="eyebrow">How to read the report</p>
      <h2 id="comparison-guide-heading">Quick guide</h2>
      <article class="guide-offset">
        <h3>Offset (tiny move)</h3>
        <p>We nudge the shape a fraction of a pixel to see whether its edge quality stays steady.</p>
      </article>
      <div class="guide-grid">
        <article>
          <h3>View modes</h3>
          <dl class="guide-list">
            <div><dt>Result</dt><dd>What the method actually drew.</dd></div>
            <div><dt>Difference</dt><dd>A heat map of disagreements with SSAA8. Black means the same; colors moving from blue toward red and white mean a bigger mismatch. Like turning up contrast, the {diff_gain:g}× gain brightens tiny differences but does not change the scores.</dd></div>
            <div><dt>Blink</dt><dd>Like flipping between two drawings. Flicker reveals what changed.</dd></div>
          </dl>
        </article>
        <article>
          <h3>Scores and reference</h3>
          <dl class="guide-list">
            <div><dt>MSAA 4×</dt><dd>Checks four spots inside each pixel, then blends them into one edge.</dd></div>
            <div><dt>SSIM</dt><dd>A look-alike score. Closer to 1 means the shapes look more alike.</dd></div>
            <div><dt>PSNR</dt><dd>A pixel-error score. Higher means less error; ∞ means an exact match.</dd></div>
            <div><dt>SSAA8</dt><dd>Our high-detail yardstick: render bigger, then shrink. Useful, but not perfect truth.</dd></div>
          </dl>
        </article>
      </div>
    </section>
    """


def _comparison_guide_css() -> str:
    """Return styles shared by the guide on overview and detail pages."""

    return """
    .comparison-guide { margin:0 0 46px; padding:24px; background:var(--panel); border:1px solid var(--line); border-radius:14px; }
    .comparison-guide h2 { margin:0 0 16px; }
    .guide-offset { margin-bottom:14px; padding:17px 18px; background:var(--panel2); border:1px solid var(--accent); border-radius:11px; }
    .guide-offset h3 { margin:0 0 5px; color:var(--accent); }
    .guide-offset p { margin:0; color:#d8e2ee; }
    .guide-grid { display:grid; grid-template-columns:repeat(auto-fit,minmax(min(100%,460px),1fr)); gap:14px; }
    .guide-grid article { padding:17px 18px; background:var(--panel2); border:1px solid var(--line); border-radius:11px; }
    .guide-grid h3 { margin:0 0 12px; }
    .guide-list { margin:0; }
    .guide-list div+div { margin-top:12px; padding-top:12px; border-top:1px solid var(--line); }
    .guide-list dt { color:var(--accent); font-weight:800; }
    .guide-list dd { margin:3px 0 0; color:#c3ccd8; }
    """


def _viewport_accessibility_attributes(
    method_label: str,
    offset_x: float,
    offset_y: float,
    *,
    reference_card: bool,
    characteristic_label: Optional[str] = None,
) -> str:
    """Return the accessible name and data used to keep it in sync with view mode."""

    context_parts = []
    if characteristic_label is not None:
        context_parts.append(f"characteristic {characteristic_label}")
    context_parts.append(
        f"offset ({_format_offset(offset_x)}, {_format_offset(offset_y)}) px"
    )
    context = ", ".join(context_parts)
    initial_view = "Reference result" if reference_card else "Candidate result"
    label = f"{method_label}. {initial_view}. {context}."
    return (
        'role="img" '
        f'aria-label="{html.escape(label, quote=True)}" '
        f'data-method-label="{html.escape(method_label, quote=True)}" '
        f'data-context="{html.escape(context, quote=True)}" '
        f'data-reference-card="{str(reference_card).lower()}"'
    )


def _viewport_label_script() -> str:
    """Return JavaScript that updates composite viewport names with active mode."""

    return f"""
      function updateViewportLabels(mode) {{
        document.querySelectorAll('.image-viewport').forEach(viewport => {{
          const descriptions = viewport.dataset.referenceCard === 'true' ? {{
            original: 'Reference result',
            diff: 'Difference view: exact agreement, shown as black',
            blink: 'Blink view: SSAA8 in both frames'
          }} : {{
            original: 'Candidate result',
            diff: 'Difference view against SSAA8',
            blink: 'Blink view alternating candidate and SSAA8 every {BLINK_INTERVAL_MS} ms'
          }};
          viewport.setAttribute('aria-label', viewport.dataset.methodLabel + '. ' + descriptions[mode] + '. ' + viewport.dataset.context + '.');
        }});
      }}
    """


def render_report_html(
    title: str,
    generated_at: str,
    characteristics: Optional[Sequence[Characteristic]] = None,
) -> str:
    """Return a simple offline index for the eight characteristic pages."""

    if characteristics is None:
        characteristics = CHARACTERISTICS
    characteristic_links = []
    for index, characteristic in enumerate(characteristics):
        characteristic_links.append(
            f"""
        <a class="characteristic-link" href="pages/{html.escape(characteristic.key)}.html">
          <span class="characteristic-number">{index + 1:02d}</span>
          <strong>{html.escape(characteristic.label)}</strong>
          <span>{html.escape(characteristic.description)}</span>
        </a>
            """
        )

    escaped_title = html.escape(title)
    return f"""<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>{escaped_title}</title>
  <style>
    :root {{ color-scheme:dark; --bg:#0c1017; --panel:#151b25; --panel2:#1c2431; --ink:#f4f7fb; --muted:#9aa8ba; --line:#303b4a; --accent:#61d9c8; }}
    * {{ box-sizing:border-box; }}
    html {{ background:var(--bg); color:var(--ink); font:15px/1.45 ui-sans-serif,system-ui,-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif; }}
    body {{ margin:0; }}
    .hero {{ padding:48px max(24px,calc((100vw - 1200px)/2)); background:radial-gradient(circle at 20% 0,#173c43 0,transparent 38%),linear-gradient(145deg,#121824,#0c1017); border-bottom:1px solid var(--line); }}
    .eyebrow {{ margin:0 0 8px; color:var(--accent); font-size:.75rem; font-weight:800; letter-spacing:.16em; text-transform:uppercase; }}
    h1 {{ margin:0; font-size:clamp(2rem,5vw,4rem); letter-spacing:-.045em; line-height:1; }}
    .lede {{ max-width:720px; margin:16px 0 10px; color:#c3ccd8; font-size:1.08rem; }}
    main {{ max-width:1200px; margin:auto; padding:34px 18px 72px; }}
    h2 {{ margin:0 0 18px; font-size:1.55rem; }}
    .summary-link {{ display:flex; align-items:center; justify-content:space-between; gap:18px; margin:0 0 30px; padding:20px; color:var(--ink); background:linear-gradient(135deg,#173c43,var(--panel)); border:1px solid #397f7a; border-radius:14px; text-decoration:none; }}
    .summary-link:hover {{ border-color:var(--accent); transform:translateY(-1px); }}
    .summary-link strong {{ display:block; font-size:1.15rem; }}
    .summary-link span {{ color:#c3ccd8; }}
    .summary-link b {{ color:var(--accent); font-size:1.25rem; }}
    .characteristic-links {{ display:grid; grid-template-columns:repeat(auto-fit,minmax(min(100%,260px),1fr)); gap:12px; }}
    .characteristic-link {{ display:grid; grid-template-columns:auto 1fr; gap:2px 12px; min-height:128px; padding:18px; color:var(--ink); background:linear-gradient(145deg,var(--panel2),var(--panel)); border:1px solid var(--line); border-radius:14px; text-decoration:none; }}
    .characteristic-link:hover {{ border-color:var(--accent); transform:translateY(-1px); }}
    .characteristic-number {{ grid-row:1/3; color:var(--accent); font-size:.72rem; font-weight:800; letter-spacing:.12em; }}
    .characteristic-link strong {{ font-size:1.05rem; }}
    .characteristic-link span:last-child {{ color:var(--muted); font-size:.87rem; }}
  </style>
</head>
<body>
  <header class="hero"><p class="eyebrow">ThorVG · visual quality study</p><h1>{escaped_title}</h1><p class="lede">Choose what you want to inspect. Each page compares five AA methods with the same high-detail yardstick.</p><small>Generated {html.escape(generated_at)}</small></header>
  <main aria-labelledby="characteristic-heading">
    <a class="summary-link" href="pages/summary.html"><span><strong>Overall summary</strong>Compare the five methods across the full scene and every visual test.</span><b aria-hidden="true">→</b></a>
    <p class="eyebrow">What to inspect</p>
    <h2 id="characteristic-heading">Eight visual characteristics</h2>
    <div class="characteristic-links">{''.join(characteristic_links)}</div>
  </main>
</body>
</html>
"""


def render_summary_html(
    title: str,
    generated_at: str,
    reference_path: pathlib.Path,
    reference_offset: Tuple[float, float],
    comparisons: Sequence[SummaryComparison],
    characteristic_summaries: Sequence[SummaryCharacteristic],
) -> str:
    """Return the all-shapes score summary with one reference scene picture."""

    overall_rows = "".join(
        f"""
          <tr><th scope="row">{html.escape(comparison.method.label)}</th><td>{comparison.overall_metrics.ssim:.6f}</td><td>{_format_psnr(comparison.overall_metrics.psnr_db)}</td></tr>
        """
        for comparison in comparisons
    )
    move_rows = "".join(
        f"""
          <tr><th scope="row">{html.escape(comparison.method.label)}</th><td>({_format_offset(move.offset_x)}, {_format_offset(move.offset_y)})</td><td>{move.metrics.ssim:.6f}</td><td>{_format_psnr(move.metrics.psnr_db)}</td></tr>
        """
        for comparison in comparisons
        for move in comparison.moves
    )
    characteristic_groups = []
    characteristic_move_rows = []
    for summary in characteristic_summaries:
        characteristic_rows = []
        for comparison_index, comparison in enumerate(summary.comparisons):
            scene_cell = ""
            if comparison_index == 0:
                scene_cell = (
                    f'<th class="scene-name" scope="rowgroup" '
                    f'rowspan="{len(summary.comparisons)}"><a href="{html.escape(summary.characteristic.key)}.html">'
                    f'{html.escape(summary.characteristic.label)}</a></th>'
                )
            characteristic_rows.append(
                f"""
          <tr>{scene_cell}<th class="method-name" scope="row">{html.escape(comparison.method.label)}</th><td>{comparison.overall_metrics.ssim:.6f}</td><td>{_format_psnr(comparison.overall_metrics.psnr_db)}</td></tr>
                """
            )
            for move in comparison.moves:
                characteristic_move_rows.append(
                    f"""
          <tr><th scope="row"><a href="{html.escape(summary.characteristic.key)}.html">{html.escape(summary.characteristic.label)}</a></th><td>{html.escape(comparison.method.label)}</td><td>({_format_offset(move.offset_x)}, {_format_offset(move.offset_y)})</td><td>{move.metrics.ssim:.6f}</td><td>{_format_psnr(move.metrics.psnr_db)}</td></tr>
                    """
                )
        characteristic_groups.append(f"<tbody>{''.join(characteristic_rows)}</tbody>")
    offset_label = (
        f"({_format_offset(reference_offset[0])}, "
        f"{_format_offset(reference_offset[1])})"
    )
    escaped_title = html.escape(title)
    return f"""<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Overall summary · {escaped_title}</title>
  <style>
    :root {{ color-scheme:dark; --bg:#0c1017; --panel:#151b25; --panel2:#1c2431; --ink:#f4f7fb; --muted:#9aa8ba; --line:#303b4a; --accent:#61d9c8; }}
    * {{ box-sizing:border-box; }}
    html {{ background:var(--bg); color:var(--ink); font:15px/1.5 ui-sans-serif,system-ui,-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif; }}
    body {{ margin:0; }}
    header,main {{ width:min(1120px,calc(100% - 32px)); margin:auto; }}
    header {{ padding:34px 0 24px; }}
    main {{ padding-bottom:72px; }}
    a {{ color:var(--accent); }}
    h1 {{ margin:12px 0 8px; font-size:clamp(2rem,5vw,3.6rem); letter-spacing:-.04em; line-height:1; }}
    h2 {{ margin:0 0 12px; }}
    .lede,.note,figcaption {{ color:var(--muted); }}
    .lede {{ max-width:760px; margin:0; font-size:1.05rem; }}
    .layout {{ display:grid; grid-template-columns:minmax(0,1.35fr) minmax(300px,.8fr); gap:22px; align-items:start; }}
    .panel {{ padding:20px; background:linear-gradient(145deg,var(--panel2),var(--panel)); border:1px solid var(--line); border-radius:15px; }}
    figure {{ margin:0; }}
    figure img {{ display:block; width:100%; height:auto; margin-top:12px; background:white; border:1px solid var(--line); border-radius:10px; }}
    figcaption {{ margin-top:8px; font-size:.85rem; }}
    table {{ width:100%; border-collapse:collapse; font-variant-numeric:tabular-nums; }}
    th,td {{ padding:10px 9px; border-bottom:1px solid var(--line); text-align:right; }}
    th:first-child {{ text-align:left; }}
    thead th {{ color:var(--muted); font-size:.75rem; letter-spacing:.08em; text-transform:uppercase; }}
    tbody tr:last-child th,tbody tr:last-child td {{ border-bottom:0; }}
    .scene-table {{ margin-top:10px; }}
    .scene-table tbody + tbody tr:first-child > * {{ border-top:2px solid #465567; }}
    .scene-name,.method-name {{ text-align:left; }}
    .scene-name {{ width:30%; vertical-align:top; padding-top:13px; }}
    .table-panel {{ margin-top:22px; overflow-x:auto; }}
    .table-intro {{ margin:0 0 12px; color:var(--muted); }}
    .definitions {{ display:grid; gap:7px; margin:16px 0 0; color:#c3ccd8; font-size:.9rem; }}
    .definitions p {{ margin:0; }}
    details {{ margin-top:22px; }}
    summary {{ cursor:pointer; color:var(--accent); font-weight:700; }}
    .move-table {{ margin-top:14px; }}
    @media (max-width:760px) {{ .layout {{ grid-template-columns:1fr; }} }}
  </style>
</head>
<body>
  <header><a href="../report.html">← Eight visual characteristics</a><h1>Overall summary</h1><p class="lede">All eight shapes are scored together. SSAA8 is the yardstick; higher SSIM and PSNR mean a method stayed closer to it.</p></header>
  <main>
    <div class="layout">
      <figure class="panel"><h2>The full test scene</h2><p class="note">Only the reference picture is shown here so you can see what was measured.</p><img src="../{_relative_url(reference_path)}" alt="SSAA8 reference showing all eight anti-aliasing test shapes"><figcaption>SSAA8 reference at tiny move {html.escape(offset_label)}.</figcaption></figure>
      <section class="panel" aria-labelledby="score-heading"><h2 id="score-heading">General comparison</h2><table><thead><tr><th scope="col">Method</th><th scope="col">SSIM</th><th scope="col">PSNR</th></tr></thead><tbody>{overall_rows}</tbody></table><div class="definitions"><p><strong>SSIM</strong> is the look-alike score. Closer to 1 is better.</p><p><strong>PSNR</strong> is the pixel-error score. Higher is better; ∞ means an exact match.</p><p>Each value combines every tiny move. Empty background outside the shapes is ignored.</p></div></section>
    </div>
    <section class="panel table-panel" aria-labelledby="visual-test-heading"><h2 id="visual-test-heading">Comparison by visual test</h2><p class="table-intro">Each row combines all tiny moves for one visual test. Select a test name to inspect its pictures.</p><table class="scene-table"><thead><tr><th scope="col">Visual test</th><th scope="col">Method</th><th scope="col">SSIM</th><th scope="col">PSNR</th></tr></thead>{''.join(characteristic_groups)}</table></section>
    <details class="panel"><summary>Full-scene values for each tiny move</summary><table class="move-table"><thead><tr><th scope="col">Method</th><th scope="col">Tiny move (x, y)</th><th scope="col">SSIM</th><th scope="col">PSNR</th></tr></thead><tbody>{move_rows}</tbody></table></details>
    <details class="panel"><summary>Every visual-test value for each tiny move</summary><table class="move-table"><thead><tr><th scope="col">Visual test</th><th scope="col">Method</th><th scope="col">Tiny move (x, y)</th><th scope="col">SSIM</th><th scope="col">PSNR</th></tr></thead><tbody>{''.join(characteristic_move_rows)}</tbody></table></details>
    <p class="note">Generated {html.escape(generated_at)}</p>
  </main>
</body>
</html>
"""


def render_characteristic_html(
    title: str,
    characteristic: Characteristic,
    cases: Sequence[CharacteristicCase],
    diff_gain: float,
    generated_at: str,
    previous_characteristic: Characteristic,
    next_characteristic: Characteristic,
) -> str:
    """Return one offline page for close inspection of a scene characteristic."""

    sections = []
    for case_index, case in enumerate(cases):
        cards = [
            f"""
          <article class="method-card reference-card">
            <header><div><h3>{html.escape(REFERENCE.label)}</h3><p>{html.escape(REFERENCE.description)}</p></div><span class="reference-badge">reference</span></header>
            <div class="image-viewport" tabindex="0" {_viewport_accessibility_attributes(REFERENCE.label, case.offset_x, case.offset_y, reference_card=True, characteristic_label=characteristic.label)}><div class="image-stage" data-width="{case.width}" data-height="{case.height}" data-reference="true">
              <img class="layer reference-layer" src="../{_relative_url(case.reference_path)}" alt="" aria-hidden="true">
              <img class="layer candidate-layer" src="../{_relative_url(case.reference_path)}" alt="" aria-hidden="true">
              <img class="layer diff-layer" src="../{_relative_url(case.reference_diff_path)}" alt="" aria-hidden="true">
            </div></div>
            <dl class="metric-strip"><div><dt>SSIM</dt><dd>1.000000</dd></div><div><dt>PSNR</dt><dd>∞</dd></div></dl>
          </article>
            """
        ]
        for comparison in case.comparisons:
            metrics = comparison.roi_metrics
            cards.append(
                f"""
          <article class="method-card">
            <header><div><h3>{html.escape(comparison.method.label)}</h3><p>{html.escape(comparison.method.description)}</p></div></header>
            <div class="image-viewport" tabindex="0" {_viewport_accessibility_attributes(comparison.method.label, case.offset_x, case.offset_y, reference_card=False, characteristic_label=characteristic.label)}><div class="image-stage" data-width="{case.width}" data-height="{case.height}">
              <img class="layer reference-layer" src="../{_relative_url(case.reference_path)}" alt="" aria-hidden="true">
              <img class="layer candidate-layer" src="../{_relative_url(comparison.image_path)}" alt="" aria-hidden="true">
              <img class="layer diff-layer" src="../{_relative_url(comparison.diff_path)}" alt="" aria-hidden="true">
            </div></div>
            <dl class="metric-strip"><div><dt>SSIM</dt><dd>{metrics.ssim:.6f}</dd></div><div><dt>PSNR</dt><dd>{_format_psnr(metrics.psnr_db)}</dd></div></dl>
          </article>
                """
            )
        sections.append(
            f"""
      <section class="case" id="{html.escape(case.key)}" data-case="{case_index}">
        <div class="case-heading"><div><p class="eyebrow">Tiny move {case_index + 1} of {len(cases)}</p><h2>({_format_offset(case.offset_x)}, {_format_offset(case.offset_y)}) px</h2></div></div>
        <div class="cards">{''.join(cards)}</div>
      </section>
            """
        )

    escaped_title = html.escape(title)
    escaped_label = html.escape(characteristic.label)
    escaped_description = html.escape(characteristic.description)
    offset_links = "".join(
        f'<a href="#{html.escape(case.key)}">({_format_offset(case.offset_x)}, {_format_offset(case.offset_y)})</a>'
        for case in cases
    )
    return f"""<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>{escaped_label} · {escaped_title}</title>
  <style>
    :root {{ color-scheme:dark; --bg:#0c1017; --panel:#151b25; --panel2:#1c2431; --ink:#f4f7fb; --muted:#9aa8ba; --line:#303b4a; --accent:#61d9c8; --warm:#ffc96b; }}
    * {{ box-sizing:border-box; }} html {{ background:var(--bg); color:var(--ink); font:15px/1.45 ui-sans-serif,system-ui,-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif; }} body {{ margin:0; }}
    a {{ color:var(--accent); }} h1,h2,h3,p {{ margin-top:0; }}
    .hero {{ padding:34px max(24px,calc((100vw - 1680px)/2)); background:radial-gradient(circle at 20% 0,#173c43 0,transparent 38%),linear-gradient(145deg,#121824,#0c1017); border-bottom:1px solid var(--line); }}
    .breadcrumb {{ display:inline-block; margin-bottom:20px; text-decoration:none; }}
    .eyebrow {{ margin:0 0 7px; color:var(--accent); font-size:.74rem; font-weight:800; letter-spacing:.16em; text-transform:uppercase; }}
    h1 {{ margin-bottom:10px; font-size:clamp(2rem,5vw,4rem); letter-spacing:-.045em; line-height:1; }} .lede {{ max-width:850px; margin-bottom:10px; color:#c3ccd8; font-size:1.08rem; }}
    .page-nav {{ display:flex; flex-wrap:wrap; justify-content:space-between; gap:12px; max-width:1680px; margin:auto; padding:14px 18px; }} .page-nav a,.offset-nav a {{ padding:8px 11px; background:var(--panel); border:1px solid var(--line); border-radius:9px; text-decoration:none; }} .offset-nav {{ display:flex; flex-wrap:wrap; gap:7px; width:100%; padding-top:3px; }} .offset-nav::before {{ content:"Tiny moves"; align-self:center; margin-right:3px; color:var(--muted); font-size:.72rem; font-weight:800; letter-spacing:.1em; text-transform:uppercase; }}
    .controls {{ position:sticky; z-index:20; top:0; display:flex; flex-wrap:wrap; gap:14px 24px; align-items:center; padding:12px max(18px,calc((100vw - 1680px)/2)); background:rgba(12,16,23,.94); border-block:1px solid var(--line); backdrop-filter:blur(14px); }}
    .control {{ display:flex; gap:9px; align-items:center; color:var(--muted); }} .control strong {{ min-width:3.5em; color:var(--ink); }} input[type=range] {{ accent-color:var(--accent); }}
    .modes {{ display:flex; gap:4px; padding:3px; background:#080b10; border:1px solid var(--line); border-radius:10px; }} button {{ appearance:none; padding:7px 11px; color:var(--muted); background:transparent; border:0; border-radius:7px; font:inherit; cursor:pointer; }} button:hover,button[aria-pressed=true] {{ color:var(--ink); background:var(--panel2); }}
    main {{ max-width:1680px; margin:auto; padding:25px 18px 72px; }} .case {{ margin-bottom:58px; scroll-margin-top:90px; }} .case-heading {{ display:flex; justify-content:space-between; gap:24px; align-items:end; margin:0 3px 15px; }} h2 {{ margin-bottom:0; }} .case-heading>p {{ max-width:780px; margin:0; color:var(--muted); text-align:right; font-variant-numeric:tabular-nums; }}
    .cards {{ display:grid; grid-template-columns:repeat(2,minmax(0,1fr)); gap:16px; }} .method-card {{ min-width:0; overflow:hidden; background:var(--panel); border:1px solid var(--line); border-radius:14px; box-shadow:0 14px 38px rgba(0,0,0,.15); }} .method-card>header {{ min-height:86px; display:flex; justify-content:space-between; gap:12px; padding:15px 17px 10px; }} .method-card h3 {{ margin:0 0 3px; font-size:1.08rem; }} .method-card header p {{ margin:0; color:var(--muted); font-size:.83rem; }} .reference-badge {{ align-self:start; padding:3px 7px; color:#10151d; background:var(--warm); border-radius:99px; font-size:.7rem; font-weight:800; text-transform:uppercase; }}
    .image-viewport {{ width:100%; height:560px; overflow:auto; background:#090c11; border-block:1px solid var(--line); overscroll-behavior:contain; }} .image-viewport:focus-visible {{ outline:2px solid var(--accent); outline-offset:-2px; }} .image-stage {{ position:relative; width:1px; height:1px; transform-origin:top left; }} .layer {{ position:absolute; inset:0 auto auto 0; display:block; max-width:none; image-rendering:pixelated; user-select:none; -webkit-user-drag:none; }} .reference-layer,.diff-layer {{ opacity:0; }}
    body[data-mode=diff] .candidate-layer,body[data-mode=diff] .reference-layer {{ opacity:0; }} body[data-mode=diff] .diff-layer {{ opacity:1; }} body[data-mode=blink][data-blink=reference] .candidate-layer {{ opacity:0; }} body[data-mode=blink][data-blink=reference] .reference-layer {{ opacity:1; }}
    .metric-strip {{ display:flex; gap:28px; margin:0; padding:12px 17px 14px; font-variant-numeric:tabular-nums; }} .metric-strip div {{ display:grid; gap:1px; }} .metric-strip dt {{ color:var(--muted); font-size:.68rem; font-weight:800; letter-spacing:.08em; text-transform:uppercase; }} .metric-strip dd {{ margin:0; font-weight:700; }}
{_comparison_guide_css()}
    @media (max-width:900px) {{ .cards {{ grid-template-columns:1fr; }} .case-heading {{ display:block; }} .case-heading>p {{ margin-top:6px; text-align:left; }} }} @media (max-width:600px) {{ .image-viewport {{ height:430px; }} }}
  </style>
</head>
<body data-mode="original" data-blink="candidate">
  <header class="hero"><a class="breadcrumb" href="../report.html">← Overview</a><p class="eyebrow">Characteristic detail</p><h1>{escaped_label}</h1><p class="lede">{escaped_description} Compare methods at the same tiny move, and compare scores only on this page.</p><small>Generated {html.escape(generated_at)}</small></header>
  <nav class="page-nav" aria-label="Characteristic pages"><a rel="prev" href="{html.escape(previous_characteristic.key)}.html">← {html.escape(previous_characteristic.label)}</a><a href="../report.html">All characteristics</a><a rel="next" href="{html.escape(next_characteristic.key)}.html">{html.escape(next_characteristic.label)} →</a><div class="offset-nav" aria-label="Tiny move sections">{offset_links}</div></nav>
  <div class="controls" role="group" aria-label="Comparison controls" aria-describedby="comparison-guide">
    <div class="modes" aria-label="View mode"><button type="button" data-mode-button="original" aria-pressed="true">Result</button><button type="button" data-mode-button="diff" aria-pressed="false">Difference</button><button type="button" data-mode-button="blink" aria-pressed="false">Blink</button></div>
    <label class="control">Zoom <input id="zoom" type="range" min="1" max="16" step="1" value="4"><strong id="zoom-value">4×</strong></label>
  </div>
  <main>{_comparison_guide_html(diff_gain)}{''.join(sections)}</main>
  <script>
    (() => {{
      const body=document.body,zoom=document.querySelector('#zoom'),zoomValue=document.querySelector('#zoom-value'); let currentZoom=4,syncing=false,blinkTimer=null;
{_viewport_label_script()}
      function setMode(mode) {{ body.dataset.mode=mode; document.querySelectorAll('[data-mode-button]').forEach(button=>button.setAttribute('aria-pressed',String(button.dataset.modeButton===mode))); updateViewportLabels(mode); if(blinkTimer){{clearInterval(blinkTimer);blinkTimer=null;}} body.dataset.blink='candidate'; if(mode==='blink')blinkTimer=setInterval(()=>{{body.dataset.blink=body.dataset.blink==='candidate'?'reference':'candidate';}},{BLINK_INTERVAL_MS}); }}
      function setZoom(nextZoom) {{ nextZoom=Number(nextZoom); document.querySelectorAll('.case').forEach(section=>{{ const viewports=[...section.querySelectorAll('.image-viewport')]; if(!viewports.length)return; const anchor=viewports[0],centerX=(anchor.scrollLeft+anchor.clientWidth/2)/currentZoom,centerY=(anchor.scrollTop+anchor.clientHeight/2)/currentZoom; section.querySelectorAll('.image-stage').forEach(stage=>{{const width=Number(stage.dataset.width)*nextZoom,height=Number(stage.dataset.height)*nextZoom;stage.style.width=width+'px';stage.style.height=height+'px';stage.querySelectorAll('img').forEach(image=>{{image.style.width=width+'px';image.style.height=height+'px';}});}}); requestAnimationFrame(()=>viewports.forEach(viewport=>{{viewport.scrollLeft=centerX*nextZoom-viewport.clientWidth/2;viewport.scrollTop=centerY*nextZoom-viewport.clientHeight/2;}})); }}); currentZoom=nextZoom;zoom.value=String(nextZoom);zoomValue.textContent=nextZoom+'×'; }}
      document.querySelectorAll('[data-mode-button]').forEach(button=>button.addEventListener('click',()=>setMode(button.dataset.modeButton))); zoom.addEventListener('input',()=>setZoom(zoom.value)); document.querySelectorAll('.case').forEach(section=>section.querySelectorAll('.image-viewport').forEach(viewport=>viewport.addEventListener('scroll',()=>{{if(syncing)return;syncing=true;section.querySelectorAll('.image-viewport').forEach(peer=>{{if(peer!==viewport){{peer.scrollLeft=viewport.scrollLeft;peer.scrollTop=viewport.scrollTop;}}}});syncing=false;}}))); document.addEventListener('keydown',event=>{{const modes={{'1':'original','2':'diff','3':'blink'}};if(modes[event.key])setMode(modes[event.key]);if(event.key==='+'||event.key==='=')setZoom(Math.min(16,currentZoom+1));if(event.key==='-'||event.key==='_')setZoom(Math.max(1,currentZoom-1));}}); setZoom(4);
    }})();
  </script>
</body>
</html>
"""


def _prepare_output_directory(output_dir: pathlib.Path) -> None:
    if output_dir.exists():
        if not output_dir.is_dir():
            raise ReportError(f"output path is not a directory: {output_dir}")
        try:
            if next(output_dir.iterdir(), None) is not None:
                raise ReportError(
                    f"output directory is not empty: {output_dir}; choose a new or empty directory"
                )
        except OSError as error:
            raise ReportError(f"cannot inspect output directory {output_dir}: {error}") from error
    else:
        try:
            output_dir.mkdir(parents=True)
        except OSError as error:
            raise ReportError(f"cannot create output directory {output_dir}: {error}") from error


def _roi_dict(roi: Tuple[int, int, int, int]) -> dict:
    x0, y0, x1, y1 = roi
    return {"x": x0, "y": y0, "width": x1 - x0, "height": y1 - y0}


def _generate_report_in_directory(
    args: argparse.Namespace, build_dir: pathlib.Path, output_dir: pathlib.Path
) -> pathlib.Path:
    """Generate a complete report inside an already-created staging directory."""

    offsets = args.offset if args.offset else list(DEFAULT_OFFSETS)
    unique_offsets = []
    for offset in offsets:
        if offset not in unique_offsets:
            unique_offsets.append(offset)
    executables = {
        method.key: _find_executable(build_dir, method.executable)
        for method in METHODS + (REFERENCE,)
    }
    command_records = []
    rendered_cases = []
    images_root = output_dir / "images"
    diffs_root = output_dir / "diffs"
    images_root.mkdir()
    diffs_root.mkdir()

    with tempfile.TemporaryDirectory(prefix="thorvg-aa-report-") as temporary:
        temporary_root = pathlib.Path(temporary)
        for case_index, offset in enumerate(unique_offsets):
            case_key = f"offset-{case_index:02d}"
            print(
                f"Rendering case {case_index + 1}/{len(unique_offsets)} at "
                f"({_format_offset(offset[0])}, {_format_offset(offset[1])})"
            )
            source_paths = {}
            reference_source, record = _run_renderer(
                REFERENCE,
                executables[REFERENCE.key],
                temporary_root / case_key / REFERENCE.key,
                offset,
                args.timeout,
            )
            source_paths[REFERENCE.key] = reference_source
            record["case"] = case_key
            command_records.append(record)
            for method in METHODS:
                source, record = _run_renderer(
                    method,
                    executables[method.key],
                    temporary_root / case_key / method.key,
                    offset,
                    args.timeout,
                )
                source_paths[method.key] = source
                record["case"] = case_key
                command_records.append(record)

            decoded = {key: decode_png(path) for key, path in source_paths.items()}
            width, height = validate_same_size(decoded.values())
            if (width, height) != (SCENE_WIDTH, SCENE_HEIGHT):
                raise ReportError(
                    "shared comparison renderers must all produce "
                    f"{SCENE_WIDTH}x{SCENE_HEIGHT} images; got {width}x{height}"
                )
            rendered_cases.append(
                {
                    "key": case_key,
                    "offset": offset,
                    "decoded": decoded,
                }
            )

    generated_at = _datetime.datetime.now(_datetime.timezone.utc).replace(microsecond=0).isoformat()
    pages_root = output_dir / "pages"
    characteristic_images_root = images_root / "characteristics"
    characteristic_diffs_root = diffs_root / "characteristics"
    summary_images_root = images_root / "summary"
    pages_root.mkdir()
    characteristic_images_root.mkdir()
    characteristic_diffs_root.mkdir()
    summary_images_root.mkdir()
    all_images = tuple(
        case["decoded"][method.key]
        for case in rendered_cases
        for method in METHODS + (REFERENCE,)
    )
    summary_roi = common_content_roi(all_images, padding=args.roi_padding)
    summary_reference_destination = summary_images_root / REFERENCE.output_name
    write_png_image(
        summary_reference_destination,
        rendered_cases[0]["decoded"][REFERENCE.key],
    )
    summary_comparisons = []
    manifest_summary_comparisons = []
    for method in METHODS:
        moves = []
        manifest_moves = []
        for rendered_case in rendered_cases:
            offset = rendered_case["offset"]
            move_metrics = compute_metrics(
                rendered_case["decoded"][method.key],
                rendered_case["decoded"][REFERENCE.key],
                summary_roi,
            )
            move = SummaryMove(
                key=rendered_case["key"],
                offset_x=offset[0],
                offset_y=offset[1],
                metrics=move_metrics,
            )
            moves.append(move)
            manifest_moves.append(
                {
                    "key": move.key,
                    "offset": {"x": move.offset_x, "y": move.offset_y},
                    "metrics": _metric_dict(move.metrics),
                }
            )
        overall_metrics = combine_metrics([move.metrics for move in moves])
        summary_comparison = SummaryComparison(
            method=method,
            overall_metrics=overall_metrics,
            moves=tuple(moves),
        )
        summary_comparisons.append(summary_comparison)
        manifest_summary_comparisons.append(
            {
                "method": method.key,
                "overall_metrics": _metric_dict(overall_metrics),
                "moves": manifest_moves,
            }
        )
    summary_page_relative = pathlib.Path("pages") / "summary.html"
    summary_reference_relative = summary_reference_destination.relative_to(output_dir)
    manifest_summary = {
        "page": _relative_url(summary_page_relative),
        "reference_image": _relative_url(summary_reference_relative),
        "reference_offset": {
            "x": rendered_cases[0]["offset"][0],
            "y": rendered_cases[0]["offset"][1],
        },
        "dimensions": {"width": SCENE_WIDTH, "height": SCENE_HEIGHT},
        "metric_roi": _roi_dict(summary_roi),
        "comparisons": manifest_summary_comparisons,
    }
    visual_padding = max(12, args.roi_padding)
    manifest_characteristics = []
    summary_characteristics = []
    for characteristic_index, characteristic in enumerate(CHARACTERISTICS):
        # A missing reference shape means the named scene contract has changed.
        # Candidate output is intentionally not required: a fully missing or
        # broken candidate remains measurable and visible in the report.
        for rendered_case in rendered_cases:
            try:
                content_roi_within(
                    (rendered_case["decoded"][REFERENCE.key],),
                    characteristic.search_roi,
                    padding=0,
                )
            except ReportError as error:
                raise ReportError(
                    f"SSAA8 reference has no {characteristic.label} content in "
                    f"{rendered_case['key']}"
                ) from error
        visual_crop = content_roi_within(
            all_images, characteristic.search_roi, padding=visual_padding
        )
        metric_roi = content_roi_within(
            all_images, characteristic.search_roi, padding=args.roi_padding
        )
        crop_width = visual_crop[2] - visual_crop[0]
        crop_height = visual_crop[3] - visual_crop[1]
        characteristic_image_root = characteristic_images_root / characteristic.key
        characteristic_diff_root = characteristic_diffs_root / characteristic.key
        characteristic_image_root.mkdir()
        characteristic_diff_root.mkdir()
        detail_cases = []
        manifest_detail_cases = []
        summary_moves_by_method = {method.key: [] for method in METHODS}
        for rendered_case in rendered_cases:
            case_key = rendered_case["key"]
            offset = rendered_case["offset"]
            decoded = rendered_case["decoded"]
            case_image_dir = characteristic_image_root / case_key
            case_diff_dir = characteristic_diff_root / case_key
            case_image_dir.mkdir()
            case_diff_dir.mkdir()
            cropped = {
                method.key: crop_image(decoded[method.key], visual_crop)
                for method in METHODS + (REFERENCE,)
            }
            reference_destination = case_image_dir / REFERENCE.output_name
            write_png_image(reference_destination, cropped[REFERENCE.key])
            reference_diff = case_diff_dir / REFERENCE.output_name
            write_difference_map(
                reference_diff,
                cropped[REFERENCE.key],
                cropped[REFERENCE.key],
                args.diff_gain,
            )
            comparisons = []
            manifest_comparisons = []
            for method in METHODS:
                image_destination = case_image_dir / method.output_name
                write_png_image(image_destination, cropped[method.key])
                diff_destination = case_diff_dir / method.output_name
                write_difference_map(
                    diff_destination,
                    cropped[method.key],
                    cropped[REFERENCE.key],
                    args.diff_gain,
                )
                characteristic_metrics = compute_metrics(
                    decoded[method.key], decoded[REFERENCE.key], metric_roi
                )
                summary_moves_by_method[method.key].append(
                    SummaryMove(
                        key=case_key,
                        offset_x=offset[0],
                        offset_y=offset[1],
                        metrics=characteristic_metrics,
                    )
                )
                visual_crop_metrics = compute_metrics(
                    decoded[method.key], decoded[REFERENCE.key], visual_crop
                )
                comparison = Comparison(
                    method=method,
                    image_path=image_destination.relative_to(output_dir),
                    diff_path=diff_destination.relative_to(output_dir),
                    roi_metrics=characteristic_metrics,
                    full_metrics=visual_crop_metrics,
                )
                comparisons.append(comparison)
                manifest_comparisons.append(
                    {
                        "method": method.key,
                        "image": _relative_url(comparison.image_path),
                        "difference_image": _relative_url(comparison.diff_path),
                        "characteristic_metrics": _metric_dict(characteristic_metrics),
                        "visual_crop_metrics": _metric_dict(visual_crop_metrics),
                    }
                )
            reference_relative = reference_destination.relative_to(output_dir)
            reference_diff_relative = reference_diff.relative_to(output_dir)
            detail_cases.append(
                CharacteristicCase(
                    key=case_key,
                    offset_x=offset[0],
                    offset_y=offset[1],
                    width=crop_width,
                    height=crop_height,
                    source_crop=visual_crop,
                    metric_roi=metric_roi,
                    reference_path=reference_relative,
                    reference_diff_path=reference_diff_relative,
                    comparisons=tuple(comparisons),
                )
            )
            manifest_detail_cases.append(
                {
                    "key": case_key,
                    "offset": {"x": offset[0], "y": offset[1]},
                    "dimensions": {"width": crop_width, "height": crop_height},
                    "reference": _relative_url(reference_relative),
                    "reference_difference_image": _relative_url(reference_diff_relative),
                    "comparisons": manifest_comparisons,
                }
            )
        summary_characteristics.append(
            SummaryCharacteristic(
                characteristic=characteristic,
                comparisons=tuple(
                    SummaryComparison(
                        method=method,
                        overall_metrics=combine_metrics(
                            [move.metrics for move in summary_moves_by_method[method.key]]
                        ),
                        moves=tuple(summary_moves_by_method[method.key]),
                    )
                    for method in METHODS
                ),
            )
        )
        page_relative = pathlib.Path("pages") / f"{characteristic.key}.html"
        previous_characteristic = CHARACTERISTICS[
            (characteristic_index - 1) % len(CHARACTERISTICS)
        ]
        next_characteristic = CHARACTERISTICS[
            (characteristic_index + 1) % len(CHARACTERISTICS)
        ]
        (output_dir / page_relative).write_text(
            render_characteristic_html(
                args.title,
                characteristic,
                detail_cases,
                args.diff_gain,
                generated_at,
                previous_characteristic,
                next_characteristic,
            ),
            encoding="utf-8",
        )
        manifest_characteristics.append(
            {
                "key": characteristic.key,
                "label": characteristic.label,
                "description": characteristic.description,
                "page": _relative_url(page_relative),
                "coordinate_convention": (
                    "x/y are the top-left source-image pixel; width/height define "
                    "half-open [x, x + width) x [y, y + height) bounds"
                ),
                "search_region": _roi_dict(characteristic.search_roi),
                "visual_crop": _roi_dict(visual_crop),
                "metric_roi": _roi_dict(metric_roi),
                "cases": manifest_detail_cases,
            }
        )

    manifest_summary["characteristics"] = [
        {
            "key": summary.characteristic.key,
            "label": summary.characteristic.label,
            "comparisons": [
                {
                    "method": comparison.method.key,
                    "overall_metrics": _metric_dict(comparison.overall_metrics),
                    "moves": [
                        {
                            "key": move.key,
                            "offset": {"x": move.offset_x, "y": move.offset_y},
                            "metrics": _metric_dict(move.metrics),
                        }
                        for move in comparison.moves
                    ],
                }
                for comparison in summary.comparisons
            ],
        }
        for summary in summary_characteristics
    ]
    (output_dir / summary_page_relative).write_text(
        render_summary_html(
            args.title,
            generated_at,
            summary_reference_relative,
            rendered_cases[0]["offset"],
            summary_comparisons,
            summary_characteristics,
        ),
        encoding="utf-8",
    )

    manifest = {
        "schema_version": 4,
        "title": args.title,
        "generated_at": generated_at,
        "reference": REFERENCE.key,
        "metric_conventions": {
            "sample_domain": "8-bit GL framebuffer RGB normalized without a transfer-function conversion",
            "alpha": "straight alpha composited over white before comparison",
            "psnr": "RGB, peak=255",
            "ssim": "Rec.709 luminance, valid 11x11 uniform windows, population moments, K1=0.01, K2=0.03, L=255",
            "roi": f"union of non-white pixels across every method and reference, padded by {args.roi_padding} pixels",
            "difference_gain": args.diff_gain,
            "characteristic_visual_padding": visual_padding,
            "characteristic_metric_padding": args.roi_padding,
        },
        "summary": manifest_summary,
        "characteristics": manifest_characteristics,
        "commands": command_records,
    }
    (output_dir / "report.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    log_lines = []
    for record in command_records:
        log_lines.append(f"[{record['case']}] {record['method']}")
        log_lines.append(f"$ {shlex.join(record['command'])}")
        log_lines.append(f"exit: {record['returncode']}")
        if record["stdout"]:
            log_lines.append("stdout:")
            log_lines.append(record["stdout"].rstrip())
        if record["stderr"]:
            log_lines.append("stderr:")
            log_lines.append(record["stderr"].rstrip())
        log_lines.append("")
    (output_dir / "run-log.txt").write_text("\n".join(log_lines), encoding="utf-8")
    report_path = output_dir / "report.html"
    report_path.write_text(
        render_report_html(
            args.title,
            generated_at,
            characteristics=CHARACTERISTICS,
        ),
        encoding="utf-8",
    )
    return report_path


def generate_report(args: argparse.Namespace) -> pathlib.Path:
    """Generate transactionally so renderer/report failures leave no partial report."""

    build_dir = pathlib.Path(args.build_dir).expanduser().resolve()
    requested_output = pathlib.Path(args.output_dir).expanduser().resolve()
    _prepare_output_directory(requested_output)
    staging_output = pathlib.Path(
        tempfile.mkdtemp(
            prefix=f".{requested_output.name}-staging-", dir=requested_output.parent
        )
    )
    try:
        _generate_report_in_directory(args, build_dir, staging_output)
        requested_output.rmdir()
        staging_output.rename(requested_output)
    except BaseException:
        shutil.rmtree(staging_output, ignore_errors=True)
        raise
    return requested_output / "report.html"


def build_argument_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Render all five AA methods on one scene and create an offline visual/metric report.",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument(
        "--build-dir",
        default="build-aa-poc",
        help="Meson build directory containing tools/aa_poc executables",
    )
    parser.add_argument(
        "--output-dir",
        default="aa-comparison-report",
        help="new or empty report directory",
    )
    parser.add_argument(
        "--offset",
        type=parse_offset,
        action="append",
        metavar="X,Y",
        help="subpixel offset to render; repeat for multiple cases (four representative offsets are used when omitted)",
    )
    parser.add_argument(
        "--timeout", type=float, default=180.0, help="timeout in seconds for each renderer invocation"
    )
    parser.add_argument(
        "--roi-padding", type=int, default=4, help="pixels added around the shared non-white metric ROI"
    )
    parser.add_argument(
        "--diff-gain", type=float, default=8.0, help="amplification applied to difference-map magnitudes"
    )
    parser.add_argument("--title", default="Anti-aliasing quality", help="report title")
    return parser


def main(argv: Optional[Sequence[str]] = None) -> int:
    parser = build_argument_parser()
    args = parser.parse_args(argv)
    if not math.isfinite(args.timeout) or args.timeout <= 0:
        parser.error("--timeout must be positive and finite")
    if args.roi_padding < 0:
        parser.error("--roi-padding cannot be negative")
    if not math.isfinite(args.diff_gain) or args.diff_gain <= 0:
        parser.error("--diff-gain must be positive and finite")
    try:
        report_path = generate_report(args)
    except (ReportError, OSError, ValueError) as error:
        print(f"generate_aa_report.py: error: {error}", file=sys.stderr)
        return 1
    print(f"Wrote {report_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
