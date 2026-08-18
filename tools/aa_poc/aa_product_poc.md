# Product GL AA test harness

> **Testing only:** This is experimental instrumentation for comparing GL anti-aliasing candidates. It is not a supported ThorVG product API, does not change the default from MSAA4, and must not be used to select a production AA mode without a separate product decision.

The harness runs seven modes through the same `GlRenderer` path, checks the selected route and root sample count, produces an SSAA8 quality oracle, and records offscreen performance measurements. It supports macOS through CGL and Linux through EGL. The current setup has not been verified on Windows.

## Prerequisites

- A C++ toolchain, Meson, Ninja, and pkg-config.
- macOS with OpenGL, or Linux with OpenGL 3.3 and EGL development packages.
- Python 3 for the quality evaluator.
- Enough free space for the 336 quality candidates and their SSAA8 references.

Use a release build for performance measurements:

```sh
meson setup build-aa-poc-release \
  -Dbuildtype=release \
  -Dengines=gl \
  -Dtools=aa_poc \
  -Dloaders='' \
  -Dsavers='' \
  -Dbindings='' \
  -Dthreads=false \
  -Dpartial=false

meson compile -C build-aa-poc-release \
  aa_product_poc aa_size_diagnostic aa_aabb_diagnostic
```

If the build directory already exists, reconfigure it with `meson setup --reconfigure build-aa-poc-release` followed by the same options.

## Smoke test

Run this first to verify context creation, mode selection, root sampling, route assertions, malformed-path fallback, and the single-sample blend fallback:

```sh
AA_SMOKE_DIR="$(mktemp -d)"

./build-aa-poc-release/tools/aa_poc/aa_product_poc \
  --suite headline \
  --mode hybrid \
  --scene mixed-product-tile \
  --scale icon \
  --warmup 0 \
  --frames 1 \
  --repetitions 1 \
  --output-dir "$AA_SMOKE_DIR"
```

The output must report `root-samples=1`, `route-valid=1`, and `vsync=offscreen-no-swap`.

## Full quality matrix

```sh
AA_QUALITY_DIR="$PWD/aa-product-quality"

./build-aa-poc-release/tools/aa_poc/aa_product_poc \
  --suite quality \
  --mode all \
  --scene all \
  --scale all \
  --output-dir "$AA_QUALITY_DIR"

python3 tools/aa_poc/evaluate_aa_product.py \
  "$AA_QUALITY_DIR/quality-manifest.tsv" \
  --require-full-suite \
  --continue-valid-rows \
  --output-dir "$AA_QUALITY_DIR"
```

The evaluator intentionally returns a nonzero status when a candidate violates a route/sample invariant or visual gate. With the current implementation, FlatDirect and FlatMask have known mixed-product icon fallbacks, and FlatDirect has known transparency-icon quality failures. The JSON and TSV results are still written.

The default, predeclared visual thresholds are:

- Every required row: SSIM >= 0.98 and PSNR >= 30 dB.
- Every required mode aggregate: SSIM >= 0.995 and PSNR >= 35 dB.
- NoAa is diagnostic-only.

## Headline performance

Do not run the quality evaluator or other GPU/CPU-heavy work at the same time. The runner uses an offscreen context with no buffer swap; only use timings that report `vsync=offscreen-no-swap`.

```sh
AA_HEADLINE_DIR="$PWD/aa-product-headline"

./build-aa-poc-release/tools/aa_poc/aa_product_poc \
  --suite headline \
  --mode all \
  --scene all \
  --scale all \
  --warmup 100 \
  --frames 1000 \
  --repetitions 5 \
  --output-dir "$AA_HEADLINE_DIR"
```

Creation, shader/program initialization, warmup, PNG encoding, readback, and reporting are outside the timed region. Each repetition is bracketed by `glFinish()`.

## Focused diagnostics

```sh
./build-aa-poc-release/tools/aa_poc/aa_size_diagnostic \
  --output-dir "$PWD/aa-size-diagnostic"

./build-aa-poc-release/tools/aa_poc/aa_aabb_diagnostic \
  --output-dir "$PWD/aa-aabb-diagnostic"
```

The size diagnostic isolates route eligibility across scale. The AABB diagnostic keeps the mathematical curve fixed while changing analytical patch subdivision to isolate conservative control-AABB fragment cost. Diagnostic results must not be mixed into the headline matrix.

## Verification tests

```sh
python3 tools/aa_poc/test_evaluate_aa_product.py
python3 tools/aa_poc/test_aa_report.py
```

The completed reference run and interpretation are recorded in [`aa_product_test_results.md`](aa_product_test_results.md).
