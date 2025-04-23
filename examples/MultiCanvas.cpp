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

#include "Example.h"

#ifdef THORVG_GL_RASTER_SUPPORT
    #include <SDL2/SDL_opengl.h>
#endif

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
    canvas->push(bg);

    auto picture = tvg::Picture::gen();
    if (!tvgexam::verify(picture->load(EXAMPLE_DIR"/svg/logo.svg"))) return;

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

    canvas->push(picture);
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


/************************************************************************/
/* SW Engine Specific Setup                                             */
/************************************************************************/

void runSw()
{
    auto window = SDL_CreateWindow("ThorVG Example (Software)", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WIDTH, HEIGHT, SDL_WINDOW_HIDDEN);
    auto surface = SDL_GetWindowSurface(window);

    for (int counter = 0; counter < NUM_PER_LINE * NUM_PER_LINE; ++counter) {
        auto canvas = unique_ptr<tvg::SwCanvas>(tvg::SwCanvas::gen());
        auto offx = (counter % NUM_PER_LINE) * SIZE;
        auto offy = SIZE * (counter / NUM_PER_LINE);
        auto w = surface->w - offx;
        auto h = surface->h - offy;
        tvgexam::verify(canvas->target((uint32_t*)surface->pixels + SIZE * (counter / NUM_PER_LINE) * (surface->pitch / 4) + (counter % NUM_PER_LINE) * SIZE, surface->pitch / 4, w, h, tvg::ColorSpace::ARGB8888));
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


/************************************************************************/
/* GL Engine Specific Setup                                             */
/************************************************************************/

#ifdef THORVG_GL_RASTER_SUPPORT

typedef void (*PFNGLTEXIMAGE2DPROC)(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid* pixels);
typedef void (*PFNGLTEXPARAMETERIPROC)(GLenum target, GLenum pname, GLint param);

PFNGLBINDTEXTUREEXTPROC fglBindTexture = nullptr;
PFNGLDELETETEXTURESEXTPROC fglDeleteTextures = nullptr;
PFNGLGENTEXTURESEXTPROC fglGenTextures = nullptr;
PFNGLTEXIMAGE2DPROC fglTexImage2D = nullptr;
PFNGLTEXPARAMETERIPROC fglTexParameteri = nullptr;
PFNGLGENFRAMEBUFFERSPROC fglGenFramebuffers = nullptr;
PFNGLBINDFRAMEBUFFERPROC fglBindFramebuffer = nullptr;
PFNGLDELETEFRAMEBUFFERSPROC fglDeleteFramebuffers = nullptr;
PFNGLFRAMEBUFFERTEXTURE2DPROC fglFramebufferTexture2D = nullptr;
PFNGLBLITFRAMEBUFFERPROC fglBlitFramebuffer = nullptr;

/* A helper class to manage OpenGL FrameBuffer creation and deletion
   Also provides a simple way to flush the framebuffer to the screen at given position */
struct GLFrameBuffer
{
    GLuint fbo;
    GLuint texture;

    ~GLFrameBuffer()
    {
        if (fbo) fglDeleteFramebuffers(1, &fbo);
        if (texture) fglDeleteTextures(1, &texture);
    }

    GLFrameBuffer(uint32_t width, uint32_t height)
    {
        fglGenFramebuffers(1, &fbo);
        fglBindFramebuffer(GL_FRAMEBUFFER, fbo);
        fglGenTextures(1, &texture);
        fglBindTexture(GL_TEXTURE_2D, texture);
        fglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, nullptr);
        fglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        fglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        fglFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
        fglBindFramebuffer(GL_FRAMEBUFFER, 0);
        fglBindTexture(GL_TEXTURE_2D, 0);
    }

    void blitToScreen(uint32_t posX, uint32_t posY, uint32_t width, uint32_t height)
    {
        /* As this is a simple example, we will just blit the framebuffer to the screen
           For a real application, you should use a shader to sample the texture and draw to the screen */
        fglBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
        fglBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        fglBlitFramebuffer(0, 0, width, height, posX, posY, posX +width, posY + height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    }
};
#endif

void runGl()
{
#ifdef THORVG_GL_RASTER_SUPPORT

#ifdef THORVG_GL_TARGET_GLES
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#else
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
#endif

    auto window = SDL_CreateWindow("ThorVG Example (OpenGL)", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WIDTH, HEIGHT, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    auto context = SDL_GL_CreateContext(window);

    fglBindTexture = (PFNGLBINDTEXTUREEXTPROC)SDL_GL_GetProcAddress("glBindTexture");
    fglDeleteTextures = (PFNGLDELETETEXTURESEXTPROC)SDL_GL_GetProcAddress("glDeleteTextures");
    fglGenTextures = (PFNGLGENTEXTURESEXTPROC)SDL_GL_GetProcAddress("glGenTextures");
    fglTexImage2D = (PFNGLTEXIMAGE2DPROC)SDL_GL_GetProcAddress("glTexImage2D");
    fglTexParameteri = (PFNGLTEXPARAMETERIPROC)SDL_GL_GetProcAddress("glTexParameteri");
    fglBindFramebuffer = (PFNGLBINDFRAMEBUFFERPROC)SDL_GL_GetProcAddress("glBindFramebuffer");
    fglGenFramebuffers = (PFNGLGENFRAMEBUFFERSPROC)SDL_GL_GetProcAddress("glGenFramebuffers");
    fglDeleteFramebuffers = (PFNGLDELETEFRAMEBUFFERSPROC)SDL_GL_GetProcAddress("glDeleteFramebuffers");
    fglFramebufferTexture2D = (PFNGLFRAMEBUFFERTEXTURE2DPROC)SDL_GL_GetProcAddress("glFramebufferTexture2D");
    fglBlitFramebuffer = (PFNGLBLITFRAMEBUFFERPROC)SDL_GL_GetProcAddress("glBlitFramebuffer");

    // Create the framebuffer which the GlCanvas can render to
    // Since the example is just a simple demo, and only run the rendering function once
    // we can just create one framebuffer
    GLFrameBuffer glFbo{SIZE, SIZE};
    
    for (int counter = 0; counter < NUM_PER_LINE * NUM_PER_LINE; ++counter) {
        auto canvas = unique_ptr<tvg::GlCanvas>(tvg::GlCanvas::gen());

        // Pass the framebuffer id to the GlCanvas
        tvgexam::verify(canvas->target(context, glFbo.fbo, SIZE, SIZE, tvg::ColorSpace::ABGR8888S));

        content(canvas.get());
        if (tvgexam::verify(canvas->draw())) {
            tvgexam::verify(canvas->sync());
        }

        // After the GlCanvas::sync() function, the content is rendered to the framebuffer
        // The texture is now ready to be blit to the screen
        auto y = (counter / NUM_PER_LINE) * SIZE;
        auto x = (counter % NUM_PER_LINE) * SIZE;
        glFbo.blitToScreen(x, y, SIZE, SIZE);

        // After the framebuffer is blit to the screen, the framebuffer and texture can be reused in the next iteration
    }

    SDL_ShowWindow(window);
    SDL_GL_SwapWindow(window);

    mainloop();

    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
#else
    cout << "Not Support OpenGL" << endl;
#endif
}


/************************************************************************/
/* WG Engine Specific Setup                                             */
/************************************************************************/

#ifdef THORVG_WG_RASTER_SUPPORT
void wgCopyTextureToTexture(WGPUDevice device, WGPUTexture src, WGPUTexture dst, uint32_t posX, uint32_t posY, uint32_t width, uint32_t height) {
    WGPUQueue queue = wgpuDeviceGetQueue(device);

    // create command encoder
    const WGPUCommandEncoderDescriptor commandEncoderDesc{};
    WGPUCommandEncoder commandEncoder = wgpuDeviceCreateCommandEncoder(device, &commandEncoderDesc);

    // copy canvas data to surface
    const WGPUImageCopyTexture texSrc { .texture = src };
    const WGPUImageCopyTexture texDst { .texture = dst, .origin = { .x = posX, .y = posY } };
    const WGPUExtent3D copySize { .width = width, .height = height, .depthOrArrayLayers = 1 };
    wgpuCommandEncoderCopyTextureToTexture(commandEncoder, &texSrc, &texDst, &copySize);

    // release command encoder
    const WGPUCommandBufferDescriptor commandBufferDesc{};
    WGPUCommandBuffer commandsBuffer = wgpuCommandEncoderFinish(commandEncoder, &commandBufferDesc);
    wgpuQueueSubmit(queue, 1, &commandsBuffer);
    wgpuCommandBufferRelease(commandsBuffer);
    wgpuCommandEncoderRelease(commandEncoder);

    wgpuQueueRelease(queue);
}
#endif

void runWg()
{
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

    // request adapter
    WGPUAdapter adapter{};
    const WGPURequestAdapterOptions requestAdapterOptions { .compatibleSurface = surface, .powerPreference = WGPUPowerPreference_HighPerformance };
    auto onAdapterRequestEnded = [](WGPURequestAdapterStatus status, WGPUAdapter adapter, char const * message, void * pUserData) { *((WGPUAdapter*)pUserData) = adapter; };
    wgpuInstanceRequestAdapter(instance, &requestAdapterOptions, onAdapterRequestEnded, &adapter);

    // get adapter and surface properties
    WGPUFeatureName featureNames[32]{};
    size_t featuresCount = wgpuAdapterEnumerateFeatures(adapter, featureNames);

    // request device
    WGPUDevice device{};
    const WGPUDeviceDescriptor deviceDesc { .label = "The shared device", .requiredFeatureCount = featuresCount, .requiredFeatures = featureNames };
    auto onDeviceRequestEnded = [](WGPURequestDeviceStatus status, WGPUDevice device, char const * message, void * pUserData) { *((WGPUDevice*)pUserData) = device; };
    wgpuAdapterRequestDevice(adapter, &deviceDesc, onDeviceRequestEnded, &device);

    // setup surface configuration
    WGPUSurfaceConfiguration surfaceConfiguration {
        .device = device,
        .format = WGPUTextureFormat_BGRA8Unorm,
        .usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopyDst,
        .width = WIDTH, .height = HEIGHT
    };
    wgpuSurfaceConfigure(surface, &surfaceConfiguration);

    // create offscren target texure
    const WGPUTextureDescriptor textureDesc {
        .usage = WGPUTextureUsage_CopySrc | WGPUTextureUsage_RenderAttachment,
        .dimension = WGPUTextureDimension_2D, .size = { SIZE, SIZE, 1 },
        .format = WGPUTextureFormat_BGRA8Unorm, .mipLevelCount = 1, .sampleCount = 1
    };
    WGPUTexture renderTarget = wgpuDeviceCreateTexture(device, &textureDesc);

    WGPUSurfaceTexture surfaceTexture{};
    wgpuSurfaceGetCurrentTexture(surface, &surfaceTexture);

    for (int counter = 0; counter < NUM_PER_LINE * NUM_PER_LINE; ++counter) {
        auto canvas = unique_ptr<tvg::WgCanvas>(tvg::WgCanvas::gen());

        //Set the canvas target and draw on it.
        tvgexam::verify(canvas->target(device, instance, renderTarget, SIZE, SIZE, tvg::ColorSpace::ABGR8888S, 1));

        content(canvas.get());
        if (tvgexam::verify(canvas->draw())) {
            tvgexam::verify(canvas->sync());
        }

        // After the WgCanvas::sync() function, the content is rendered to the render target
        // The texture is now ready to be blit to the screen
        uint32_t y = (counter / NUM_PER_LINE) * SIZE;
        uint32_t x = (counter % NUM_PER_LINE) * SIZE;
        wgCopyTextureToTexture(device, renderTarget, surfaceTexture.texture, x, y, SIZE, SIZE);
    }
    
    SDL_ShowWindow(window);
    wgpuSurfacePresent(surface);

    mainloop();

    wgpuTextureRelease(surfaceTexture.texture);
    wgpuTextureDestroy(renderTarget);
    wgpuTextureRelease(renderTarget);
    wgpuDeviceRelease(device);
    wgpuAdapterRelease(adapter);
    wgpuSurfaceUnconfigure(surface);
    wgpuSurfaceRelease(surface);
    wgpuInstanceRelease(instance);
    SDL_DestroyWindow(window);
#else
    cout << "Not Support WebGPU" << endl;
#endif
}

/************************************************************************/
/* Entry Point                                                          */
/************************************************************************/

int main(int argc, char **argv)
{
    auto engine = 0; //0: sw, 1: gl, 2: wg

    if (argc > 1) {
        if (!strcmp(argv[1], "gl")) engine = 1;
        if (!strcmp(argv[1], "wg")) engine = 2;
    }

    if (tvgexam::verify(tvg::Initializer::init(4))) {

        SDL_Init(SDL_INIT_VIDEO);

        if (engine == 0) runSw();
        else if (engine == 1) runGl();
        else if (engine == 2) runWg();

        SDL_Quit();

        //Terminate ThorVG Engine
        tvg::Initializer::term();
    }

    return 0;
}