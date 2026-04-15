# GL/GPU Test Investigation

- Date: 2026-04-15
- Status: in progress
- Scope:
  - ThorVG GL test introduction strategy
  - External project survey: `vello`, `rive-runtime`, `skia`

## Current ThorVG Notes

- ThorVG `GlCanvas` does not create a GL context.
- ThorVG GL code loads GL entry points and uses an externally-created context.
- `GlCanvas::target(display, surface, context, id, width, height, ...)` still requires a valid `context`.
- `id == 0` means the default framebuffer, not "create one for me".
- Therefore GL tests need an external context provider even if the target is the main framebuffer.

## Practical Direction

- Ubuntu-only CI is a reasonable first target.
- `SDL2` is not a requirement for correctness; it is one way to create/manage a GL context.
- For ThorVG's current GL test shape, the real requirement is:
  - create a valid GL context
  - make it current
  - call `GlCanvas::target(..., context, 0, ...)`
  - read back pixels for assertion

## External Project Findings Snapshot

- `vello`
  - Headless `wgpu` rendering into an offscreen texture.
  - Two major strategies:
    - snapshot-vs-reference PNG
    - CPU-vs-GPU image comparison using `nv-flip`
  - CI can disable GPU cases with a build-time cfg.

- `rive-runtime`
  - Uses a `TestingWindow` abstraction to normalize backend-specific rendering and pixel readback.
  - Heavy GPU validation is visual:
    - `gms` for hand-written rendering cases
    - `goldens` for `.riv` scene rendering
  - Image comparison/reporting is done by Python diff tooling.
  - Unit tests also use "silver" serialized rendering output for deterministic renderer-command checks.

- `skia`
  - Two layers:
    - macro-driven GPU unit tests across backend matrices
    - large DM/GM visual regression harness
  - DM hashes normalized output and uploads `dm.json` + images for Gold triage.

## Deliverables

- [x] `.codex/GL_TEST.md`
- [x] `.codex/vello_TEST.md`
- [x] `.codex/rive-runtime_TEST.md`
- [x] `.codex/skia_TEST.md`
