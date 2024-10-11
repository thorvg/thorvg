/*
 * Copyright (c) 2020 - 2024 the ThorVG project. All rights reserved.

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
#ifdef THORVG_WG_RASTER_SUPPORT
    #include <webgpu/webgpu.h>
#endif

using namespace emscripten;
using namespace std;
using namespace tvg;

static const char* NoError = "None";

#ifdef THORVG_WG_RASTER_SUPPORT
    static WGPUInstance instance{};
    static WGPUAdapter adapter{};
    static WGPUDevice device{};
#endif

void init()
{
#ifdef THORVG_WG_RASTER_SUPPORT
    //Init WebGPU
    if (!instance) instance = wgpuCreateInstance(nullptr);

    // request adapter
    if (!adapter) {
        const WGPURequestAdapterOptions requestAdapterOptions { .nextInChain = nullptr, .powerPreference = WGPUPowerPreference_HighPerformance, .forceFallbackAdapter = false };
        auto onAdapterRequestEnded = [](WGPURequestAdapterStatus status, WGPUAdapter adapter, char const * message, void * pUserData) { *((WGPUAdapter*)pUserData) = adapter; };
        wgpuInstanceRequestAdapter(instance, &requestAdapterOptions, onAdapterRequestEnded, &adapter);
        while (!adapter) emscripten_sleep(10);
    }

    // request device
    WGPUFeatureName featureNames[32]{};
    size_t featuresCount = wgpuAdapterEnumerateFeatures(adapter, featureNames);
    if (!device) {
        const WGPUDeviceDescriptor deviceDesc { .nextInChain = nullptr, .label = "The device", .requiredFeatureCount = featuresCount, .requiredFeatures = featureNames };
        auto onDeviceRequestEnded = [](WGPURequestDeviceStatus status, WGPUDevice device, char const * message, void * pUserData) { *((WGPUDevice*)pUserData) = device; };
        wgpuAdapterRequestDevice(adapter, &deviceDesc, onDeviceRequestEnded, &device);
        while (!device) emscripten_sleep(10);
    }
#endif
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
    virtual unique_ptr<Canvas> init(string&) = 0;
    virtual void resize(Canvas* canvas, int w, int h) = 0;
    virtual val output(int w, int h)
    {
        return val(typed_memory_view<uint8_t>(0, nullptr));
    }
};

struct TvgSwEngine : TvgEngineMethod
{
    uint8_t* buffer = nullptr;

    ~TvgSwEngine()
    {
        free(buffer);
        Initializer::term(tvg::CanvasEngine::Sw);
    }

    unique_ptr<Canvas> init(string&) override
    {
        Initializer::init(0, tvg::CanvasEngine::Sw);
        return SwCanvas::gen();
    }

    void resize(Canvas* canvas, int w, int h) override
    {
        free(buffer);
        buffer = (uint8_t*)malloc(w * h * sizeof(uint32_t));
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
        #endif
        Initializer::term(tvg::CanvasEngine::Wg);
    }

    unique_ptr<Canvas> init(string& selector) override
    {
        #ifdef THORVG_WG_RASTER_SUPPORT
            WGPUSurfaceDescriptorFromCanvasHTMLSelector canvasDesc{};
            canvasDesc.chain.next = nullptr;
            canvasDesc.chain.sType = WGPUSType_SurfaceDescriptorFromCanvasHTMLSelector;
            canvasDesc.selector = selector.c_str();

            WGPUSurfaceDescriptor surfaceDesc{};
            surfaceDesc.nextInChain = &canvasDesc.chain;
            surface = wgpuInstanceCreateSurface(instance, &surfaceDesc);
        #endif

        Initializer::init(0, tvg::CanvasEngine::Wg);
        return WgCanvas::gen();
    }

    void resize(Canvas* canvas, int w, int h) override
    {
    #ifdef THORVG_WG_RASTER_SUPPORT
        static_cast<WgCanvas*>(canvas)->target(instance, surface, w, h, device);
    #endif
    }
};


class __attribute__((visibility("default"))) TvgLottieAnimation
{
public:
    ~TvgLottieAnimation()
    {
        delete(engine);
    }

    explicit TvgLottieAnimation(string engine = "sw", string selector = "")
    {
        errorMsg = NoError;

        if (engine == "sw") this->engine = new TvgSwEngine;
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

        this->data = data; //back up for saving

        canvas->clear(true);

        animation = Animation::gen();

        string filetype = mimetype;
        if (filetype == "json") {
            filetype = "lottie";
        }

        if (animation->picture()->load(data.c_str(), data.size(), filetype, rpath, false) != Result::Success) {
            errorMsg = "load() fail";
            return false;
        }

        animation->picture()->size(&psize[0], &psize[1]);

        /* need to reset size to calculate scale in Picture.size internally before calling resize() */
        this->width = 0;
        this->height = 0;

        resize(width, height);

        if (canvas->push(cast(animation->picture())) != Result::Success) {
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

        if (canvas->draw() != Result::Success) {
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

        this->canvas->clear(false);

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

        engine->resize(canvas.get(), width, height);

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
    bool save(string mimetype)
    {
        if (mimetype == "gif") {
            return save2Gif();
        } else if (mimetype == "tvg") {
            return save2Tvg();
        }

        errorMsg = "Invalid mimetype";
        return false;
    }

    bool save2Gif()
    {
        errorMsg = NoError;

        if (data.empty()) {
            errorMsg = "Invalid data";
            return false;
        }

        auto saver = Saver::gen();
        if (!saver) {
            errorMsg = "Invalid saver";
            return false;
        }

        //animation to save
        auto animation = Animation::gen();
        if (!animation) {
            errorMsg = "Invalid animation";
            return false;
        }

        if (animation->picture()->load(data.c_str(), data.size(), "lottie", nullptr, false) != Result::Success) {
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

        if (saver->save(std::move(animation), "output.gif", 100, 30) != tvg::Result::Success) {
            errorMsg = "save(), fail";
            return false;
        }

        saver->sync();

        return true;
    }

    bool save2Tvg()
    {
        errorMsg = NoError;

        if (!animation) return false;

        auto saver = Saver::gen();
        if (!saver) {
            errorMsg = "Invalid saver";
            return false;
        }

        //preserve the picture using the reference counting
        PP(animation->picture())->ref();

        if (saver->save(tvg::cast<Picture>(animation->picture()), "output.tvg") != tvg::Result::Success) {
            PP(animation->picture())->unref();
            errorMsg = "save(), fail";
            return false;
        }

        saver->sync();

        PP(animation->picture())->unref();

        return true;
    }

    // TODO: Advanced APIs wrt Interactivity & theme methods...

private:
    string                 errorMsg;
    unique_ptr<Canvas>     canvas = nullptr;
    unique_ptr<Animation>  animation = nullptr;
    TvgEngineMethod*       engine = nullptr;
    string                 data;
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
