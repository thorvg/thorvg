/*
 * Copyright (c) 2023 the ThorVG project. All rights reserved.

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


#ifndef _TVG_WINDOW_IMPL_H_
#define _TVG_WINDOW_IMPL_H_

#include <cstdlib>
#include <thread>
#include <vector>
#include <iostream>

// sudo apt install libglew-dev -y
// sudo apt install libglfw3-dev libglfw3 -y
#include <GL/glew.h>
#include <GLFW/glfw3.h>
//#define GLFW_DLL

#include "tvgCommon.h"
#include "thorvg_window.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

const int MAX_WINDOW = 10;
static std::vector<tvg::Window*> window_list;
static constexpr auto BPP = 4;

struct Window::Impl
{
    std::string glsl_version;
    GLFWwindow* gl_window = nullptr;
    uint32_t* buffer = nullptr;
    GLuint texture = 0;
    int width = 0;
    int height = 0;
    std::unique_ptr<tvg::Canvas> canvas = nullptr;
    double lastTime = 0;
    double fps = 0;
    bool shoudClose = false;
    std::function<bool(tvg::Canvas*)> on_update;
    tvg::CanvasEngine engine = tvg::CanvasEngine::Sw;

    static void glfwOnFramebufferResize(GLFWwindow* gl_window, int w, int h) {
        for (auto window : window_list) {
            if(window->pImpl->gl_window == gl_window) {
                window->resize(w,h);
                break;
            }
        }
    }

    Impl(tvg::Window* window, int w, int h, std::string name, tvg::CanvasEngine engine_) 
    {
        window_list.push_back(window);
        engine = engine_;

        if (!glfwInit()) {
            printf("Fail to glfwInit()\n");
            return;
        }

        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        glfwWindowHint(GLFW_RED_BITS, mode->redBits);
        glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
        glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
        glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
        glfwWindowHint(GLFW_STENCIL_BITS, 0);
        glfwWindowHint(GLFW_DEPTH_BITS, 0);

        // Decide GL+GLSL versions
    #if defined(__APPLE__)
        // GL 3.2 + GLSL 150
        glsl_version = "#version 150";
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
    #else
        // GL 3.0 + GLSL 130
        glsl_version = "#version 130";
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
        //glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
        //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
    #endif

        gl_window = glfwCreateWindow(w, h, name.c_str(), NULL, NULL);
        if (!gl_window) {
            glfwTerminate();
            printf("Fail to glfwCreateWindow()\n");
            return;
        }

        glfwSetFramebufferSizeCallback(gl_window, Window::Impl::glfwOnFramebufferResize);

        glfwMakeContextCurrent(gl_window);
        //glfwSwapInterval(1);
        glEnable(GL_TEXTURE_2D);
        if (engine == tvg::CanvasEngine::Sw) {
            canvas = tvg::SwCanvas::gen();
        }
        else if (engine == tvg::CanvasEngine::Gl) {
            canvas = tvg::GlCanvas::gen();
        }

        glfwGetWindowSize(gl_window, &width, &height);
        resize(width, height);

        lastTime = glfwGetTime();

        printf("Called WindowImpl Constructor\n");
    }

    ~Impl()
    {
        printf("Called WindowImpl Destructor\n");
    }

    void close()
    {
        printf("Called WindowImpl close()\n");
        if (buffer) free(buffer);
    }

    bool run()
    {
        glfwMakeContextCurrent(gl_window);
        if (glfwWindowShouldClose(gl_window) ||
            glfwGetKey(gl_window, GLFW_KEY_ESCAPE))
        {
            glfwDestroyWindow (gl_window);
            return false;
        }

        // User render loop
        double thisTime = glfwGetTime();
        fps = 1.0 / (thisTime - lastTime);

        //sw_canvas->clear();
        bool updated = false;
        if (on_update) updated = on_update(canvas.get());
        if (updated) {
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            // Note: Gl renderer should to call target() again.
            if (engine == tvg::CanvasEngine::Gl) {
                auto gl_canvas = reinterpret_cast<tvg::GlCanvas*>(canvas.get());
                gl_canvas->target(nullptr, width * BPP, width, height);
            }
            if (canvas->draw() == tvg::Result::Success) canvas->sync();
        }

        // Render the buffer to a texture and display it
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexSubImage2D(
            GL_TEXTURE_2D,
            0,
            0, 0,
            width, height,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            (void*)buffer
        );
        glBegin(GL_QUADS); 
        glTexCoord2f(0, 0); glVertex2f(0, 0);
        glTexCoord2f(1, 0); glVertex2f(width, 0);
        glTexCoord2f(1, 1); glVertex2f(width, height);
        glTexCoord2f(0, 1); glVertex2f(0, height);
        glEnd();
        glBindTexture(GL_TEXTURE_2D, 0);

        // Swap framebuffers
        glfwSwapBuffers(gl_window);
        glfwPollEvents();
        lastTime = thisTime;


        return true;
    }

    void resize(int w, int h)
    {
        // Create a new framebuffer
        width = w;
        height = h;
        buffer = static_cast<uint32_t*>(realloc(buffer, width * height * sizeof(uint32_t)));

        // Create a new texture
        glDeleteTextures(1, &texture);
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RGBA,
            width, height,
            0,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            (void*)buffer
        );
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        // reattach buffer to tvg canvas
        if (engine == tvg::CanvasEngine::Sw) {
            auto sw_canvas = reinterpret_cast<tvg::SwCanvas*>(canvas.get());
            sw_canvas->target(buffer, width, width, height, tvg::SwCanvas::ABGR8888);
        }
        else if (engine == tvg::CanvasEngine::Gl) {
            auto gl_canvas = reinterpret_cast<tvg::GlCanvas*>(canvas.get());
            static constexpr auto BPP = 4;
            gl_canvas->target(nullptr, width * BPP, width, height);
        }
        // Set projection
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0.0f, width, height, 0.0f, 0.0f, 1.0f);
        glViewport(0, 0, width, height);
        if (canvas->draw() == tvg::Result::Success) canvas->sync();
    }

    void init(std::function<bool(tvg::Canvas*)> on_init)
    {
        on_init(canvas.get());
        if (canvas->draw() == tvg::Result::Success) canvas->sync();
    }

    void update(std::function<bool(tvg::Canvas*)> _on_update)
    {
        on_update = _on_update;
    }
};
#endif //_TVG_WINDOW_IMPL_H_
