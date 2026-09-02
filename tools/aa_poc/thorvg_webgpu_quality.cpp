/*
 * Copyright (c) 2026 ThorVG project. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <webgpu/webgpu.h>
#include <webgpu/wgpu.h>

#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <vector>

#include <thorvg.h>

#include "aa_poc_comparison_scene.h"
#include "aa_product_scenes.h"
#include "lodepng.h"

namespace
{

constexpr const char* MODE = "thorvg-webgpu-msaa4";

enum class Suite
{
    Product,
    Comparison,
};

struct ScaleSpec
{
    const char* name;
    float coordinateScale;
    uint32_t targetWidth;
    uint32_t targetHeight;
};

struct Offset
{
    float x;
    float y;
};

constexpr aa_poc::SceneKind SCENES[] = {
    aa_poc::SceneKind::FlatCore,
    aa_poc::SceneKind::CurveCore,
    aa_poc::SceneKind::MixedProductTile,
    aa_poc::SceneKind::TransparencyCore,
};

constexpr ScaleSpec PRODUCT_SCALES[] = {
    {"icon", 0.25f, 64, 64},
    {"component", 1.0f, 256, 256},
    {"large", 4.0f, 1024, 1024},
};

constexpr ScaleSpec COMPARISON_SCALES[] = {
    {"quarter", 0.25f, 200, 120},
    {"half", 0.5f, 400, 240},
    {"original", 1.0f, 800, 480},
};

constexpr Offset OFFSETS[] = {
    {0.0f, 0.0f},
    {0.125f, 0.375f},
    {0.5f, 0.5f},
    {0.875f, 0.625f},
};

struct PaintReleaser
{
    void operator()(tvg::Paint* paint) const { tvg::Paint::rel(paint); }
};

using ShapePtr = std::unique_ptr<tvg::Shape, PaintReleaser>;

std::string stringView(WGPUStringView view)
{
    if (!view.data) return {};
    if (view.length == WGPU_STRLEN) return std::string(view.data);
    return std::string(view.data, view.length);
}

const char* backendName(WGPUBackendType backend)
{
    switch (backend) {
        case WGPUBackendType_Metal: return "Metal";
        case WGPUBackendType_Vulkan: return "Vulkan";
        case WGPUBackendType_D3D11: return "D3D11";
        case WGPUBackendType_D3D12: return "D3D12";
        case WGPUBackendType_OpenGL: return "OpenGL";
        case WGPUBackendType_OpenGLES: return "OpenGL ES";
        case WGPUBackendType_WebGPU: return "WebGPU";
        default: return "unknown";
    }
}

std::string jsonEscape(const std::string& value)
{
    std::ostringstream output;
    for (unsigned char character : value) {
        switch (character) {
            case '\"': output << "\\\""; break;
            case '\\': output << "\\\\"; break;
            case '\b': output << "\\b"; break;
            case '\f': output << "\\f"; break;
            case '\n': output << "\\n"; break;
            case '\r': output << "\\r"; break;
            case '\t': output << "\\t"; break;
            default:
                if (character < 0x20) {
                    output << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                           << static_cast<unsigned>(character) << std::dec;
                } else {
                    output << static_cast<char>(character);
                }
        }
    }
    return output.str();
}

bool ensureDirectory(const std::string& path)
{
    if (path.empty()) return false;
    std::string current;
    for (size_t index = 0; index < path.size(); ++index) {
        auto character = path[index];
        current.push_back(character);
        if (character != '/' && index + 1 != path.size()) continue;
        if (current == "/") continue;
        while (current.size() > 1 && current.back() == '/') current.pop_back();
        if (mkdir(current.c_str(), 0755) != 0 && errno != EEXIST) {
            std::perror(("mkdir " + current).c_str());
            return false;
        }
        if (index + 1 != path.size()) current.push_back('/');
    }
    return true;
}

std::string offsetComponent(float value)
{
    std::ostringstream output;
    output << std::fixed << std::setprecision(3) << value;
    auto text = output.str();
    for (auto& character : text) {
        if (character == '.') character = 'p';
        else if (character == '-') character = 'm';
    }
    return text;
}

std::string formatNumber(float value)
{
    std::ostringstream output;
    output << std::fixed << std::setprecision(3) << value;
    auto text = output.str();
    while (text.size() > 1 && text.back() == '0') text.pop_back();
    if (!text.empty() && text.back() == '.') text.pop_back();
    return text;
}

bool check(tvg::Result result, const char* operation, const std::string& diagnostic)
{
    if (result == tvg::Result::Success) return true;
    std::cerr << diagnostic << ": " << operation << " failed (Result="
              << static_cast<unsigned>(result) << ")\n";
    return false;
}

class WebGpuContext
{
public:
    WGPUInstance instance = nullptr;
    WGPUAdapter adapter = nullptr;
    WGPUDevice device = nullptr;
    WGPUQueue queue = nullptr;
    std::string deviceName = "unknown";
    std::string vendor = "unknown";
    std::string description = "unknown";
    std::string backend = "unknown";
    std::atomic<bool> uncapturedError{false};
    std::atomic<bool> deviceLost{false};

    ~WebGpuContext()
    {
        if (queue) wgpuQueueRelease(queue);
        if (device) {
            wgpuDeviceDestroy(device);
            wgpuDeviceRelease(device);
        }
        if (adapter) wgpuAdapterRelease(adapter);
        if (instance) wgpuInstanceRelease(instance);
    }

    bool initialize()
    {
        WGPUInstanceDescriptor instanceDescriptor = {};
        instance = wgpuCreateInstance(&instanceDescriptor);
        if (!instance) {
            std::cerr << "WebGPU: wgpuCreateInstance failed\n";
            return false;
        }

        struct AdapterState
        {
            WGPURequestAdapterStatus status = WGPURequestAdapterStatus_Unknown;
            WGPUAdapter adapter = nullptr;
            std::string message;
            std::atomic<bool> complete{false};
        } adapterState;

        WGPURequestAdapterOptions adapterOptions = {};
        adapterOptions.featureLevel = WGPUFeatureLevel_Core;
        adapterOptions.powerPreference = WGPUPowerPreference_HighPerformance;
        WGPURequestAdapterCallbackInfo adapterCallback = {};
        adapterCallback.mode = WGPUCallbackMode_AllowSpontaneous;
        adapterCallback.callback = [](WGPURequestAdapterStatus status,
                                      WGPUAdapter requestedAdapter,
                                      WGPUStringView message, void* userdata1,
                                      void*) {
            auto* state = static_cast<AdapterState*>(userdata1);
            state->status = status;
            state->adapter = requestedAdapter;
            state->message = stringView(message);
            state->complete.store(true, std::memory_order_release);
        };
        adapterCallback.userdata1 = &adapterState;
        wgpuInstanceRequestAdapter(instance, &adapterOptions, adapterCallback);
        while (!adapterState.complete.load(std::memory_order_acquire)) {
            wgpuInstanceProcessEvents(instance);
        }
        if (adapterState.status != WGPURequestAdapterStatus_Success ||
            !adapterState.adapter) {
            std::cerr << "WebGPU: adapter request failed: " << adapterState.message << "\n";
            return false;
        }
        adapter = adapterState.adapter;

        WGPUAdapterInfo adapterInfo = {};
        if (wgpuAdapterGetInfo(adapter, &adapterInfo) == WGPUStatus_Success) {
            deviceName = stringView(adapterInfo.device);
            vendor = stringView(adapterInfo.vendor);
            description = stringView(adapterInfo.description);
            backend = backendName(adapterInfo.backendType);
            wgpuAdapterInfoFreeMembers(adapterInfo);
        }

        struct DeviceState
        {
            WGPURequestDeviceStatus status = WGPURequestDeviceStatus_Unknown;
            WGPUDevice device = nullptr;
            std::string message;
            std::atomic<bool> complete{false};
        } deviceState;

        WGPUDeviceDescriptor deviceDescriptor = {};
        deviceDescriptor.label = {"ThorVG WebGPU AAA quality device", WGPU_STRLEN};
        deviceDescriptor.uncapturedErrorCallbackInfo.callback =
            [](WGPUDevice const*, WGPUErrorType type, WGPUStringView message,
               void* userdata1, void*) {
                auto* context = static_cast<WebGpuContext*>(userdata1);
                context->uncapturedError.store(true, std::memory_order_release);
                std::cerr << "WebGPU uncaptured error " << static_cast<unsigned>(type)
                          << ": " << stringView(message) << "\n";
            };
        deviceDescriptor.uncapturedErrorCallbackInfo.userdata1 = this;
        deviceDescriptor.deviceLostCallbackInfo.mode = WGPUCallbackMode_AllowSpontaneous;
        deviceDescriptor.deviceLostCallbackInfo.callback =
            [](WGPUDevice const*, WGPUDeviceLostReason reason, WGPUStringView message,
               void* userdata1, void*) {
                if (reason == WGPUDeviceLostReason_Destroyed ||
                    reason == WGPUDeviceLostReason_InstanceDropped) return;
                auto* context = static_cast<WebGpuContext*>(userdata1);
                context->deviceLost.store(true, std::memory_order_release);
                std::cerr << "WebGPU device lost " << static_cast<unsigned>(reason)
                          << ": " << stringView(message) << "\n";
            };
        deviceDescriptor.deviceLostCallbackInfo.userdata1 = this;
        WGPURequestDeviceCallbackInfo deviceCallback = {};
        deviceCallback.mode = WGPUCallbackMode_AllowSpontaneous;
        deviceCallback.callback = [](WGPURequestDeviceStatus status,
                                     WGPUDevice requestedDevice,
                                     WGPUStringView message, void* userdata1,
                                     void*) {
            auto* state = static_cast<DeviceState*>(userdata1);
            state->status = status;
            state->device = requestedDevice;
            state->message = stringView(message);
            state->complete.store(true, std::memory_order_release);
        };
        deviceCallback.userdata1 = &deviceState;
        wgpuAdapterRequestDevice(adapter, &deviceDescriptor, deviceCallback);
        while (!deviceState.complete.load(std::memory_order_acquire)) {
            wgpuInstanceProcessEvents(instance);
        }
        if (deviceState.status != WGPURequestDeviceStatus_Success ||
            !deviceState.device) {
            std::cerr << "WebGPU: device request failed: " << deviceState.message << "\n";
            return false;
        }
        device = deviceState.device;
        queue = wgpuDeviceGetQueue(device);
        if (!queue) {
            std::cerr << "WebGPU: wgpuDeviceGetQueue failed\n";
            return false;
        }
        return true;
    }

    bool healthy() const
    {
        return !uncapturedError.load(std::memory_order_acquire) &&
               !deviceLost.load(std::memory_order_acquire);
    }

    bool readTexture(WGPUTexture texture, uint32_t width, uint32_t height,
                     std::vector<unsigned char>& rgba)
    {
        constexpr uint32_t COPY_ALIGNMENT = 256;
        auto unpaddedRow = width * 4u;
        auto paddedRow = (unpaddedRow + COPY_ALIGNMENT - 1u) & ~(COPY_ALIGNMENT - 1u);
        uint64_t bufferSize = static_cast<uint64_t>(paddedRow) * height;

        WGPUBufferDescriptor bufferDescriptor = {};
        bufferDescriptor.label = {"ThorVG WebGPU AAA readback", WGPU_STRLEN};
        bufferDescriptor.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead;
        bufferDescriptor.size = bufferSize;
        auto buffer = wgpuDeviceCreateBuffer(device, &bufferDescriptor);
        if (!buffer) {
            std::cerr << "WebGPU: wgpuDeviceCreateBuffer failed\n";
            return false;
        }

        WGPUCommandEncoderDescriptor encoderDescriptor = {};
        auto encoder = wgpuDeviceCreateCommandEncoder(device, &encoderDescriptor);
        if (!encoder) {
            wgpuBufferRelease(buffer);
            std::cerr << "WebGPU: wgpuDeviceCreateCommandEncoder failed\n";
            return false;
        }
        WGPUTexelCopyTextureInfo source = {};
        source.texture = texture;
        source.aspect = WGPUTextureAspect_All;
        WGPUTexelCopyBufferInfo destination = {};
        destination.buffer = buffer;
        destination.layout.bytesPerRow = paddedRow;
        destination.layout.rowsPerImage = height;
        WGPUExtent3D extent = {width, height, 1};
        wgpuCommandEncoderCopyTextureToBuffer(encoder, &source, &destination, &extent);
        auto command = wgpuCommandEncoderFinish(encoder, nullptr);
        wgpuCommandEncoderRelease(encoder);
        if (!command) {
            wgpuBufferRelease(buffer);
            std::cerr << "WebGPU: wgpuCommandEncoderFinish failed\n";
            return false;
        }
        wgpuQueueSubmit(queue, 1, &command);
        wgpuCommandBufferRelease(command);

        struct MapState
        {
            WGPUMapAsyncStatus status = WGPUMapAsyncStatus_Unknown;
            std::string message;
            std::atomic<bool> complete{false};
        } mapState;
        WGPUBufferMapCallbackInfo mapCallback = {};
        mapCallback.mode = WGPUCallbackMode_AllowSpontaneous;
        mapCallback.callback = [](WGPUMapAsyncStatus status,
                                  WGPUStringView message, void* userdata1,
                                  void*) {
            auto* state = static_cast<MapState*>(userdata1);
            state->status = status;
            state->message = stringView(message);
            state->complete.store(true, std::memory_order_release);
        };
        mapCallback.userdata1 = &mapState;
        wgpuBufferMapAsync(buffer, WGPUMapMode_Read, 0,
                           static_cast<size_t>(bufferSize), mapCallback);
        while (!mapState.complete.load(std::memory_order_acquire)) {
            wgpuDevicePoll(device, true, nullptr);
            wgpuInstanceProcessEvents(instance);
        }
        if (mapState.status != WGPUMapAsyncStatus_Success) {
            std::cerr << "WebGPU: readback map failed: " << mapState.message << "\n";
            wgpuBufferRelease(buffer);
            return false;
        }

        auto* mapped = static_cast<const unsigned char*>(
            wgpuBufferGetConstMappedRange(buffer, 0, static_cast<size_t>(bufferSize)));
        if (!mapped) {
            std::cerr << "WebGPU: mapped range is null\n";
            wgpuBufferUnmap(buffer);
            wgpuBufferRelease(buffer);
            return false;
        }

        rgba.resize(static_cast<size_t>(width) * height * 4u);
        for (uint32_t y = 0; y < height; ++y) {
            const auto* input = mapped + static_cast<size_t>(y) * paddedRow;
            auto* output = rgba.data() + static_cast<size_t>(y) * unpaddedRow;
            for (uint32_t x = 0; x < width; ++x) {
                auto offset = static_cast<size_t>(x) * 4u;
                output[offset] = input[offset + 2u];
                output[offset + 1u] = input[offset + 1u];
                output[offset + 2u] = input[offset];
                output[offset + 3u] = input[offset + 3u];
            }
        }
        wgpuBufferUnmap(buffer);
        wgpuBufferRelease(buffer);
        return true;
    }
};

bool addWhiteBackground(tvg::Canvas& canvas, uint32_t width, uint32_t height,
                        const std::string& diagnostic)
{
    ShapePtr background(tvg::Shape::gen());
    if (!background) {
        std::cerr << diagnostic << ": Shape::gen for white background failed\n";
        return false;
    }
    if (!check(background->appendRect(0.0f, 0.0f, static_cast<float>(width),
                                      static_cast<float>(height)),
               "Shape::appendRect(background)", diagnostic) ||
        !check(background->fill(255, 255, 255, 255), "Shape::fill(background)",
               diagnostic) ||
        !check(canvas.add(background.get()), "Canvas::add(background)", diagnostic)) {
        return false;
    }
    background.release();
    return true;
}

bool renderCase(WebGpuContext& webgpu, tvg::WgCanvas& canvas,
                Suite suite, aa_poc::SceneKind scene, const ScaleSpec& scale,
                const Offset& offset, const std::string& outputPath,
                const std::string& diagnostic)
{
    WGPUTextureDescriptor textureDescriptor = {};
    textureDescriptor.label = {"ThorVG WebGPU AAA target", WGPU_STRLEN};
    textureDescriptor.usage = WGPUTextureUsage_CopySrc |
                              WGPUTextureUsage_CopyDst |
                              WGPUTextureUsage_RenderAttachment;
    textureDescriptor.dimension = WGPUTextureDimension_2D;
    textureDescriptor.size = {scale.targetWidth, scale.targetHeight, 1};
    textureDescriptor.format = WGPUTextureFormat_BGRA8Unorm;
    textureDescriptor.mipLevelCount = 1;
    textureDescriptor.sampleCount = 1;
    auto texture = wgpuDeviceCreateTexture(webgpu.device, &textureDescriptor);
    if (!texture) {
        std::cerr << diagnostic << ": wgpuDeviceCreateTexture failed\n";
        return false;
    }

    bool success = check(canvas.target({webgpu.instance, webgpu.adapter, webgpu.device},
                                       texture, scale.targetWidth, scale.targetHeight,
                                       tvg::ColorSpace::ABGR8888S, 1),
                         "WgCanvas::target", diagnostic) &&
                   addWhiteBackground(canvas, scale.targetWidth, scale.targetHeight,
                                      diagnostic);
    if (success) {
        success = suite == Suite::Product
                      ? aa_poc::populateProductScene(canvas, scene,
                                                     scale.coordinateScale,
                                                     offset.x, offset.y,
                                                     diagnostic.c_str())
                      : aa_poc::populateComparisonScene(
                            canvas, offset.x / scale.coordinateScale,
                            offset.y / scale.coordinateScale,
                            scale.coordinateScale, diagnostic.c_str());
    }
    success = success &&
                   check(canvas.update(), "Canvas::update", diagnostic) &&
                   check(canvas.draw(true), "Canvas::draw", diagnostic) &&
                   check(canvas.sync(), "Canvas::sync", diagnostic);

    std::vector<unsigned char> rgba;
    if (success) success = webgpu.readTexture(texture, scale.targetWidth,
                                              scale.targetHeight, rgba);
    if (success && !webgpu.healthy()) {
        std::cerr << diagnostic << ": WebGPU reported an uncaptured error or device loss\n";
        success = false;
    }
    if (success) {
        for (size_t index = 3; index < rgba.size(); index += 4) {
            if (rgba[index] != 255) {
                std::cerr << diagnostic << ": candidate is not opaque at pixel "
                          << (index / 4) << " (alpha=" << static_cast<unsigned>(rgba[index])
                          << ")\n";
                success = false;
                break;
            }
        }
    }
    if (success) {
        lodepng::State state;
        state.encoder.auto_convert = 0;
        state.info_raw.colortype = LCT_RGBA;
        state.info_raw.bitdepth = 8;
        state.info_png.color.colortype = LCT_RGBA;
        state.info_png.color.bitdepth = 8;
        std::vector<unsigned char> encoded;
        auto error = lodepng::encode(encoded, rgba, scale.targetWidth,
                                     scale.targetHeight, state);
        if (!error) error = lodepng::save_file(encoded, outputPath);
        if (error) {
            std::cerr << diagnostic << ": PNG encode failed: "
                      << lodepng_error_text(error) << "\n";
            success = false;
        }
    }

    if (!check(canvas.remove(), "Canvas::remove", diagnostic)) success = false;
    wgpuTextureDestroy(texture);
    wgpuTextureRelease(texture);
    return success;
}

bool writeMetadata(const std::string& path, const WebGpuContext& webgpu,
                   std::time_t runStarted, Suite suite)
{
    uint32_t major = 0, minor = 0, micro = 0;
    const char* version = tvg::Initializer::version(&major, &minor, &micro);
    std::ofstream output(path);
    if (!output) {
        std::cerr << "Cannot write metadata " << path << "\n";
        return false;
    }
    output << "{\n"
           << "  \"schema_version\": 1,\n"
           << "  \"renderer\": \"" << MODE << "\",\n"
           << "  \"renderer_version\": \"" << jsonEscape(version ? version : "unknown") << "\",\n"
           << "  \"backend\": \"WebGPU " << jsonEscape(webgpu.backend) << "\",\n"
           << "  \"adapter_feature_level\": \"Core\",\n"
           << "  \"gpu_device\": \"" << jsonEscape(webgpu.deviceName) << "\",\n"
           << "  \"gpu_vendor\": \"" << jsonEscape(webgpu.vendor) << "\",\n"
           << "  \"gpu_description\": \"" << jsonEscape(webgpu.description) << "\",\n"
           << "  \"antialiasing\": \"ThorVG WebGPU internal 4x MSAA resolve\",\n"
           << "  \"external_texture_format\": \"BGRA8Unorm\",\n"
           << "  \"external_texture_sample_count\": 1,\n"
           << "  \"base_color\": \"opaque white (#ffffffff)\",\n"
           << "  \"candidate_alpha\": \"opaque RGBA; runner rejects alpha other than 255\",\n"
           << "  \"presentation\": \"offscreen-no-swap (quality only)\",\n"
           << "  \"scene_contract\": \""
           << (suite == Suite::Product
                   ? "tools/aa_poc/aa_product_scenes.cpp"
                   : "tools/aa_poc/aa_poc_comparison_scene.cpp")
           << "\",\n"
           << "  \"reference\": \"ThorVG NoAa at 8x with premultiplied box downsampling\",\n"
           << "  \"matrix\": \""
           << (suite == Suite::Product
                   ? "4 scenes x 3 scales x 4 subpixel offsets = 48 rows"
                   : "1 original comparison scene x 3 display scales x 4 subpixel offsets = 12 frames; 8 characteristics scored separately")
           << "\",\n"
           << "  \"run_started_unix_seconds\": " << static_cast<long long>(runStarted) << "\n"
           << "}\n";
    return static_cast<bool>(output);
}

void usage(const char* executable)
{
    std::cerr << "Usage: " << executable
              << " --output-dir DIRECTORY [--suite product|comparison]\n";
}

}  // namespace

int main(int argc, char** argv)
{
    auto runStarted = std::time(nullptr);
    std::string outputDirectory;
    Suite suite = Suite::Product;
    for (int index = 1; index < argc; ++index) {
        if (std::strcmp(argv[index], "--output-dir") == 0 && index + 1 < argc) {
            outputDirectory = argv[++index];
        } else if (std::strcmp(argv[index], "--suite") == 0 && index + 1 < argc) {
            auto value = argv[++index];
            if (std::strcmp(value, "product") == 0) suite = Suite::Product;
            else if (std::strcmp(value, "comparison") == 0) suite = Suite::Comparison;
            else {
                usage(argv[0]);
                return 2;
            }
        } else if (std::strcmp(argv[index], "--help") == 0) {
            usage(argv[0]);
            return 0;
        } else {
            usage(argv[0]);
            return 2;
        }
    }
    while (outputDirectory.size() > 1 && outputDirectory.back() == '/') {
        outputDirectory.pop_back();
    }
    if (outputDirectory.empty() || !ensureDirectory(outputDirectory)) {
        usage(argv[0]);
        return 2;
    }

    WebGpuContext webgpu;
    if (!webgpu.initialize()) return 1;
    if (tvg::Initializer::init() != tvg::Result::Success) {
        std::cerr << "ThorVG Initializer::init failed\n";
        return 1;
    }

    bool success = true;
    std::unique_ptr<tvg::WgCanvas> canvas(tvg::WgCanvas::gen());
    if (!canvas) {
        std::cerr << "WgCanvas::gen failed; build ThorVG with -Dengines=wg\n";
        success = false;
    }

    auto manifestPath = outputDirectory +
                        (suite == Suite::Product
                             ? "/thorvg-webgpu-quality-manifest.tsv"
                             : "/thorvg-webgpu-comparison-manifest.tsv");
    std::ofstream manifest(manifestPath);
    if (!manifest) {
        std::cerr << "Cannot write manifest " << manifestPath << "\n";
        success = false;
    } else {
        manifest << "mode\tscene\tscale\toffset_x\toffset_y\tcandidate_png\n";
    }

    if (success) {
        const auto renderMatrix = [&](aa_poc::SceneKind scene,
                                      const ScaleSpec* scales,
                                      size_t scaleCount,
                                      const char* sceneName,
                                      const char* rootDirectory) {
            for (size_t scaleIndex = 0; scaleIndex < scaleCount; ++scaleIndex) {
                const auto& scale = scales[scaleIndex];
                for (const auto& offset : OFFSETS) {
                    auto offsetDirectory = std::string("offset-x") +
                                           offsetComponent(offset.x) + "-y" +
                                           offsetComponent(offset.y);
                    auto relativeDirectory = std::string(rootDirectory) + "/" +
                                             sceneName + "/" + scale.name + "/" +
                                             offsetDirectory;
                    auto absoluteDirectory = outputDirectory + "/" + relativeDirectory;
                    if (!ensureDirectory(absoluteDirectory)) {
                        return false;
                    }
                    auto candidateRelative = relativeDirectory + "/" + MODE + ".png";
                    auto candidateAbsolute = outputDirectory + "/" + candidateRelative;
                    auto diagnostic = std::string(MODE) + " " + sceneName + "/" +
                                      scale.name + " offset(" + formatNumber(offset.x) +
                                      "," + formatNumber(offset.y) + ")";
                    if (!renderCase(webgpu, *canvas, suite, scene, scale, offset,
                                    candidateAbsolute, diagnostic)) {
                        return false;
                    }
                    manifest << MODE << '\t' << sceneName << '\t' << scale.name << '\t'
                             << formatNumber(offset.x) << '\t' << formatNumber(offset.y)
                             << '\t' << candidateRelative << '\n';
                    if (!manifest) {
                        std::cerr << "Failed writing manifest " << manifestPath << "\n";
                        return false;
                    }
                    std::cout << "Rendered " << candidateRelative << "\n";
                }
            }
            return true;
        };

        if (suite == Suite::Product) {
            for (auto scene : SCENES) {
                if (!renderMatrix(scene, PRODUCT_SCALES,
                                  sizeof(PRODUCT_SCALES) / sizeof(PRODUCT_SCALES[0]),
                                  aa_poc::sceneName(scene), "quality")) {
                    success = false;
                    break;
                }
            }
        } else {
            success = renderMatrix(aa_poc::SceneKind::FlatCore, COMPARISON_SCALES,
                                   sizeof(COMPARISON_SCALES) /
                                       sizeof(COMPARISON_SCALES[0]),
                                   "comparison", "diagnostic");
        }
    }
    manifest.flush();
    if (!manifest) {
        std::cerr << "Failed flushing manifest " << manifestPath << "\n";
        success = false;
    }
    manifest.close();
    if (!manifest) {
        std::cerr << "Failed closing manifest " << manifestPath << "\n";
        success = false;
    }

    if (success) {
        success = writeMetadata(outputDirectory +
                                    (suite == Suite::Product
                                         ? "/thorvg-webgpu-renderer-metadata.json"
                                         : "/thorvg-webgpu-comparison-metadata.json"),
                                webgpu, runStarted, suite);
    }
    canvas.reset();
    if (tvg::Initializer::term() != tvg::Result::Success) {
        std::cerr << "ThorVG Initializer::term failed\n";
        success = false;
    }
    if (!success) return 1;
    std::cout << "Wrote " << manifestPath << "\n";
    return 0;
}
