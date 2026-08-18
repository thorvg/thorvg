# Product GL AA test results

Date: 2026-08-19 (Asia/Taipei)

Plan: [Notion GL anti-aliasing test plan](https://app.notion.com/p/3c032f8a3abd81ba8db8d845ff3cf031)

## Outcome

Keep `Msaa4` as the product default. `CurveDirect`, `CurveMask`, and `Hybrid` pass the predeclared visual gates, but none is a uniform performance win. `FlatDirect` and `FlatMask` are ineligible because they fall back on the mixed-product icon scene; `FlatDirect` also fails the translucent-icon visual gate.

| Mode | Route/sample gate | Aggregate SSIM | PSNR (dB) | Visual gate | Decision |
| --- | --- | ---: | ---: | --- | --- |
| NoAa | Pass, 1x | 0.983905 | 30.418 | Diagnostic only | Baseline diagnostic |
| Msaa4 | Pass, 4x | 0.997727 | 39.462 | Pass | Retain as default |
| FlatDirect | 4/48 rows invalid | 0.994997* | 38.355* | Fail | Ineligible |
| CurveDirect | Pass, 1x | 0.998231 | 39.886 | Pass | Promising, scene-dependent |
| FlatMask | 4/48 rows invalid | 0.999748* | 49.896* | Invalid | Ineligible |
| CurveMask | Pass, 1x | 0.999791 | 49.515 | Pass | Best quality, too costly as default |
| Hybrid | Pass, 1x | 0.998380 | 39.939 | Pass | Promising, not a uniform win |

`*` Aggregate excludes the four invariant-invalid rows and cannot make the mode eligible.

## Test contract

- Seven creation-time modes: NoAa, Msaa4, FlatDirect, CurveDirect, FlatMask, CurveMask, Hybrid.
- Four shared scenes: flat core, curve core, mixed product tile, transparency core.
- Three plan scale labels mapped to a normalized 256x256 scene: icon = 0.25x/64px, component = 1x/256px, large = 4x/1024px. The plan did not prescribe numeric scale values.
- Quality matrix: 7 modes x 4 scenes x 3 scales x 4 subpixel offsets = 336 rows.
- Quality offsets: (0,0), (0.125,0.375), (0.5,0.5), (0.875,0.625).
- Performance offset: (0.375,0.625), with 100 warmup frames, 1,000 timed frames, and five repetitions.
- Oracle: true single-sample rendering at 8x target/coordinates/offset, premultiplied box filtering, then standards-compliant straight-alpha PNG encoding.
- Every row asserts selected mode, root samples, requested route count, total route count, and zero fallback. Invalid rows are not decoded or included in visual aggregates.
- Predeclared gates: each required row SSIM >= 0.98 and PSNR >= 30 dB; each required mode aggregate SSIM >= 0.995 and PSNR >= 35 dB. NoAa is diagnostic-only.

The implementation preserves the existing default (`Msaa4`). Its root and pooled targets are true 4x with resolve; all other modes use true 1x targets without an MSAA resolve. Unsupported work falls back through the current renderer on the selected root topology.

## Quality failures

Eight rows are invariant-invalid: FlatDirect and FlatMask at all four offsets of `mixed-product-tile/icon`. Each frame routes six of the twelve shapes as requested and explicitly falls back for the six curved tiles. These rows are excluded from visual scoring.

The size diagnostic identifies an adaptive-tessellation eligibility cliff, not inter-shape overlap. The 1px analytical boundary band rejects nonadjacent flattened edges on the small curved tile. CurveDirect remains valid at every sampled size, while FlatDirect and FlatMask are valid only at 0.135-0.145 and from 0.275 upward (plus the 0.5 and 1.0 controls).

FlatDirect also fails all four required `transparency-core/icon` rows:

| Offset | SSIM | PSNR (dB) |
| --- | ---: | ---: |
| (0,0) | 0.966643 | 34.529 |
| (0.125,0.375) | 0.969473 | 34.586 |
| (0.5,0.5) | 0.976495 | 35.870 |
| (0.875,0.625) | 0.971310 | 35.088 |

## Headline performance

Values are candidate time divided by Msaa4 time; lower is faster. Only quality-eligible candidates are shown. All listed rows have correct routes and zero fallback.

| Scene | Scale | Msaa4 (us/frame) | CurveDirect | CurveMask | Hybrid |
| --- | --- | ---: | ---: | ---: | ---: |
| Flat core | Icon | 167.551 | 0.620x | 3.200x | 0.617x |
| Flat core | Component | 176.932 | 0.616x | 3.084x | 0.610x |
| Flat core | Large | 829.410 | 0.164x | 0.773x | 0.158x |
| Curve core | Icon | 166.790 | 0.639x | 3.324x | 0.616x |
| Curve core | Component | 176.768 | 0.792x | 3.195x | 0.848x |
| Curve core | Large | 826.253 | 1.295x | 1.680x | 1.295x |
| Mixed product tile | Icon | 164.494 | 0.885x | 8.650x | 1.133x |
| Mixed product tile | Component | 174.412 | 0.851x | 8.278x | 1.777x |
| Mixed product tile | Large | 823.644 | 0.668x | 1.962x | 0.947x |
| Transparency core | Icon | 169.135 | 0.671x | 1.291x | 1.226x |
| Transparency core | Component | 183.052 | 0.789x | 1.300x | 1.681x |
| Transparency core | Large | 824.471 | 2.159x | 2.343x | 2.343x |

CurveDirect is faster than Msaa4 in 10/12 cases, including an 83.6% reduction on flat/large, but is 1.30x slower on curve/large and 2.16x slower on transparency/large. CurveMask is faster only on flat/large and reaches 8.65x the Msaa4 time on mixed/icon. Hybrid is faster in 6/12 cases, but its classification work and mask route make mixed/component 1.78x and transparency/large 2.34x slower.

## Focused diagnostics

### Size eligibility

- Matrix: 53 scales x FlatDirect/FlatMask/CurveDirect = 159 rows, fixed 64x64 target.
- CurveDirect: 53/53 valid.
- FlatDirect and FlatMask: valid at 0.135-0.145, invalid at 0.100-0.130 and 0.150-0.270, then valid from 0.275 through 1.0.
- Valid paired rows: FlatMask averages 2.01x FlatDirect and 1.97x CurveDirect; FlatDirect averages 0.98x CurveDirect.
- All controls used 1x roots, 20 warmup frames, 100 timed frames x three repetitions, and offscreen-no-swap timing.

### Curve control-AABB coverage

A fixed 800px-diameter cubic circle was recursively split without changing its mathematical silhouette. Reducing conservative analytical patch coverage from 99.75% to 2.93% changed CurveDirect from 1.606ms to 0.250ms (6.42x faster) and CurveMask from 1.773ms to 0.433ms (4.09x faster), while Msaa4 stayed effectively flat at 0.882-0.886ms. This isolates broad control AABBs as the principal large-curve regression.

## Recommendation

Do not change the default yet.

1. Continue with CurveDirect as the most promising single-sample candidate, but subdivide or tighten conservative cubic patch bounds before judging large curve-heavy surfaces.
2. Keep CurveMask as the visual-quality reference candidate, not the default, until its fragment and multi-pass cost is reduced.
3. Rework FlatDirect/FlatMask boundary eligibility around adaptive curve tessellation before reconsidering them; the current icon-size fallback violates the no-fallback gate.
4. Rerun this exact matrix after those targeted changes. Hybrid should be reconsidered only after both analytical patch cost and classification overhead are addressed.

## Artifacts

These raw artifact paths identify the original test machine and are not part of the commit. Use [`aa_product_poc.md`](aa_product_poc.md) to generate an equivalent local result set on another computer.

- [Quality manifest](/tmp/thorvg-aa-product-quality-final2.3IMsyn/quality-manifest.tsv) — SHA-256 `87c69afa4c231ae750326a90bdab7e0555ebcd76ee2623c2e690e26896003c22`
- [Quality row results](/tmp/thorvg-aa-product-quality-final2.3IMsyn/quality-results.tsv)
- [Quality summary](/tmp/thorvg-aa-product-quality-final2.3IMsyn/quality-summary.json)
- [Headline timing](/tmp/thorvg-aa-product-headline-final2.A430w5/headline.tsv) — SHA-256 `a31fe08bd1d6d60df4e2107b48ea296461ff79080f432cf08fffc7a6ea6991f2`
- [Size diagnostic](/tmp/thorvg-aa-size-diagnostic-full-20260819/size-diagnostic.tsv) — SHA-256 `edccc3b26686dc90775f54af0a0d198dcd6946ac15e1aabc482f60970b9148ff`
- [AABB diagnostic](/tmp/thorvg-aa-aabb-diagnostic.xE4Sea/aabb-diagnostic.tsv) — SHA-256 `cb27b0afd729751b8007ebe64e53e62ee0976af4174bb21ad42569523c757be2`

## Environment and verification

- Apple M1, 8-core GPU; macOS 26.5.2 (25F84), arm64.
- OpenGL 4.1 Metal 90.5; ThorVG GL compatibility target OpenGL 3.3 / GLSL 3.30.
- Apple clang 21.0.0; Meson 1.11.0.
- Performance label: `offscreen-no-swap`; there is no drawable swap interval or frame-rate limiter.
- Release product/diagnostic/legacy POC targets built successfully.
- A normal non-POC GL build completed successfully.
- GL canvas tests: 56 assertions in 6 cases passed.
- GL engine tests: 9,911 assertions in 7 cases passed.
- Evaluator tests: 12 passed. Existing report tests: 24 passed.
- Independent final audit: no remaining P0/P1 blocker.
- `git diff --check`: clean.

All implementation and report changes remain uncommitted.
