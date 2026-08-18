# Curve-mask anti-aliasing POC

This experiment keeps ThorVG's normal GL tessellation and stencil fill for
inside/outside classification, but obtains boundary coverage from a transformed
copy of the original trimmed `LineTo` and `CubicTo` segments.

The renderer owns one reusable, full-surface, single-sample RGBA8 texture with
a depth/stencil attachment. For each solid shape it:

1. draws the normal tessellated fill into stencil;
2. stores binary interior classification in the mask's red channel;
3. draws conservative original-segment patches and stores nearest-boundary
   proximity in green using `GL_MAX`;
4. resolves the two channels to coverage on one bounds quad; and
5. applies premultiplied fill color and effective opacity once.

Line patches use exact screen-space point-to-segment distance. Cubic patches
use a control-point AABB expanded by half a pixel. The CPU fits an implicit
cubic polynomial, while the fragment shader evaluates `F`, `dFdx(F)`, and
`dFdy(F)` to approximate signed distance as `F / length(gradient(F))`. A
bounded closest-parameter search clips the infinite implicit curve to the
source segment's half-pixel band. Quadratics already reach the renderer as
degree-elevated cubics.

The two-channel resolve reconstructs the requested inner and outer halves:

```text
outside coverage = 0.5 * proximity
inside coverage  = 1.0 - 0.5 * proximity
```

It deliberately does not use `max(binaryInterior, boundaryCoverage)`.

## Running

Configure with `-Dengines=gl -Dtools=aa_poc`, then run:

```sh
meson compile -C build-aa-poc aa_curve_mask_poc aa_msaa4_poc aa_ssaa8_poc
./build-aa-poc/tools/aa_poc/aa_curve_mask_poc --output-dir /tmp/curve-mask
./build-aa-poc/tools/aa_poc/aa_msaa4_poc --scene curve-mask --output-dir /tmp/curve-mask-msaa4
./build-aa-poc/tools/aa_poc/aa_ssaa8_poc --scene curve-mask --output-dir /tmp/curve-mask-ssaa8
```

`--offset-grid` and `--motion` exercise subpixel stability.

## Comparing five AA methods

`generate_aa_report.py` renders MSAA 4x, flat-direct, curve-direct, flat-mask,
and curve-mask on one shared 800 x 480 scene, with SSAA8 as the high-sample
reference. The scene stays inside the common supported subset: normal solid
fills, closed single contours, line and cubic boundaries, and one translucent
paint. Each method receives exactly the same geometry, colors, canvas size,
and subpixel offset.

Build every candidate and the 8x supersampled reference, then generate the
offline report:

```sh
meson compile -C build-aa-poc \
  aa_flat_direct_poc aa_curve_direct_poc \
  aa_flat_mask_poc aa_curve_mask_poc \
  aa_msaa4_poc aa_ssaa8_poc
python3 tools/aa_poc/generate_aa_report.py \
  --build-dir build-aa-poc \
  --output-dir aa-comparison-report
```

Open `aa-comparison-report/report.html` for an overall summary or to choose one
of eight visual characteristics: diagonal edges, corners, circles, changing
curves, nearly horizontal edges, line/curve meetings, curve joins, and
half-transparent edges. The summary shows one full-scene SSAA8 reference image
and SSIM/PSNR tables for the full scene, each visual characteristic, and each
fractional-pixel position. Each detail page offers Result, Difference, and
Blink views at integer zoom.
The default run nudges each shape to four fractional-pixel positions to show
whether moving it changes the edge quality; repeat `--offset X,Y` to choose
other positions. `report.json` contains the machine-readable
per-characteristic measurements, and `run-log.txt` records every renderer
invocation.

SSIM and PSNR are measured against the SSAA8 box-resolved image. Each detail
page reuses one characteristic-specific region across every method and offset.
The region is the union of all non-white pixels plus four pixels of padding by
default, so method-specific stray pixels remain measurable while blank
background inflation is reduced. `report.json` documents the exact RGB, alpha,
SSIM, PSNR, and difference-map conventions; SSAA8 is a high-sample reference,
not analytic ground truth.

## Experiment results and limitations

The POC scene exercises a four-cubic circle, high curvature, an inflected S,
connected curves, a mixed line/cubic boundary, a sharp join, a concave curved
shape, NonZero and EvenOdd curved holes, a straight-line control, and a
50%-opaque curved shape.

The baseline scene renders with continuous inner/outer edge transitions. The
opaque connected patches do not show dark joins, and the 50%-opaque shape does
not accumulate alpha at patch overlaps because color is applied only by the
final quad. Both hole fill rules retain their binary stencil classification.

These results are only a feasibility signal. The implementation intentionally
keeps the following problems visible instead of adding production geometry:

- Each segment contributes an independent half-pixel band. Sharp joins can be
  rounded, clipped, or widened where conservative patches overlap.
- The sampled implicit fit and `F / |gradient(F)|` approximation are unstable
  near cusps, singular gradients, degenerate cubics, and some inflections.
- A broad cubic control AABB can shade many irrelevant fragments and can expose
  another branch of the implicit curve before the closest-parameter clip wins.
- Self-intersections and NonZero regions with winding magnitude greater than
  one can introduce internal-boundary or nearest-boundary errors.
- Line/cubic endpoints are resolved independently, so their distance fields
  need not agree exactly at a join.
- The RGBA8 mask quantizes coverage and consumes four bytes per full-surface
  pixel even though the POC uses only two channels.
- The extra render-to-texture pass, texture reads/writes, large conservative
  patches, and the fragment shader's bounded Newton search are deliberately
  expensive and have not been optimized.
- Only normal-blended solid fills are intercepted. Thin fills, strokes,
  gradients, non-normal blends, clip paths, and paths that continue drawing
  after `Close` use the existing GL path.
- Keeping a second transformed path increases per-shape POC memory and repeats
  path work that a production design would need to cache more selectively.

No performance conclusion should be drawn from this implementation.
