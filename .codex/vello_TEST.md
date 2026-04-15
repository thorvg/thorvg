# Vello GPU Test Analysis

## Summary

`vello` tests its GPU path with headless `wgpu` rendering, then validates output in two complementary ways:

- snapshot tests against committed PNG references
- CPU-vs-GPU comparison tests against the same scene

The important point is that even the "CPU" path still goes through a `wgpu` device and offscreen texture. The difference is `RendererOptions { use_cpu: true }`, not a separate windowing stack.

## Core Technique

```mermaid
flowchart TD
    A[Test scene] --> B[render_then_debug()]
    B --> C[RenderContext::new + device()]
    C --> D[Renderer::new(use_cpu false/true)]
    D --> E[render_to_texture()]
    E --> F[copy_texture_to_buffer + map_async]
    F --> G[ImageData RGBA]
    G --> H1[snapshot_test_image]
    G --> H2[compare_gpu_cpu]
    H1 --> I1[nv-flip vs committed snapshot PNG]
    H2 --> I2[nv-flip vs CPU/GPU pair]
    I1 --> J[write current/update artifacts on failure]
    I2 --> J
```

## Offscreen Rendering Path

`vello_tests/src/lib.rs` renders directly into a `wgpu` texture, copies the texture into a readback buffer, then builds an `ImageData`:

```rust
let target = device.create_texture(&TextureDescriptor {
    label: Some("Target texture"),
    size,
    format: TextureFormat::Rgba8Unorm,
    usage: TextureUsages::STORAGE_BINDING | TextureUsages::COPY_SRC,
    ..Default::default()
});
let view = target.create_view(&wgpu::TextureViewDescriptor::default());
renderer.render_to_texture(device, queue, scene, &view, &render_params)?;
encoder.copy_texture_to_buffer(target.as_image_copy(), ..., size);
buf_slice.map_async(wgpu::MapMode::Read, move |v| sender.send(v).unwrap());
```

This means `vello` does not depend on a visible window for GPU tests. The test contract is "render to offscreen texture, read back bytes, compare."

## Snapshot Tests

`vello_tests/src/snapshot.rs` stores per-test outputs under `current/{cpu|gpu}` and compares them against `snapshots/`:

```rust
let update_path = c_dir.join(&params.name).with_extension("png");
let reference_path = snapshot_dir(directory)
    .join(&params.name)
    .with_extension("png");

let expected_data = match image::open(&reference_path) {
    Ok(contents) => { ... }
    Err(ImageError::IoError(ref e)) if e.kind() == ErrorKind::NotFound => { ... }
```

On failure it either:

- writes a fresh PNG into `current/...`
- or updates the reference if `VELLO_TEST_UPDATE=...` is set

The comparison metric is `nv-flip`, not strict byte equality, which gives them some tolerance for GPU-side numeric drift.

## CPU-vs-GPU Comparison

`vello_tests/src/compare.rs` renders the same scene twice and compares the images:

```rust
params.use_cpu = false;
let gpu_rendered = render_then_debug(&scene, &params).await?;
params.use_cpu = true;
let cpu_rendered = render_then_debug(&scene, &params).await?;

let error_map = nv_flip::flip(cpu_flip, gpu_flip, nv_flip::DEFAULT_PIXELS_PER_DEGREE);
let pool = FlipPool::from_image(&error_map);
```

The public tests in `tests/compare_gpu_cpu.rs` keep this very direct:

```rust
compare_gpu_cpu_sync(scene, params)
    .unwrap()
    .assert_mean_less_than(0.01);
```

This is a strong pattern for GPU backend bring-up because it avoids committing large golden sets for every case.

## GPU Availability Handling

GPU coverage is gated at build time in `vello_tests/build.rs`:

```rust
println!("cargo:rerun-if-env-changed=VELLO_CI_GPU_SUPPORT");
println!("cargo:rustc-check-cfg=cfg(skip_gpu_tests)");
if let Ok(mut value) = env::var("VELLO_CI_GPU_SUPPORT") {
    match &*value {
        "no" | "n" => println!("cargo:rustc-cfg=skip_gpu_tests"),
        _ => {}
    }
}
```

Tests then use:

```rust
#[cfg_attr(skip_gpu_tests, ignore)]
```

That is a nice fit for CI where a runner may compile the tests but intentionally skip GPU execution.

## Developer Workflow

`xtask` wraps snapshot/comparison reporting:

```rust
pub enum CliCommand {
    SnapshotsCpu(Args),
    SnapshotsGpu(Args),
    Comparisons(ComparisonsArgs),
}
```

And `xtask/README.md` shows the intended workflow:

```bash
cargo xtask snapshots-gpu report
cargo xtask snapshots-gpu review
cargo xtask comparisons report
```

## What ThorVG Can Learn

- Offscreen texture readback is the cleanest GPU test shape.
- GPU-vs-reference and GPU-vs-CPU are separate test values and both matter.
- Environment-driven skip is better than hard failure on non-GPU CI.
- Artifact directories (`current/`, `comparisons/`) make failures debuggable.

## Key Files

- `vello/vello_tests/src/lib.rs`
- `vello/vello_tests/src/snapshot.rs`
- `vello/vello_tests/src/compare.rs`
- `vello/vello_tests/tests/compare_gpu_cpu.rs`
- `vello/vello_tests/build.rs`
- `vello/examples/headless/src/main.rs`
- `vello/xtask/src/main.rs`
