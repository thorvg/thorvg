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

#include <thorvg.h>
#include <emscripten.h>
#include <emscripten/bind.h>
#include "tvgPicture.h"
#include "tvgWasmDefaultFont.h"
#ifdef THORVG_WG_RASTER_SUPPORT
    #include <emscripten/html5_webgpu.h>
#endif
#ifdef THORVG_GL_RASTER_SUPPORT
    #include <emscripten/html5_webgl.h>
#endif

using namespace emscripten;
using namespace std;
using namespace tvg;

static const char* NoError = "None";

#ifdef THORVG_WG_RASTER_SUPPORT
    static WGPUInstance instance{};
    static WGPUAdapter adapter{};
    static WGPUDevice device{};
    static bool adapterRequested = false;
    static bool deviceRequested = false;
    static bool initializationFailed = false;
#endif

// 0: success, 1: fail, 2: wait for async request
int init()
{
#ifdef THORVG_WG_RASTER_SUPPORT
    if (initializationFailed) return 1;

    //Init WebGPU
    if (!instance) instance = wgpuCreateInstance(nullptr);

    // request adapter
    if (!adapter) {
        if (adapterRequested) return 2;

        const WGPURequestAdapterOptions requestAdapterOptions { .nextInChain = nullptr, .powerPreference = WGPUPowerPreference_HighPerformance, .forceFallbackAdapter = false };
        auto onAdapterRequestEnded = [](WGPURequestAdapterStatus status, WGPUAdapter adapter, char const* message, void* pUserData) { 
            if (status != WGPURequestAdapterStatus_Success) {
                initializationFailed = true;
                return;
            }
            *((WGPUAdapter*)pUserData) = adapter;
        };
        wgpuInstanceRequestAdapter(instance, &requestAdapterOptions, onAdapterRequestEnded, &adapter);

        adapterRequested = true;
        return 2;
    }

    // request device
    if (deviceRequested) return device == nullptr ? 2 : 0;

    WGPUFeatureName featureNames[32]{};
    size_t featuresCount = wgpuAdapterEnumerateFeatures(adapter, featureNames);
    if (!device) {
        const WGPUDeviceDescriptor deviceDesc { .nextInChain = nullptr, .label = "The device", .requiredFeatureCount = featuresCount, .requiredFeatures = featureNames };
        auto onDeviceRequestEnded = [](WGPURequestDeviceStatus status, WGPUDevice device, char const* message, void* pUserData) {
            if (status != WGPURequestDeviceStatus_Success) {
                initializationFailed = true;
                return;
            }
            *((WGPUDevice*)pUserData) = device; 
        };
        wgpuAdapterRequestDevice(adapter, &deviceDesc, onDeviceRequestEnded, &device);

        deviceRequested = true;
        return 2;
    }
#endif

    return 0;
}

void term()
{
#ifdef THORVG_WG_RASTER_SUPPORT
    wgpuDeviceRelease(device);
    wgpuAdapterRelease(adapter);
    wgpuInstanceRelease(instance);
#endif
}

struct TvgEngineMethod
{
    virtual ~TvgEngineMethod() {}
    virtual Canvas* init(string&) = 0;
    virtual void resize(Canvas* canvas, int w, int h) = 0;
    virtual val output(int w, int h)
    {
        return val(typed_memory_view<uint8_t>(0, nullptr));
    }

    void loadFont() {
        Text::load("default", requestFont(), DEFAULT_FONT_SIZE, "ttf", false);
    }
};

struct TvgSwEngine : TvgEngineMethod
{
    uint8_t* buffer = nullptr;

    ~TvgSwEngine()
    {
        std::free(buffer);
        Initializer::term(tvg::CanvasEngine::Sw);
        retrieveFont();
    }

    Canvas* init(string&) override
    {
        Initializer::init(0, tvg::CanvasEngine::Sw);
        loadFont();
        return SwCanvas::gen();
    }

    void resize(Canvas* canvas, int w, int h) override
    {
        std::free(buffer);
        buffer = (uint8_t*)std::malloc(w * h * sizeof(uint32_t));
        static_cast<SwCanvas*>(canvas)->target((uint32_t *)buffer, w, w, h, ColorSpace::ABGR8888S);
    }

    val output(int w, int h) override
    {
        return val(typed_memory_view(w * h * 4, buffer));
    }
};


struct TvgWgEngine : TvgEngineMethod
{
#ifdef THORVG_WG_RASTER_SUPPORT
    WGPUSurface surface{};
#endif

    ~TvgWgEngine()
    {
    #ifdef THORVG_WG_RASTER_SUPPORT
        wgpuSurfaceRelease(surface);
        Initializer::term(tvg::CanvasEngine::Wg);
        retrieveFont();
    #endif
    }

    Canvas* init(string& selector) override
    {
    #ifdef THORVG_WG_RASTER_SUPPORT
        WGPUSurfaceDescriptorFromCanvasHTMLSelector canvasDesc{};
        canvasDesc.chain.next = nullptr;
        canvasDesc.chain.sType = WGPUSType_SurfaceDescriptorFromCanvasHTMLSelector;
        canvasDesc.selector = selector.c_str();

        WGPUSurfaceDescriptor surfaceDesc{};
        surfaceDesc.nextInChain = &canvasDesc.chain;
        surface = wgpuInstanceCreateSurface(instance, &surfaceDesc);

        Initializer::init(0, tvg::CanvasEngine::Wg);
        loadFont();
        return WgCanvas::gen();
    #else
        return nullptr;
    #endif
    }

    void resize(Canvas* canvas, int w, int h) override
    {
        if (canvas) static_cast<WgCanvas*>(canvas)->target(device, instance, surface, w, h, ColorSpace::ABGR8888S);
    }
};

struct TvgGLEngine : TvgEngineMethod
{
    intptr_t context = 0;
    ~TvgGLEngine() override
    {
    #ifdef THORVG_GL_RASTER_SUPPORT
        if (context) {
            Initializer::term(tvg::CanvasEngine::Gl);
            emscripten_webgl_destroy_context(context);
            context = 0;
        }
        retrieveFont();
    #endif
    }

    Canvas* init(string& selector) override
    {
    #ifdef THORVG_GL_RASTER_SUPPORT
        EmscriptenWebGLContextAttributes attrs{};
        attrs.alpha = true;
        attrs.depth = false;
        attrs.stencil = false;
        attrs.premultipliedAlpha = true;
        attrs.failIfMajorPerformanceCaveat = false;
        attrs.majorVersion = 2;
        attrs.minorVersion = 0;
        attrs.enableExtensionsByDefault = true;

        context = emscripten_webgl_create_context(selector.c_str(), &attrs);
        if (context == 0) return nullptr;

        emscripten_webgl_make_context_current(context);

        if (Initializer::init(0, tvg::CanvasEngine::Gl) != Result::Success) return nullptr;
        loadFont();

        return GlCanvas::gen();
    #else
        return nullptr;
    #endif
    }

    void resize(Canvas* canvas, int w, int h) override
    {
        if (canvas) static_cast<GlCanvas*>(canvas)->target((void*)context, 0, w, h, ColorSpace::ABGR8888S);
    }
};


class __attribute__((visibility("default"))) TvgLottieAnimation
{
public:
    ~TvgLottieAnimation()
    {
        delete(animation);
        delete(canvas);
        delete(engine);
    }

    explicit TvgLottieAnimation(string engine = "sw", string selector = "")
    {
        errorMsg = NoError;

        if (engine == "sw") this->engine = new TvgSwEngine;
        else if (engine == "gl") this->engine = new TvgGLEngine;
        else if (engine == "wg") this->engine = new TvgWgEngine;

        if (!this->engine) {
            errorMsg = "Invalid engine";
            return;
        }

        canvas = this->engine->init(selector);

        if (!canvas) {
            errorMsg = "Unsupported!";
            return;
        }

        animation = Animation::gen();
        if (!animation) errorMsg = "Invalid animation";
    }

    string error()
    {
        return errorMsg;
    }

    // Getter methods
    val size()
    {
        return val(typed_memory_view(2, psize));
    }

    float duration()
    {
        if (!canvas || !animation) return 0;
        return animation->duration();
    }

    float totalFrame()
    {
        if (!canvas || !animation) return 0;
        return animation->totalFrame();
    }

    float curFrame()
    {
        if (!canvas || !animation) return 0;
        return animation->curFrame();
    }

    // Render methods
    bool load(string data, string mimetype, int width, int height, string rpath = "")
    {
        errorMsg = NoError;

        if (!canvas) return false;

        if (data.empty()) {
            errorMsg = "Invalid data";
            return false;
        }

        canvas->remove();

        delete(animation);
        animation = Animation::gen();

        string filetype = mimetype;
        if (filetype == "json") {
            filetype = "lottie+json";
        }

        if (animation->picture()->load(data.c_str(), data.size(), filetype.c_str(), rpath.c_str(), false) != Result::Success) {
            errorMsg = "load() fail";
            return false;
        }

        animation->picture()->size(&psize[0], &psize[1]);

        /* need to reset size to calculate scale in Picture.size internally before calling resize() */
        this->width = 0;
        this->height = 0;

        resize(width, height);

        if (canvas->push(animation->picture()) != Result::Success) {
            errorMsg = "push() fail";
            return false;
        }

        updated = true;

        return true;
    }

    val render()
    {
        errorMsg = NoError;

        if (!canvas || !animation) return val(typed_memory_view<uint8_t>(0, nullptr));

        if (!updated) return engine->output(width, height);

        if (canvas->draw(true) != Result::Success) {
            errorMsg = "draw() fail";
            return val(typed_memory_view<uint8_t>(0, nullptr));
        }

        canvas->sync();

        updated = false;

        return engine->output(width, height);
    }

    bool update()
    {
        if (!updated) return true;

        errorMsg = NoError;

        if (canvas->update() != Result::Success) {
            errorMsg = "update() fail";
            return false;
        }

        return true;
    }

    bool frame(float no)
    {
        if (!canvas || !animation) return false;
        if (animation->frame(no) == Result::Success) {
            updated = true;
        }
        return true;
    }

    bool viewport(float x, float y, float width, float height)
    {
        if (!canvas || !animation) return false;
        if (canvas->viewport(x, y, width, height) != Result::Success) {
            errorMsg = "viewport() fail";
            return false;
        }

        return true;
    }

    void resize(int width, int height)
    {
        if (!canvas || !animation) return;
        if (this->width == width && this->height == height) return;

        canvas->sync();

        this->width = width;
        this->height = height;

        engine->resize(canvas, width, height);

        float scale;
        float shiftX = 0.0f, shiftY = 0.0f;
        if (psize[0] > psize[1]) {
            scale = width / psize[0];
            shiftY = (height - psize[1] * scale) * 0.5f;
        } else {
            scale = height / psize[1];
            shiftX = (width - psize[0] * scale) * 0.5f;
        }
        animation->picture()->scale(scale);
        animation->picture()->translate(shiftX, shiftY);

        updated = true;
    }

    // Saver methods
    bool save(string data, string mimetype)
    {
        if (mimetype == "gif") return save2Gif(data);

        errorMsg = "Invalid mimetype";
        return false;
    }

    bool save2Gif(string data)
    {
        errorMsg = NoError;

        if (data.empty()) {
            errorMsg = "Invalid data";
            return false;
        }

        auto saver = unique_ptr<Saver>(Saver::gen());
        if (!saver) {
            errorMsg = "Invalid saver";
            return false;
        }

        //animation to save
        auto animation = unique_ptr<Animation>(Animation::gen());
        if (!animation) {
            errorMsg = "Invalid animation";
            return false;
        }

        if (animation->picture()->load(data.c_str(), data.size(), "lot", nullptr, false) != Result::Success) {
            errorMsg = "load() fail";
            return false;
        }

        //gif resolution (600x600)
        constexpr float GIF_SIZE = 600;

        //transform
        float width, height;
        animation->picture()->size(&width, &height);
        float scale;
        if (width > height) scale = GIF_SIZE / width;
        else scale = GIF_SIZE / height;
        animation->picture()->size(width * scale, height * scale);

        //set a white opaque background
        auto bg = tvg::Shape::gen();
        if (!bg) {
            errorMsg = "Invalid bg";
            return false;
        }
        bg->fill(255, 255, 255, 255);
        bg->appendRect(0, 0, GIF_SIZE, GIF_SIZE);

        if (saver->background(std::move(bg)) != Result::Success) {
            errorMsg = "background() fail";
            return false;
        }

        if (saver->save(animation.release(), "output.gif", 100, 30) != tvg::Result::Success) {
            errorMsg = "save(), fail";
            return false;
        }

        saver->sync();

        return true;
    }

    // TODO: Advanced APIs wrt Interactivity & theme methods...

private:
    string                 errorMsg;
    Canvas*                canvas = nullptr;
    Animation*             animation = nullptr;
    TvgEngineMethod*       engine = nullptr;
    uint32_t               width = 0;
    uint32_t               height = 0;
    float                  psize[2];         //picture size
    bool                   updated = false;
};

EMSCRIPTEN_BINDINGS(thorvg_bindings)
{
    emscripten::function("init", &init);
    emscripten::function("term", &term);

    class_<TvgLottieAnimation>("TvgLottieAnimation")
        .constructor<string, string>()
        .function("error", &TvgLottieAnimation ::error, allow_raw_pointers())
        .function("size", &TvgLottieAnimation ::size)
        .function("duration", &TvgLottieAnimation ::duration)
        .function("totalFrame", &TvgLottieAnimation ::totalFrame)
        .function("curFrame", &TvgLottieAnimation ::curFrame)
        .function("render", &TvgLottieAnimation::render)
        .function("load", &TvgLottieAnimation ::load)
        .function("update", &TvgLottieAnimation ::update)
        .function("frame", &TvgLottieAnimation ::frame)
        .function("viewport", &TvgLottieAnimation ::viewport)
        .function("resize", &TvgLottieAnimation ::resize)
        .function("save", &TvgLottieAnimation ::save);
}
