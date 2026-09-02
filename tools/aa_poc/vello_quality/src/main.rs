use image::{ColorType, ImageFormat};
use serde::Serialize;
use std::fs::{self, File};
use std::io::{BufWriter, Write};
use std::path::PathBuf;
use std::time::{SystemTime, UNIX_EPOCH};
use vello::kurbo::{Affine, BezPath};
use vello::peniko::{Color, Fill, color::palette};
use vello::wgpu;
use vello::{AaConfig, AaSupport, RenderParams, Renderer, RendererOptions, Scene};

const VELLO_VERSION: &str = "0.10.0";
const WGPU_VERSION: &str = "29.0.4";
const KAPPA: f32 = 0.552_284_8;

#[derive(Clone, Copy, PartialEq, Eq)]
enum Suite {
    Product,
    Comparison,
}

#[derive(Clone, Copy)]
struct ScaleSpec {
    name: &'static str,
    coordinate_scale: f32,
    target_width: u32,
    target_height: u32,
}

#[derive(Clone, Copy)]
struct Offset {
    x: f32,
    y: f32,
}

#[derive(Clone, Copy)]
struct Coordinates {
    scale: f32,
    offset_x: f32,
    offset_y: f32,
}

impl Coordinates {
    fn point(self, x: f32, y: f32) -> (f64, f64) {
        (
            f64::from(x * self.scale + self.offset_x),
            f64::from(y * self.scale + self.offset_y),
        )
    }
}

struct PathBuilder {
    path: BezPath,
    coordinates: Coordinates,
}

impl PathBuilder {
    fn new(coordinates: Coordinates) -> Self {
        Self {
            path: BezPath::new(),
            coordinates,
        }
    }

    fn move_to(&mut self, x: f32, y: f32) {
        self.path.move_to(self.coordinates.point(x, y));
    }

    fn line_to(&mut self, x: f32, y: f32) {
        self.path.line_to(self.coordinates.point(x, y));
    }

    fn cubic_to(&mut self, x1: f32, y1: f32, x2: f32, y2: f32, x: f32, y: f32) {
        self.path.curve_to(
            self.coordinates.point(x1, y1),
            self.coordinates.point(x2, y2),
            self.coordinates.point(x, y),
        );
    }

    fn close(&mut self) {
        self.path.close_path();
    }
}

const PRODUCT_SCALES: [ScaleSpec; 3] = [
    ScaleSpec {
        name: "icon",
        coordinate_scale: 0.25,
        target_width: 64,
        target_height: 64,
    },
    ScaleSpec {
        name: "component",
        coordinate_scale: 1.0,
        target_width: 256,
        target_height: 256,
    },
    ScaleSpec {
        name: "large",
        coordinate_scale: 4.0,
        target_width: 1024,
        target_height: 1024,
    },
];

const COMPARISON_SCALES: [ScaleSpec; 3] = [
    ScaleSpec {
        name: "quarter",
        coordinate_scale: 0.25,
        target_width: 200,
        target_height: 120,
    },
    ScaleSpec {
        name: "half",
        coordinate_scale: 0.5,
        target_width: 400,
        target_height: 240,
    },
    ScaleSpec {
        name: "original",
        coordinate_scale: 1.0,
        target_width: 800,
        target_height: 480,
    },
];

const OFFSETS: [Offset; 4] = [
    Offset { x: 0.0, y: 0.0 },
    Offset { x: 0.125, y: 0.375 },
    Offset { x: 0.5, y: 0.5 },
    Offset { x: 0.875, y: 0.625 },
];

const SCENES: [&str; 4] = [
    "flat-core",
    "curve-core",
    "mixed-product-tile",
    "transparency-core",
];

const COMPARISON_SCENES: [&str; 1] = ["comparison"];

#[derive(Serialize)]
struct RendererMetadata {
    schema_version: u32,
    renderer: &'static str,
    runner_version: &'static str,
    renderer_version: &'static str,
    renderer_revision: &'static str,
    wgpu_version: &'static str,
    backend: &'static str,
    graphics_api: String,
    gpu_device: String,
    gpu_device_type: String,
    gpu_vendor: String,
    gpu_driver: String,
    antialiasing: &'static str,
    texture_format: &'static str,
    texture_sample_count: u32,
    base_color: &'static str,
    candidate_alpha: &'static str,
    gpu_completion: &'static str,
    presentation: &'static str,
    build_profile: &'static str,
    host_os: &'static str,
    host_arch: &'static str,
    run_started_unix_seconds: u64,
    reference: &'static str,
    scene_contract: &'static str,
    matrix: &'static str,
}

struct VelloContext {
    device: wgpu::Device,
    queue: wgpu::Queue,
    renderer: Renderer,
    metadata: RendererMetadata,
}

impl VelloContext {
    fn new(suite: Suite) -> Result<Self, String> {
        let instance = wgpu::Instance::default();
        let adapter = pollster::block_on(instance.request_adapter(&wgpu::RequestAdapterOptions {
            power_preference: wgpu::PowerPreference::HighPerformance,
            force_fallback_adapter: false,
            compatible_surface: None,
        }))
        .map_err(|error| format!("no compatible WebGPU adapter: {error}"))?;

        let available_features = adapter.features();
        let optional_features = wgpu::Features::CLEAR_TEXTURE | wgpu::Features::PIPELINE_CACHE;
        let (device, queue) = pollster::block_on(adapter.request_device(&wgpu::DeviceDescriptor {
            label: Some("Vello AAA quality device"),
            required_features: available_features & optional_features,
            required_limits: wgpu::Limits::default(),
            ..Default::default()
        }))
        .map_err(|error| format!("failed to create WebGPU device: {error}"))?;

        let renderer = Renderer::new(
            &device,
            RendererOptions {
                use_cpu: false,
                antialiasing_support: AaSupport::area_only(),
                ..Default::default()
            },
        )
        .map_err(|error| format!("failed to create Vello renderer: {error}"))?;

        let info = adapter.get_info();
        let run_started_unix_seconds = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .map_err(|error| format!("system clock predates Unix epoch: {error}"))?
            .as_secs();
        let metadata = RendererMetadata {
            schema_version: 2,
            renderer: "vello-area",
            runner_version: env!("CARGO_PKG_VERSION"),
            renderer_version: VELLO_VERSION,
            renderer_revision: "crates.io:vello@0.10.0",
            wgpu_version: WGPU_VERSION,
            backend: "webgpu",
            graphics_api: format!("WebGPU {:?}", info.backend),
            gpu_device: info.name,
            gpu_device_type: format!("{:?}", info.device_type),
            gpu_vendor: format!("0x{:04x}", info.vendor),
            gpu_driver: format!("{} {}", info.driver, info.driver_info)
                .trim()
                .to_string(),
            antialiasing: "AaConfig::Area (AaSupport::area_only)",
            texture_format: "rgba8unorm",
            texture_sample_count: 1,
            base_color: "opaque white (#ffffffff)",
            candidate_alpha: "opaque RGBA; runner rejects every alpha value other than 255",
            gpu_completion: "wgpu Device::poll(wait_indefinitely)",
            presentation: "offscreen-no-swap (quality only; no performance timing)",
            build_profile: if cfg!(debug_assertions) {
                "debug"
            } else {
                "release"
            },
            host_os: std::env::consts::OS,
            host_arch: std::env::consts::ARCH,
            run_started_unix_seconds,
            reference: "ThorVG NoAa at 8x with premultiplied box downsampling",
            scene_contract: if suite == Suite::Product {
                "tools/aa_poc/aa_product_scenes.cpp"
            } else {
                "tools/aa_poc/aa_poc_comparison_scene.cpp"
            },
            matrix: if suite == Suite::Product {
                "4 scenes x 3 scales x 4 subpixel offsets = 48 rows"
            } else {
                "1 original comparison scene x 3 display scales x 4 subpixel offsets = 12 frames; 8 characteristics scored separately"
            },
        };

        Ok(Self {
            device,
            queue,
            renderer,
            metadata,
        })
    }

    fn render(&mut self, scene: &Scene, width: u32, height: u32) -> Result<Vec<u8>, String> {
        let texture = self.device.create_texture(&wgpu::TextureDescriptor {
            label: Some("Vello AAA quality target"),
            size: wgpu::Extent3d {
                width,
                height,
                depth_or_array_layers: 1,
            },
            mip_level_count: 1,
            sample_count: 1,
            dimension: wgpu::TextureDimension::D2,
            format: wgpu::TextureFormat::Rgba8Unorm,
            usage: wgpu::TextureUsages::STORAGE_BINDING
                | wgpu::TextureUsages::TEXTURE_BINDING
                | wgpu::TextureUsages::COPY_SRC,
            view_formats: &[],
        });
        let view = texture.create_view(&wgpu::TextureViewDescriptor::default());
        self.renderer
            .render_to_texture(
                &self.device,
                &self.queue,
                scene,
                &view,
                &RenderParams {
                    base_color: palette::css::WHITE,
                    width,
                    height,
                    antialiasing_method: AaConfig::Area,
                },
            )
            .map_err(|error| format!("Vello render failed: {error}"))?;
        self.read_texture(&texture, width, height)
    }

    fn read_texture(
        &self,
        texture: &wgpu::Texture,
        width: u32,
        height: u32,
    ) -> Result<Vec<u8>, String> {
        let unpadded_bytes_per_row = width * 4;
        let padded_bytes_per_row = unpadded_bytes_per_row.div_ceil(256) * 256;
        let buffer_size = u64::from(padded_bytes_per_row) * u64::from(height);
        let buffer = self.device.create_buffer(&wgpu::BufferDescriptor {
            label: Some("Vello AAA quality readback"),
            size: buffer_size,
            usage: wgpu::BufferUsages::COPY_DST | wgpu::BufferUsages::MAP_READ,
            mapped_at_creation: false,
        });
        let mut encoder = self
            .device
            .create_command_encoder(&wgpu::CommandEncoderDescriptor {
                label: Some("Vello AAA quality copy encoder"),
            });
        encoder.copy_texture_to_buffer(
            texture.as_image_copy(),
            wgpu::TexelCopyBufferInfo {
                buffer: &buffer,
                layout: wgpu::TexelCopyBufferLayout {
                    offset: 0,
                    bytes_per_row: Some(padded_bytes_per_row),
                    rows_per_image: Some(height),
                },
            },
            wgpu::Extent3d {
                width,
                height,
                depth_or_array_layers: 1,
            },
        );
        self.queue.submit([encoder.finish()]);

        let (sender, receiver) = std::sync::mpsc::sync_channel(1);
        buffer
            .slice(..)
            .map_async(wgpu::MapMode::Read, move |result| {
                let _ = sender.send(result);
            });
        self.device
            .poll(wgpu::PollType::wait_indefinitely())
            .map_err(|error| format!("GPU completion wait failed: {error}"))?;
        receiver
            .recv()
            .map_err(|error| format!("capture mapping callback was lost: {error}"))?
            .map_err(|error| format!("capture buffer mapping failed: {error}"))?;

        let mapped = buffer.slice(..).get_mapped_range();
        let mut rgba = Vec::with_capacity(unpadded_bytes_per_row as usize * height as usize);
        for row in mapped.chunks_exact(padded_bytes_per_row as usize) {
            rgba.extend_from_slice(&row[..unpadded_bytes_per_row as usize]);
        }
        drop(mapped);
        buffer.unmap();
        Ok(rgba)
    }
}

fn add_shape(
    scene: &mut Scene,
    coordinates: Coordinates,
    rgba: [u8; 4],
    fill: Fill,
    build: impl FnOnce(&mut PathBuilder),
) {
    let mut builder = PathBuilder::new(coordinates);
    build(&mut builder);
    scene.fill(
        fill,
        Affine::IDENTITY,
        Color::from_rgba8(rgba[0], rgba[1], rgba[2], rgba[3]),
        None,
        &builder.path,
    );
}

fn add_circle(
    scene: &mut Scene,
    coordinates: Coordinates,
    cx: f32,
    cy: f32,
    radius: f32,
    rgba: [u8; 4],
) {
    let k = radius * KAPPA;
    add_shape(scene, coordinates, rgba, Fill::NonZero, |path| {
        path.move_to(cx + radius, cy);
        path.cubic_to(cx + radius, cy + k, cx + k, cy + radius, cx, cy + radius);
        path.cubic_to(cx - k, cy + radius, cx - radius, cy + k, cx - radius, cy);
        path.cubic_to(cx - radius, cy - k, cx - k, cy - radius, cx, cy - radius);
        path.cubic_to(cx + k, cy - radius, cx + radius, cy - k, cx + radius, cy);
        path.close();
    });
}

fn add_rotated_rect(
    scene: &mut Scene,
    coordinates: Coordinates,
    cx: f32,
    cy: f32,
    width: f32,
    height: f32,
    degrees: f32,
    rgba: [u8; 4],
) {
    let radians = degrees * 0.017_453_292_519_943_295_f32;
    let cosine = radians.cos();
    let sine = radians.sin();
    let center_x = cx * coordinates.scale + coordinates.offset_x;
    let center_y = cy * coordinates.scale + coordinates.offset_y;
    let half_width = width * coordinates.scale * 0.5;
    let half_height = height * coordinates.scale * 0.5;
    let transform = |x: f32, y: f32| {
        (
            f64::from(center_x + x * cosine - y * sine),
            f64::from(center_y + x * sine + y * cosine),
        )
    };

    let mut path = BezPath::new();
    path.move_to(transform(-half_width, -half_height));
    path.line_to(transform(half_width, -half_height));
    path.line_to(transform(half_width, half_height));
    path.line_to(transform(-half_width, half_height));
    path.close_path();
    scene.fill(
        Fill::NonZero,
        Affine::IDENTITY,
        Color::from_rgba8(rgba[0], rgba[1], rgba[2], rgba[3]),
        None,
        &path,
    );
}

fn comparison_scene(coordinate_scale: f32, offset: Offset) -> Scene {
    let coordinates = Coordinates {
        scale: coordinate_scale,
        offset_x: offset.x,
        offset_y: offset.y,
    };
    let mut scene = Scene::new();

    add_rotated_rect(
        &mut scene,
        coordinates,
        102.0,
        105.0,
        148.0,
        68.0,
        17.0,
        [20, 107, 224, 255],
    );

    add_shape(
        &mut scene,
        coordinates,
        [224, 31, 41, 255],
        Fill::NonZero,
        |path| {
            path.move_to(290.0, 35.0);
            path.line_to(364.0, 173.0);
            path.line_to(216.0, 173.0);
            path.close();
        },
    );

    add_circle(
        &mut scene,
        coordinates,
        490.0,
        105.0,
        72.0,
        [242, 122, 15, 255],
    );

    add_shape(
        &mut scene,
        coordinates,
        [178, 51, 178, 255],
        Fill::NonZero,
        |path| {
            path.move_to(625.0, 126.0);
            path.cubic_to(612.0, 45.0, 684.0, 23.0, 731.0, 72.0);
            path.cubic_to(773.0, 116.0, 733.0, 183.0, 672.0, 174.0);
            path.cubic_to(642.0, 170.0, 629.0, 151.0, 625.0, 126.0);
            path.close();
        },
    );

    add_rotated_rect(
        &mut scene,
        coordinates,
        112.0,
        342.0,
        174.0,
        18.0,
        -11.0,
        [20, 161, 94, 255],
    );

    add_shape(
        &mut scene,
        coordinates,
        [17, 128, 178, 255],
        Fill::NonZero,
        |path| {
            path.move_to(215.0, 390.0);
            path.line_to(235.0, 274.0);
            path.cubic_to(279.0, 235.0, 356.0, 273.0, 363.0, 338.0);
            path.cubic_to(368.0, 393.0, 302.0, 427.0, 215.0, 390.0);
            path.close();
        },
    );

    add_shape(
        &mut scene,
        coordinates,
        [12, 151, 102, 255],
        Fill::NonZero,
        |path| {
            path.move_to(435.0, 355.0);
            path.cubic_to(429.0, 278.0, 482.0, 247.0, 520.0, 303.0);
            path.cubic_to(559.0, 251.0, 616.0, 287.0, 605.0, 355.0);
            path.cubic_to(594.0, 418.0, 526.0, 435.0, 493.0, 390.0);
            path.cubic_to(468.0, 422.0, 438.0, 402.0, 435.0, 355.0);
            path.close();
        },
    );

    add_shape(
        &mut scene,
        coordinates,
        [115, 46, 199, 128],
        Fill::NonZero,
        |path| {
            path.move_to(652.0, 367.0);
            path.cubic_to(643.0, 293.0, 696.0, 257.0, 738.0, 306.0);
            path.cubic_to(780.0, 354.0, 746.0, 430.0, 681.0, 414.0);
            path.cubic_to(663.0, 408.0, 654.0, 388.0, 652.0, 367.0);
            path.close();
        },
    );

    scene
}

fn populate_flat_core(scene: &mut Scene, coordinates: Coordinates) {
    add_shape(
        scene,
        coordinates,
        [20, 107, 224, 255],
        Fill::NonZero,
        |path| {
            path.move_to(18.375, 30.125);
            path.line_to(99.125, 18.625);
            path.line_to(106.625, 60.375);
            path.line_to(25.875, 75.625);
            path.close();
        },
    );
    add_shape(
        scene,
        coordinates,
        [224, 31, 41, 255],
        Fill::NonZero,
        |path| {
            path.move_to(142.25, 25.375);
            path.line_to(231.75, 37.625);
            path.line_to(227.25, 69.875);
            path.line_to(160.5, 62.375);
            path.line_to(137.75, 49.125);
            path.close();
        },
    );
    add_shape(
        scene,
        coordinates,
        [20, 161, 94, 255],
        Fill::NonZero,
        |path| {
            path.move_to(20.625, 143.25);
            path.line_to(59.875, 116.75);
            path.line_to(106.375, 148.125);
            path.line_to(83.125, 171.875);
            path.line_to(105.625, 211.25);
            path.line_to(61.125, 225.5);
            path.line_to(25.375, 188.75);
            path.line_to(47.625, 166.0);
            path.close();
        },
    );
    add_shape(
        scene,
        coordinates,
        [178, 51, 178, 255],
        Fill::NonZero,
        |path| {
            path.move_to(145.25, 132.625);
            path.line_to(222.625, 118.375);
            path.line_to(236.125, 165.25);
            path.line_to(218.875, 220.625);
            path.line_to(158.375, 229.125);
            path.line_to(132.625, 180.375);
            path.close();
        },
    );
}

fn populate_curve_core(scene: &mut Scene, coordinates: Coordinates) {
    add_circle(
        scene,
        coordinates,
        63.375,
        61.625,
        38.25,
        [242, 122, 15, 255],
    );
    add_shape(
        scene,
        coordinates,
        [178, 51, 178, 255],
        Fill::NonZero,
        |path| {
            path.move_to(184.0, 19.0);
            path.cubic_to(216.0, 18.0, 237.0, 39.0, 233.0, 65.0);
            path.cubic_to(230.0, 91.0, 208.0, 108.0, 180.0, 102.0);
            path.cubic_to(150.0, 108.0, 127.0, 86.0, 132.0, 59.0);
            path.cubic_to(135.0, 31.0, 157.0, 20.0, 184.0, 19.0);
            path.close();
        },
    );
    add_shape(
        scene,
        coordinates,
        [20, 161, 94, 255],
        Fill::NonZero,
        |path| {
            path.move_to(20.0, 166.0);
            path.cubic_to(45.0, 144.0, 84.0, 139.0, 112.0, 159.0);
            path.cubic_to(116.0, 181.0, 89.0, 211.0, 55.0, 216.0);
            path.cubic_to(31.0, 216.0, 17.0, 193.0, 20.0, 166.0);
            path.close();
        },
    );
    add_shape(
        scene,
        coordinates,
        [17, 128, 178, 255],
        Fill::NonZero,
        |path| {
            path.move_to(143.0, 141.0);
            path.line_to(211.0, 128.0);
            path.cubic_to(237.0, 143.0, 241.0, 179.0, 219.0, 198.0);
            path.line_to(173.0, 229.0);
            path.cubic_to(145.0, 218.0, 130.0, 179.0, 143.0, 141.0);
            path.close();
        },
    );
}

fn add_flat_tile(
    scene: &mut Scene,
    coordinates: Coordinates,
    cx: f32,
    cy: f32,
    skew: f32,
    rgba: [u8; 4],
) {
    add_shape(scene, coordinates, rgba, Fill::NonZero, |path| {
        path.move_to(cx - 27.0, cy - 13.0 + skew);
        path.line_to(cx + 20.0, cy - 17.0 - skew);
        path.line_to(cx + 28.0, cy + 9.0);
        path.line_to(cx + 7.0, cy + 17.0 + skew);
        path.line_to(cx - 25.0, cy + 12.0 - skew);
        path.close();
    });
}

fn add_curved_tile(scene: &mut Scene, coordinates: Coordinates, cx: f32, cy: f32, rgba: [u8; 4]) {
    add_shape(scene, coordinates, rgba, Fill::NonZero, |path| {
        path.move_to(cx - 28.0, cy);
        path.cubic_to(cx - 27.0, cy - 13.0, cx - 15.0, cy - 18.0, cx, cy - 16.0);
        path.cubic_to(
            cx + 17.0,
            cy - 18.0,
            cx + 28.0,
            cy - 10.0,
            cx + 27.0,
            cy + 2.0,
        );
        path.cubic_to(
            cx + 27.0,
            cy + 13.0,
            cx + 14.0,
            cy + 18.0,
            cx - 2.0,
            cy + 16.0,
        );
        path.cubic_to(cx - 17.0, cy + 18.0, cx - 29.0, cy + 11.0, cx - 28.0, cy);
        path.close();
    });
}

fn populate_mixed_product_tile(scene: &mut Scene, coordinates: Coordinates) {
    const COLORS: [[u8; 4]; 6] = [
        [20, 107, 224, 255],
        [242, 122, 15, 255],
        [178, 51, 178, 255],
        [20, 161, 94, 255],
        [17, 128, 178, 255],
        [218, 56, 63, 255],
    ];
    const CENTERS_X: [f32; 3] = [43.375, 128.125, 212.875];
    const CENTERS_Y: [f32; 4] = [31.625, 96.125, 160.875, 225.375];
    let mut index = 0usize;
    for cy in CENTERS_Y {
        for cx in CENTERS_X {
            if index.is_multiple_of(2) {
                let skew = ((index % 3) as i32 - 1) as f32 * 1.5;
                add_flat_tile(scene, coordinates, cx, cy, skew, COLORS[index % 6]);
            } else {
                add_curved_tile(scene, coordinates, cx, cy, COLORS[index % 6]);
            }
            index += 1;
        }
    }
}

fn populate_transparency_core(scene: &mut Scene, coordinates: Coordinates) {
    add_shape(
        scene,
        coordinates,
        [115, 46, 199, 128],
        Fill::EvenOdd,
        |path| {
            path.move_to(128.375, 25.625);
            path.cubic_to(188.625, 20.375, 232.125, 58.875, 226.625, 118.375);
            path.cubic_to(233.375, 177.625, 192.875, 225.125, 132.625, 229.375);
            path.cubic_to(73.125, 235.625, 27.875, 195.375, 30.625, 134.125);
            path.cubic_to(23.875, 74.625, 67.375, 29.125, 128.375, 25.625);
            path.close();
            path.move_to(169.625, 128.375);
            path.cubic_to(169.625, 150.125, 151.25, 166.625, 128.125, 165.875);
            path.cubic_to(104.875, 166.625, 87.625, 150.0, 88.375, 127.625);
            path.cubic_to(87.625, 105.875, 105.375, 89.625, 128.875, 90.375);
            path.cubic_to(151.875, 89.625, 170.375, 106.125, 169.625, 128.375);
            path.close();
        },
    );
}

fn product_scene(name: &str, coordinate_scale: f32, offset: Offset) -> Scene {
    let coordinates = Coordinates {
        scale: coordinate_scale,
        offset_x: offset.x,
        offset_y: offset.y,
    };
    let mut scene = Scene::new();
    match name {
        "flat-core" => populate_flat_core(&mut scene, coordinates),
        "curve-core" => populate_curve_core(&mut scene, coordinates),
        "mixed-product-tile" => populate_mixed_product_tile(&mut scene, coordinates),
        "transparency-core" => populate_transparency_core(&mut scene, coordinates),
        _ => unreachable!("scene table contains only supported names"),
    }
    scene
}

fn number_key(value: f32) -> String {
    format!("{value:.3}").replace('-', "m").replace('.', "p")
}

fn relative_case_dir(root: &str, scene: &str, scale: ScaleSpec, offset: Offset) -> PathBuf {
    PathBuf::from(root)
        .join(scene)
        .join(scale.name)
        .join(format!(
            "offset-x{}-y{}",
            number_key(offset.x),
            number_key(offset.y)
        ))
}

fn parse_options() -> Result<(PathBuf, Suite), String> {
    let mut args = std::env::args().skip(1);
    let mut output_dir = None;
    let mut suite = Suite::Product;
    while let Some(argument) = args.next() {
        if argument == "--output-dir" {
            output_dir = Some(
                args.next()
                    .ok_or_else(|| "--output-dir requires a path".to_string())?,
            );
        } else if let Some(value) = argument.strip_prefix("--output-dir=") {
            output_dir = Some(value.to_string());
        } else if argument == "--suite" {
            let value = args
                .next()
                .ok_or_else(|| "--suite requires product or comparison".to_string())?;
            suite = match value.as_str() {
                "product" => Suite::Product,
                "comparison" => Suite::Comparison,
                _ => return Err(format!("unsupported suite {value:?}")),
            };
        } else if let Some(value) = argument.strip_prefix("--suite=") {
            suite = match value {
                "product" => Suite::Product,
                "comparison" => Suite::Comparison,
                _ => return Err(format!("unsupported suite {value:?}")),
            };
        } else if argument == "--help" || argument == "-h" {
            println!("usage: vello-aa-quality --output-dir DIR [--suite product|comparison]");
            std::process::exit(0);
        } else {
            return Err(format!("unknown argument {argument:?}"));
        }
    }
    let output_dir = output_dir
        .filter(|value| !value.is_empty())
        .map(PathBuf::from)
        .ok_or_else(|| "--output-dir is required".to_string())?;
    Ok((output_dir, suite))
}

fn run() -> Result<(), String> {
    let (output_dir, suite) = parse_options()?;
    if !output_dir.is_dir() {
        return Err(format!(
            "output directory {} does not exist; generate the AAA SSAA8 references first",
            output_dir.display()
        ));
    }

    let mut context = VelloContext::new(suite)?;
    println!(
        "Vello {} / {} / {}",
        VELLO_VERSION, context.metadata.graphics_api, context.metadata.gpu_device
    );
    let metadata_name = if suite == Suite::Product {
        "vello-renderer-metadata.json"
    } else {
        "vello-comparison-metadata.json"
    };
    let metadata_path = output_dir.join(metadata_name);
    let json = serde_json::to_string_pretty(&context.metadata)
        .map_err(|error| format!("failed to encode renderer metadata: {error}"))?;
    fs::write(&metadata_path, format!("{json}\n"))
        .map_err(|error| format!("failed to write {}: {error}", metadata_path.display()))?;

    let manifest_path = output_dir.join(if suite == Suite::Product {
        "vello-quality-manifest.tsv"
    } else {
        "vello-comparison-manifest.tsv"
    });
    let manifest_file = File::create(&manifest_path)
        .map_err(|error| format!("failed to create {}: {error}", manifest_path.display()))?;
    let mut manifest = BufWriter::new(manifest_file);
    writeln!(
        manifest,
        "mode\tscene\tscale\toffset_x\toffset_y\tcandidate_png"
    )
    .map_err(|error| format!("failed to write manifest header: {error}"))?;

    let mut rows = 0u32;
    let (scene_names, scales, root): (&[&str], &[ScaleSpec], &str) = if suite == Suite::Product {
        (&SCENES, &PRODUCT_SCALES, "quality")
    } else {
        (&COMPARISON_SCENES, &COMPARISON_SCALES, "diagnostic")
    };
    for scene_name in scene_names {
        for &scale in scales {
            for offset in OFFSETS {
                let relative_dir = relative_case_dir(root, scene_name, scale, offset);
                let directory = output_dir.join(&relative_dir);
                let reference = directory.join("ssaa8.png");
                if !reference.is_file() {
                    return Err(format!("missing SSAA8 reference {}", reference.display()));
                }
                fs::create_dir_all(&directory).map_err(|error| {
                    format!("failed to create {}: {error}", directory.display())
                })?;
                let candidate = directory.join("vello-area.png");
                let scene = if suite == Suite::Product {
                    product_scene(scene_name, scale.coordinate_scale, offset)
                } else {
                    comparison_scene(scale.coordinate_scale, offset)
                };
                let rgba = context.render(&scene, scale.target_width, scale.target_height)?;
                if rgba.chunks_exact(4).any(|pixel| pixel[3] != 255) {
                    return Err(format!(
                        "{} produced non-opaque pixels despite the required white base color",
                        candidate.display()
                    ));
                }
                image::save_buffer_with_format(
                    &candidate,
                    &rgba,
                    scale.target_width,
                    scale.target_height,
                    ColorType::Rgba8,
                    ImageFormat::Png,
                )
                .map_err(|error| format!("failed to save {}: {error}", candidate.display()))?;

                let candidate_relative = relative_dir.join("vello-area.png");
                writeln!(
                    manifest,
                    "vello-area\t{}\t{}\t{:.3}\t{:.3}\t{}",
                    scene_name,
                    scale.name,
                    offset.x,
                    offset.y,
                    candidate_relative.display(),
                )
                .map_err(|error| format!("failed to write manifest row: {error}"))?;
                rows += 1;
                println!(
                    "QUALITY\trenderer=vello-area\tscene={}\tscale={}\toffset={:.3},{:.3}\tfile={}",
                    scene_name,
                    scale.name,
                    offset.x,
                    offset.y,
                    candidate.display()
                );
            }
        }
    }
    manifest
        .flush()
        .map_err(|error| format!("failed to flush {}: {error}", manifest_path.display()))?;
    println!("wrote {rows} rows to {}", manifest_path.display());
    Ok(())
}

fn main() {
    if let Err(error) = run() {
        eprintln!("vello-aa-quality: {error}");
        std::process::exit(1);
    }
}
