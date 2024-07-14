/*
 * Copyright (c) 2024 the ThorVG project. All rights reserved.

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
#include "config.h"

#include <cmath>
#include <vector>
#include <fstream>
#include <iostream>
#include <thorvg.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#ifdef _WIN32
    #include <windows.h>
    #ifndef PATH_MAX
        #define PATH_MAX MAX_PATH
    #endif
#else
    #include <dirent.h>
    #include <unistd.h>
    #include <limits.h>
    #include <sys/stat.h>
#endif

#ifdef THORVG_WG_RASTER_SUPPORT
    #include <webgpu/webgpu.h>
    #if defined(SDL_VIDEO_DRIVER_COCOA)
        #include <Cocoa/Cocoa.h>
        #include <QuartzCore/CAMetalLayer.h>
    #endif
#endif


using namespace std;

/************************************************************************/
/* Common Template Code                                                 */
/************************************************************************/

namespace tvgexam
{

bool verify(tvg::Result result, string failMsg = "");

struct Example
{
    virtual bool content(tvg::Canvas* canvas, uint32_t w, uint32_t h) = 0;
    virtual bool update(tvg::Canvas* canvas, uint32_t elapsed) { return false; }
    virtual void populate(const char* path) {}
    virtual ~Example() {}

    void scandir(const char* path)
    {
        char buf[PATH_MAX];

        //real path
    #ifdef _WIN32
        auto rpath = _fullpath(buf, path, PATH_MAX);
    #else
        auto rpath = realpath(path, buf);
    #endif

        //open directory
    #ifdef _WIN32
        WIN32_FIND_DATA fd;
        HANDLE h = FindFirstFileEx((string(rpath) + "\\*").c_str(), FindExInfoBasic, &fd, FindExSearchNameMatch, NULL, 0);
        if (h == INVALID_HANDLE_VALUE) {
            cout << "Couldn't open directory \"" << rpath << "\"." << endl;
            return;
        }
        do {
            if (*fd.cFileName == '.' || *fd.cFileName == '$') continue;
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                auto fullpath = string(path);
                fullpath += '\\';
                fullpath += fd.cFileName;
                populate(fullpath.c_str());
            }
        } while (FindNextFile(h, &fd));
        FindClose(h);
    #else
        DIR* dir = opendir(rpath);
        if (!dir) {
            cout << "Couldn't open directory \"" << rpath << "\"." << endl;
            return;
        }

        //list directory
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            if (*entry->d_name == '.' || *entry->d_name == '$') continue;
            if (entry->d_type != DT_DIR) {
                auto fullpath = string(path);
                fullpath += '/';
                fullpath += entry->d_name;
                populate(fullpath.c_str());
            }
        }
        closedir(dir);
    #endif
    }
};


struct Window
{
    SDL_Window* window = nullptr;

    tvg::Canvas* canvas = nullptr;
    uint32_t width;
    uint32_t height;

    Example* example = nullptr;

    bool needResize = false;
    bool needDraw = false;
    bool initialized = false;
    bool print = false;

    Window(tvg::CanvasEngine engine, Example* example, uint32_t width, uint32_t height, uint32_t threadsCnt)
    {
        //Initialize ThorVG Engine (engine: raster method)
        if (!verify(tvg::Initializer::init(threadsCnt, engine), "Failed to init ThorVG engine!")) return;

        //Initialize the SDL
        SDL_Init(SDL_INIT_VIDEO);

        //Init member variables
        this->width = width;
        this->height = height;
        this->example = example;
        this->initialized = true;
    }

    virtual ~Window()
    {
        delete(example);

        //Terminate the SDL
        SDL_DestroyWindow(window);
        SDL_Quit();

        //Terminate ThorVG Engine
        tvg::Initializer::term();
    }

    bool draw()
    {
        //Draw the contents to the Canvas
        if (verify(canvas->draw())) {
            verify(canvas->sync());
            return true;
        }

        return false;
    }

    bool ready()
    {
        if (!canvas) return false;

        if (!example->content(canvas, width, height)) return false;

        //initiate the first rendering before window pop-up.
        if (!verify(canvas->draw())) return false;
        if (!verify(canvas->sync())) return false;

        return true;
    }

    void show()
    {
        SDL_ShowWindow(window);
        refresh();

        //Mainloop
        SDL_Event event;
        auto running = true;

        auto ptime = SDL_GetTicks();
        auto elapsed = 0;
        uint32_t tickCnt = 0;

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
                    case SDL_WINDOWEVENT: {
                        if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                            width = event.window.data1;
                            height = event.window.data2;
                            needResize = true;
                            needDraw = true;
                        }
                    }
                }
            }

            if (needResize) {
                resize();
                needResize = false;
            }

            if (tickCnt > 0) {
                if (example->update(canvas, elapsed)) {
                    needDraw = true;
                }
            }

            if (needDraw) {
                if (draw()) refresh();
                needDraw = false;
            }

            auto ctime = SDL_GetTicks();
            elapsed += (ctime - ptime);
            tickCnt++;
            if (print) printf("[%5d]: elapsed time = %dms (%dms)\n", tickCnt, (ctime - ptime), (elapsed / tickCnt));
            ptime = ctime;
        }
    }

    virtual void resize() {}
    virtual void refresh() {}
};


/************************************************************************/
/* SwCanvas Window Code                                                 */
/************************************************************************/

struct SwWindow : Window
{
    unique_ptr<tvg::SwCanvas> canvas = nullptr;

    SwWindow(Example* example, uint32_t width, uint32_t height, uint32_t threadsCnt) : Window(tvg::CanvasEngine::Sw, example, width, height, threadsCnt)
    {
        if (!initialized) return;

        window = SDL_CreateWindow("ThorVG Example (Software)", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE);

        //Create a Canvas
        canvas = tvg::SwCanvas::gen();
        if (!canvas) {
            cout << "SwCanvas is not supported. Did you enable the SwEngine?" << endl;
            return;
        }

        Window::canvas = canvas.get();

        resize();
    }

    void resize() override
    {
        auto surface = SDL_GetWindowSurface(window);
        if (!surface) return;

        //Set the canvas target and draw on it.
        verify(canvas->target((uint32_t*)surface->pixels, surface->w, surface->pitch / 4, surface->h, tvg::SwCanvas::ARGB8888));
    }

    void refresh() override
    {
        SDL_UpdateWindowSurface(window);
    }
};

/************************************************************************/
/* GlCanvas Window Code                                                 */
/************************************************************************/

struct GlWindow : Window
{
    SDL_GLContext context;

    unique_ptr<tvg::GlCanvas> canvas = nullptr;

    GlWindow(Example* example, uint32_t width, uint32_t height, uint32_t threadsCnt) : Window(tvg::CanvasEngine::Gl, example, width, height, threadsCnt)
    {
        if (!initialized) return;

#ifdef THORVG_GL_TARGET_GLES
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#else
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
#endif
        window = SDL_CreateWindow("ThorVG Example (OpenGL/ES)", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE);
        context = SDL_GL_CreateContext(window);

        //Create a Canvas
        canvas = tvg::GlCanvas::gen();
        if (!canvas) {
            cout << "GlCanvas is not supported. Did you enable the GlEngine?" << endl;
            return;
        }

        Window::canvas = canvas.get();

        resize();
    }

    virtual ~GlWindow()
    {
        SDL_GL_DeleteContext(context);
    }

    void resize() override
    {
        //Set the canvas target and draw on it.
        verify(canvas->target(0, width, height));
    }

    void refresh() override
    {
        SDL_GL_SwapWindow(window);
    }
};

/************************************************************************/
/* WgCanvas Window Code                                                 */
/************************************************************************/

#ifdef THORVG_WG_RASTER_SUPPORT

struct WgWindow : Window
{
    unique_ptr<tvg::WgCanvas> canvas = nullptr;

    WGPUInstance instance;
    WGPUSurface surface;

    WgWindow(Example* example, uint32_t width, uint32_t height, uint32_t threadsCnt) : Window(tvg::CanvasEngine::Wg, example, width, height, threadsCnt)
    {
        if (!initialized) return;

        window = SDL_CreateWindow("ThorVG Example (WebGPU)", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, SDL_WINDOW_HIDDEN);

        //Here we create our WebGPU surface from the window!
        SDL_SysWMinfo windowWMInfo;
        SDL_VERSION(&windowWMInfo.version);
        SDL_GetWindowWMInfo(window, &windowWMInfo);

        //Init WebGPU
	    WGPUInstanceDescriptor desc = {.nextInChain = nullptr};
	    instance = wgpuCreateInstance(&desc);

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
        surface = wgpuInstanceCreateSurface(instance, &surfaceDesc);

        //Create a Canvas
        canvas = tvg::WgCanvas::gen();
        if (!canvas) {
            cout << "WgCanvas is not supported. Did you enable the WgEngine?" << endl;
            return;
        }

        Window::canvas = canvas.get();

        resize();
    }

    virtual ~WgWindow()
    {
        //wgpuSurfaceRelease(surface);
        wgpuInstanceRelease(instance);
    }

    void resize() override
    {
        //Set the canvas target and draw on it.
        verify(canvas->target(instance, surface, width, height));
    }

    void refresh() override 
    {
        wgpuSurfacePresent(surface);
    }
};

#else
struct WgWindow : Window
{
    WgWindow(Example* example, uint32_t width, uint32_t height, uint32_t threadsCnt) : Window(tvg::CanvasEngine::Wg, example, width, height, threadsCnt)
    {
        cout << "webgpu driver is not detected!" << endl;
    }
};

#endif


float progress(uint32_t elapsed, float durationInSec, bool rewind = false)
{
    auto duration = uint32_t(durationInSec * 1000.0f); //sec -> millisec.
    auto forward = ((elapsed / duration) % 2 == 0) ? true : false;
    auto clamped = elapsed % duration;
    auto progress = ((float)clamped / (float)duration);
    if (rewind) return forward ? progress : (1 - progress);
    return progress;
}


bool verify(tvg::Result result, string failMsg)
{
    switch (result) {
        case tvg::Result::FailedAllocation: {
            cout << "FailedAllocation! " << failMsg << endl;
            return false;
        }
        case tvg::Result::InsufficientCondition: {
            cout << "InsufficientCondition! " << failMsg << endl;
            return false;
        }
        case tvg::Result::InvalidArguments: {
            cout << "InvalidArguments! " << failMsg << endl;
            return false;
        }
        case tvg::Result::MemoryCorruption: {
            cout << "MemoryCorruption! " << failMsg << endl;
            return false;
        }
        case tvg::Result::NonSupport: {
            cout << "NonSupport! " << failMsg << endl;
            return false;
        }
        case tvg::Result::Unknown: {
            cout << "Unknown! " << failMsg << endl;
            return false;
        }
        default: break;
    };
    return true;
}


int main(Example* example, int argc, char **argv, uint32_t width = 800, uint32_t height = 800, uint32_t threadsCnt = 4, bool print = false)
{
    auto engine = tvg::CanvasEngine::Sw;

    if (argc > 1) {
        if (!strcmp(argv[1], "gl")) engine = tvg::CanvasEngine::Gl;
        if (!strcmp(argv[1], "wg")) engine = tvg::CanvasEngine::Wg;
    }

    unique_ptr<Window> window;

    if (engine == tvg::CanvasEngine::Sw) {
        window = unique_ptr<Window>(new SwWindow(example, width, height, threadsCnt));
    } else if (engine == tvg::CanvasEngine::Gl) {
        window = unique_ptr<Window>(new GlWindow(example, width, height, threadsCnt));
    } else if (engine == tvg::CanvasEngine::Wg) {
        window = unique_ptr<Window>(new WgWindow(example, width, height, threadsCnt));
    }

    window->print = print;

    if (window->ready()) {
        window->show();
    }

    return 0;
}

};
