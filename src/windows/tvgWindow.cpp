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

#include "tvgWindowImpl.h"

#include <vector>
#include <algorithm>

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

Window::Window(int width, int height, std::string name, tvg::CanvasEngine engine)
     : pImpl(new Impl(this, width, height, name, engine))
{
    // Window::pImpl->id = TVG_CLASS_ID_WINDOW;
    printf("Called Constructor (%d %d) %s\n", width, height, name.c_str());
}


Window::~Window()
{
    printf("Called Destructor %p\n",pImpl->gl_window);
    delete(pImpl);
}


unique_ptr<Window> Window::gen(int width, int height, std::string name, tvg::CanvasEngine engine) noexcept
{
    return unique_ptr<Window>(new Window(width, height, name, engine));
}


void Window::close()
{
    pImpl->close();
}

void Window::loop()
{
    while (true) {
        bool exit = true;
        for (auto window : window_list) {
            if (window->run()) {
                exit = false;
            } else {
                window->close();
                window_list.erase(std::remove(window_list.begin(), window_list.end(), window), window_list.end());
            }
        }
        if (exit)
            break;
    }
}

bool Window::run()
{
    return pImpl->run();
}


void Window::resize(int width, int height)
{
    pImpl->resize(width, height);
}

void Window::init(std::function<bool(tvg::Canvas*)> on_init)
{
    pImpl->init(on_init);
}

void Window::update(std::function<bool(tvg::Canvas*)> on_update)
{
    pImpl->update(on_update);
}
