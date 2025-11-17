/*
 * Copyright (c) 2020 - 2025 the ThorVG project. All rights reserved.

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "tvgCommon.h"
#include "thorvg_capi.h"
#include <emscripten.h>
#include <emscripten/bind.h>
#include <string>

using namespace tvg;
using emscripten::class_;
using emscripten::val;
using emscripten::typed_memory_view;
using std::string;

// ============================================================
// WebGPU Global State
// ============================================================

#ifdef THORVG_WG_RASTER_SUPPORT
#include <webgpu/webgpu.h>

static WGPUInstance wgpu_instance{};
static WGPUAdapter wgpu_adapter{};
static WGPUDevice wgpu_device{};
static bool adapter_requested = false;
static bool device_requested = false;
static bool initialization_failed = false;
#endif

#ifdef THORVG_GL_RASTER_SUPPORT
#include <emscripten/html5_webgl.h>
#endif

// ============================================================
// Canvas Kit Initialization Functions
// ============================================================

// Init function for all backends (WebGPU needs async, others return immediately)
// Returns: 0 = ready, 1 = failed, 2 = pending
static int init() {
#ifdef THORVG_WG_RASTER_SUPPORT
    if (initialization_failed) return 1;

    // Init WebGPU instance
    if (!wgpu_instance) {
        wgpu_instance = wgpuCreateInstance(nullptr);
    }

    // Request adapter
    if (!wgpu_adapter) {
        if (adapter_requested) return 2;

        auto onAdapterRequestEnded = [](WGPURequestAdapterStatus status, WGPUAdapter adapter,
                                       WGPUStringView message, WGPU_NULLABLE void* userdata1,
                                       WGPU_NULLABLE void* userdata2) {
            if (status != WGPURequestAdapterStatus_Success) {
                initialization_failed = true;
                return;
            }
            *((WGPUAdapter*)userdata1) = adapter;
        };

        const WGPURequestAdapterOptions requestAdapterOptions{
            .powerPreference = WGPUPowerPreference_HighPerformance
        };
        const WGPURequestAdapterCallbackInfo requestAdapterCallback{
            .mode = WGPUCallbackMode_AllowSpontaneous,
            .callback = onAdapterRequestEnded,
            .userdata1 = &wgpu_adapter
        };
        wgpuInstanceRequestAdapter(wgpu_instance, &requestAdapterOptions, requestAdapterCallback);

        adapter_requested = true;
        return 2;
    }

    // Request device
    if (device_requested) return wgpu_device == nullptr ? 2 : 0;

    if (!wgpu_device) {
        auto onDeviceError = [](WGPUDevice const * device, WGPUErrorType type,
                               WGPUStringView message, void* userdata1, void* userdata2) {};
        auto onDeviceRequestEnded = [](WGPURequestDeviceStatus status, WGPUDevice device,
                                      WGPUStringView message, void* userdata1, void* userdata2) {
            if (status != WGPURequestDeviceStatus_Success) {
                initialization_failed = true;
                return;
            }
            *((WGPUDevice*)userdata1) = device;
        };

        const WGPUDeviceDescriptor deviceDesc {
            .label = { "ThorVG Device", WGPU_STRLEN },
            .uncapturedErrorCallbackInfo = { .callback = onDeviceError }
        };
        const WGPURequestDeviceCallbackInfo requestDeviceCallback {
            .mode = WGPUCallbackMode_AllowSpontaneous,
            .callback = onDeviceRequestEnded,
            .userdata1 = &wgpu_device
        };
        wgpuAdapterRequestDevice(wgpu_adapter, &deviceDesc, requestDeviceCallback);

        device_requested = true;
        return 2;
    }
    return 0;
#else
    return 0; // SW/GL don't need async init
#endif
}

// Term function for cleanup
static void term() {
#ifdef THORVG_WG_RASTER_SUPPORT
    if (wgpu_device) wgpuDeviceRelease(wgpu_device);
    if (wgpu_adapter) wgpuAdapterRelease(wgpu_adapter);
    if (wgpu_instance) wgpuInstanceRelease(wgpu_instance);
    wgpu_device = nullptr;
    wgpu_adapter = nullptr;
    wgpu_instance = nullptr;
    adapter_requested = false;
    device_requested = false;
    initialization_failed = false;
#endif
}

// ============================================================
// ThorVG Engine Wrapper (handles complex initialization)
// ============================================================

class ThorVGEngine {
private:
    Canvas* canvas = nullptr;
    uint8_t* buffer = nullptr;
    uint32_t width = 0;
    uint32_t height = 0;
    string engine_type;

#ifdef THORVG_WG_RASTER_SUPPORT
    WGPUSurface surface{};
#endif

#ifdef THORVG_GL_RASTER_SUPPORT
    intptr_t gl_context = 0;
#endif

public:
    ~ThorVGEngine() {
        if (canvas) delete canvas;
        if (buffer) free(buffer);
        Initializer::term();

#ifdef THORVG_WG_RASTER_SUPPORT
        if (surface) wgpuSurfaceRelease(surface);
#endif

#ifdef THORVG_GL_RASTER_SUPPORT
        if (gl_context) {
            emscripten_webgl_destroy_context(gl_context);
            gl_context = 0;
        }
#endif
    }

    // Create canvas with engine-specific initialization
    // Returns canvas pointer as integer (compatible with C API)
    uintptr_t createCanvas(string engine, string selector, uint32_t w, uint32_t h) {
        engine_type = engine;
        width = w;
        height = h;

        // Initialize ThorVG
        if (Initializer::init() != Result::Success) {
            return 0;
        }

        if (engine == "sw") {
#ifdef THORVG_SW_RASTER_SUPPORT
            canvas = SwCanvas::gen();
            if (!canvas) return 0;

            buffer = (uint8_t*)malloc(w * h * 4);
            if (!buffer) {
                delete canvas;
                canvas = nullptr;
                return 0;
            }

            static_cast<SwCanvas*>(canvas)->target(
                (uint32_t*)buffer, w, w, h, ColorSpace::ABGR8888S
            );
#endif
        }
#ifdef THORVG_GL_RASTER_SUPPORT
        else if (engine == "gl") {
            EmscriptenWebGLContextAttributes attrs{};
            attrs.alpha = true;
            attrs.depth = false;
            attrs.stencil = false;
            attrs.premultipliedAlpha = true;
            attrs.failIfMajorPerformanceCaveat = false;
            attrs.majorVersion = 2;
            attrs.minorVersion = 0;
            attrs.enableExtensionsByDefault = true;

            gl_context = emscripten_webgl_create_context(selector.c_str(), &attrs);
            if (gl_context == 0) return 0;

            emscripten_webgl_make_context_current(gl_context);

            canvas = GlCanvas::gen();
            if (!canvas) return 0;

            static_cast<GlCanvas*>(canvas)->target(
                (void*)gl_context, 0, w, h, ColorSpace::ABGR8888S
            );
        }
#endif
#ifdef THORVG_WG_RASTER_SUPPORT
        else if (engine == "wg") {
            // Create WebGPU surface
            WGPUEmscriptenSurfaceSourceCanvasHTMLSelector canvasDesc{};
            canvasDesc.chain.next = nullptr;
            canvasDesc.chain.sType = WGPUSType_EmscriptenSurfaceSourceCanvasHTMLSelector;
            canvasDesc.selector.data = selector.c_str();
            canvasDesc.selector.length = WGPU_STRLEN;

            WGPUSurfaceDescriptor surfaceDesc{};
            surfaceDesc.nextInChain = &canvasDesc.chain;
            surface = wgpuInstanceCreateSurface(wgpu_instance, &surfaceDesc);

            if (!surface) return 0;

            canvas = WgCanvas::gen();
            if (!canvas) return 0;

            static_cast<WgCanvas*>(canvas)->target(
                wgpu_device, wgpu_instance, surface, w, h, ColorSpace::ABGR8888S
            );
        }
#endif

        // Return canvas pointer as integer (C API compatible)
        return reinterpret_cast<uintptr_t>(canvas);
    }

    // Resize canvas
    bool resize(uint32_t w, uint32_t h) {
        if (!canvas) return false;
        if (width == w && height == h) return true;

        canvas->sync();

        width = w;
        height = h;

        if (engine_type == "sw") {
#ifdef THORVG_SW_RASTER_SUPPORT
            free(buffer);
            buffer = (uint8_t*)malloc(w * h * 4);
            if (!buffer) return false;
            static_cast<SwCanvas*>(canvas)->target(
                (uint32_t*)buffer, w, w, h, ColorSpace::ABGR8888S
            );
#endif
        }
#ifdef THORVG_GL_RASTER_SUPPORT
        else if (engine_type == "gl") {
            static_cast<GlCanvas*>(canvas)->target(
                (void*)gl_context, 0, w, h, ColorSpace::ABGR8888S
            );
        }
#endif
#ifdef THORVG_WG_RASTER_SUPPORT
        else if (engine_type == "wg") {
            static_cast<WgCanvas*>(canvas)->target(
                wgpu_device, wgpu_instance, surface, w, h, ColorSpace::ABGR8888S
            );
        }
#endif

        return true;
    }

    // Get buffer for SW engine
    val getBuffer() {
        if (buffer && engine_type == "sw") {
            return val(typed_memory_view(width * height * 4, buffer));
        }
        return val::undefined();
    }

    // Get canvas pointer
    uintptr_t getCanvas() {
        return reinterpret_cast<uintptr_t>(canvas);
    }

    uint32_t getWidth() { return width; }
    uint32_t getHeight() { return height; }
    string getEngineType() { return engine_type; }
};

// ============================================================
// Emscripten Bindings
// ============================================================

EMSCRIPTEN_BINDINGS(thorvg_canvaskit) {
    // Canvas Kit initialization functions
    emscripten::function("init", &init);
    emscripten::function("term", &term);

    // Engine wrapper class
    class_<ThorVGEngine>("ThorVGEngine")
        .constructor<>()
        .function("createCanvas", &ThorVGEngine::createCanvas)
        .function("resize", &ThorVGEngine::resize)
        .function("getBuffer", &ThorVGEngine::getBuffer)
        .function("getCanvas", &ThorVGEngine::getCanvas)
        .function("getWidth", &ThorVGEngine::getWidth)
        .function("getHeight", &ThorVGEngine::getHeight)
        .function("getEngineType", &ThorVGEngine::getEngineType);
}

// ============================================================
// C API functions are exported via -sEXPORTED_FUNCTIONS
// No embind binding needed - JavaScript calls them directly as Module._tvg_xxx()
// ============================================================
