/*
 * Copyright (c) 2020 - 2023 the ThorVG project. All rights reserved.

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

#include <iostream>
#include <thread>
#include <thorvg.h>
#include <SDL.h>
#ifdef TVG_EXAMPLE_GL
#ifdef OS_ANDROID
#include <GLES2/gl2.h>
#elif defined(__APPLE__)
#include <OpenGL/gl.h>
#include <OpenGL/gl3.h>
#else
#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES 1
#endif
#include <GL/gl.h>
#include <GL/glext.h>
#endif
#endif

using namespace std;

static uint32_t  WIDTH = 800;
static uint32_t  HEIGHT = 800;
static uint32_t* buffer = nullptr;

static SDL_Window* win = nullptr;
SDL_Surface*       w_surface = nullptr;
SDL_Surface*       r_surface = nullptr;
SDL_GLContext      glContext = nullptr;

/************************************************************************/
/* Common Infrastructure Code                                           */
/************************************************************************/

void tvgSwTest(uint32_t* buffer);
void drawSwView(void* data);


static SDL_Window* createSwView(uint32_t w = 800, uint32_t h = 800)
{
    WIDTH = w;
    HEIGHT = h;

    Uint32 rmask, gmask, bmask, amask;
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
    rmask = 0xff000000;
    gmask = 0x00ff0000;
    bmask = 0x0000ff00;
    amask = 0x000000ff;
#else
    rmask = 0x00ff0000;
    gmask = 0x0000ff00;
    bmask = 0x000000ff;
    amask = 0xff000000;
#endif

    SDL_Init(SDL_INIT_EVERYTHING);

    buffer = static_cast<uint32_t*>(malloc(w * h * sizeof(uint32_t)));

    memset(buffer, 0 , w * h * 4);

    win = SDL_CreateWindow("Thorvg Test", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WIDTH, HEIGHT,
                           SDL_WINDOW_SHOWN);

    w_surface = SDL_GetWindowSurface(win);
    r_surface = SDL_CreateRGBSurface(0, WIDTH, HEIGHT, 32, rmask, gmask, bmask, amask);

    tvgSwTest(buffer);

    bool quite = false;

    while (!quite) {
        SDL_Event e{0};
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                quite = true;
                break;
            }
            SDL_FillRect(w_surface, nullptr, SDL_MapRGB(w_surface->format, 0x0, 0x0, 0x0));

            drawSwView(buffer);

            SDL_LockSurface(r_surface);

            std::memcpy(r_surface->pixels, buffer, WIDTH * HEIGHT * 4);

            SDL_UnlockSurface(r_surface);

            SDL_Rect stretchRect;
            stretchRect.x = 0;
            stretchRect.y = 0;
            stretchRect.w = WIDTH;
            stretchRect.h = HEIGHT;

            SDL_BlitScaled(r_surface, nullptr, w_surface, &stretchRect);

            SDL_UpdateWindowSurface(win);
        }
    }

    SDL_DestroyWindow(win);

    SDL_Quit();

    return win;
}

void initGLview(SDL_Window* win);
void drawGLview(SDL_Window* win);

static SDL_Window* createGlView(uint32_t w = 800, uint32_t h = 800)
{
    WIDTH = w;
    HEIGHT = h;

    // init sdl
    SDL_Init(SDL_INIT_EVERYTHING);

#ifdef __APPLE__
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#endif

    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);   // Enable multisampling
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 16);  // Set the number of samples

    win = SDL_CreateWindow("Hello world !", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WIDTH, HEIGHT,
                           SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL);


    glContext = SDL_GL_CreateContext(win);


    glEnable(GL_MULTISAMPLE);

    bool runing = true;

    initGLview(win);

    while (runing) {
        SDL_Event event{};

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                runing = false;
                break;
            }

            drawGLview(win);

            SDL_GL_SwapWindow(win);
        }
    }


    return win;
}

#endif //NO_GL_EXAMPLE