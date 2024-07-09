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

#include "Example.h"

#define WIDTH 1024
#define HEIGHT 1024
#define NUM_PER_LINE 4
#define SIZE (WIDTH/NUM_PER_LINE)


/************************************************************************/
/* ThorVG Drawing Contents                                              */
/************************************************************************/
void content(tvg::Canvas* canvas)
{
    //Background
    auto bg = tvg::Shape::gen();
    bg->appendRect(0, 0, SIZE, SIZE);
    bg->fill(255, 255, 255);
    canvas->push(std::move(bg));

    char buf[PATH_MAX];
    snprintf(buf, sizeof(buf), EXAMPLE_DIR"/svg/logo.svg");

    auto picture = tvg::Picture::gen();
    if (!tvgexam::verify(picture->load(buf))) return;

    float scale;
    float shiftX = 0.0f, shiftY = 0.0f;
    float w2, h2;
    picture->size(&w2, &h2);
    if (w2 > h2) {
        scale = SIZE / w2;
        shiftY = (SIZE - h2 * scale) * 0.5f;
    } else {
        scale = SIZE / h2;
        shiftX = (SIZE - w2 * scale) * 0.5f;
    }
    picture->translate(shiftX, shiftY);
    picture->scale(scale);

    canvas->push(std::move(picture));
}


void mainloop()
{
    SDL_Event event;
    auto running = true;

    while (running) {
        //SDL Event handling
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT: {
                    running = false;
                    break;
                }
                case SDL_KEYUP: {
                    if (event.key.keysym.sym == SDLK_ESCAPE) {
                        running = false;
                    }
                    break;
                }
            }
        }
    }
}


void runSw()
{
    auto window = SDL_CreateWindow("ThorVG Example (Software)", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WIDTH, HEIGHT, SDL_WINDOW_HIDDEN);
    auto surface = SDL_GetWindowSurface(window);

    for (int counter = 0; counter < NUM_PER_LINE * NUM_PER_LINE; ++counter) {
        auto canvas = tvg::SwCanvas::gen();
        tvgexam::verify(canvas->target((uint32_t*)surface->pixels + SIZE * (counter / NUM_PER_LINE) * (surface->pitch / 4) + (counter % NUM_PER_LINE) * SIZE, surface->w, surface->pitch / 4, surface->h, tvg::SwCanvas::ARGB8888));
        content(canvas.get());
        if (tvgexam::verify(canvas->draw())) {
            tvgexam::verify(canvas->sync());
        }
    }

    SDL_ShowWindow(window);
    SDL_UpdateWindowSurface(window);

    mainloop();

    SDL_DestroyWindow(window);
}


void runGl()
{
#if 1
    cout << "Not Support OpenGL" << endl;
#else //TODO with FBO Target?
    auto window = SDL_CreateWindow("ThorVG Example (OpenGL)", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WIDTH, HEIGHT, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    auto context = SDL_GL_CreateContext(window);

    for (int counter = 0; counter < NUM_PER_LINE * NUM_PER_LINE; ++counter) {
        auto canvas = tvg::GlCanvas::gen();

        //Set the canvas target and draw on it.
        tvgexam::verify(canvas->target(0, WIDTH, HEIGHT));

        content(canvas.get());
        if (tvgexam::verify(canvas->draw())) {
            tvgexam::verify(canvas->sync());
        }
    }

    SDL_ShowWindow(window);
    SDL_GL_SwapWindow(window);

    mainloop();

    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
#endif
}


void runWg()
{
#if 1
    cout << "Not Support WebGPU" << endl;
#else
#ifdef THORVG_WG_RASTER_SUPPORT
    //TODO with Drawing Target?
    auto window = SDL_CreateWindow("ThorVG Example (WebGPU)", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WIDTH, HEIGHT, SDL_WINDOW_HIDDEN);

    //Here we create our WebGPU surface from the window!
    SDL_SysWMinfo windowWMInfo;
    SDL_VERSION(&windowWMInfo.version);
    SDL_GetWindowWMInfo(window, &windowWMInfo);

    //Init WebGPU
	WGPUInstanceDescriptor desc = {.nextInChain = nullptr};
	auto instance = wgpuCreateInstance(&desc);

    #if defined(SDL_VIDEO_DRIVER_COCOA)
        [windowWMInfo.info.cocoa.window.contentView setWantsLayer:YES];
        auto layer = [CAMetalLayer layer];
        [windowWMInfo.info.cocoa.window.contentView setLayer:layer];

        WGPUSurfaceDescriptorFromMetalLayer surfaceNativeDesc = {
            .chain = {nullptr, WGPUSType_SurfaceDescriptorFromMetalLayer},
            .layer = layer
        };
    #elif defined(SDL_VIDEO_DRIVER_X11)
        WGPUSurfaceDescriptorFromXlibWindow surfaceNativeDesc = {
            .chain = {nullptr, WGPUSType_SurfaceDescriptorFromXlibWindow},
            .display = windowWMInfo.info.x11.display,
            .window = windowWMInfo.info.x11.window
        };
    #elif defined(SDL_VIDEO_DRIVER_WAYLAND)
        WGPUSurfaceDescriptorFromWaylandSurface surfaceNativeDesc = {
            .chain = {nullptr, WGPUSType_SurfaceDescriptorFromWaylandSurface},
            .display = windowWMInfo.info.wl.display,
            .surface = windowWMInfo.info.wl.surface
        };
    #elif defined(SDL_VIDEO_DRIVER_WINDOWS)
        WGPUSurfaceDescriptorFromWindowsHWND surfaceNativeDesc = {
            .chain = {nullptr, WGPUSType_SurfaceDescriptorFromWindowsHWND},
            .hinstance = GetModuleHandle(nullptr),
            .hwnd = windowWMInfo.info.win.window
        };
    #endif

    WGPUSurfaceDescriptor surfaceDesc{};
    surfaceDesc.nextInChain = (const WGPUChainedStruct*)&surfaceNativeDesc;
    surfaceDesc.label = "The surface";
    auto surface = wgpuInstanceCreateSurface(instance, &surfaceDesc);

    for (int counter = 0; counter < NUM_PER_LINE * NUM_PER_LINE; ++counter) {
        auto canvas = tvg::WgCanvas::gen();

        //Set the canvas target and draw on it.
        tvgexam::verify(canvas->target(instance, surface, WIDTH, HEIGHT));

        content(canvas.get());
        if (tvgexam::verify(canvas->draw())) {
            tvgexam::verify(canvas->sync());
        }
    }

    SDL_ShowWindow(window);

    mainloop();

    wgpuInstanceRelease(instance);
    SDL_DestroyWindow(window);
#else
    cout << "webgpu driver is not detected!" << endl;
#endif
#endif
}

/************************************************************************/
/* Entry Point                                                          */
/************************************************************************/

int main(int argc, char **argv)
{
    auto engine = tvg::CanvasEngine::Sw;

    if (argc > 1) {
        if (!strcmp(argv[1], "gl")) engine = tvg::CanvasEngine::Gl;
        if (!strcmp(argv[1], "wg")) engine = tvg::CanvasEngine::Wg;
    }

    if (tvgexam::verify(tvg::Initializer::init(4, engine))) {

        SDL_Init(SDL_INIT_VIDEO);

        if (engine == tvg::CanvasEngine::Sw) runSw();
        else if (engine == tvg::CanvasEngine::Gl) runGl();
        else if (engine == tvg::CanvasEngine::Wg) runWg();

        SDL_Quit();

        //Terminate ThorVG Engine
        tvg::Initializer::term();
    }

    return 0;
}