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

#include <iostream>
#include <thread>
#include <cstring>

#include <thorvg_window.h>

using namespace std;

static uint32_t WIDTH = 800;
static uint32_t HEIGHT = 800;

static unique_ptr<tvg::Window> window;
static unique_ptr<tvg::Window> window2;
static unique_ptr<tvg::Window> window3;
static unique_ptr<tvg::Window> window4;

/************************************************************************/
/* Main Code                                                            */
/************************************************************************/

int main(int argc, char **argv)
{
    //Threads Count
    auto threads = std::thread::hardware_concurrency();
    if (threads > 0) --threads;    //Allow the designated main thread capacity

    //Initialize ThorVG Engine
    if (tvg::Initializer::init(threads) == tvg::Result::Success) {

        window = tvg::Window::gen(WIDTH, HEIGHT, "GLFW Window Example 1 (Sw)", tvg::CanvasEngine::Sw);

        window->init([](tvg::Canvas* canvas) {
            auto main_scene = tvg::Scene::gen();
            auto shape1 = tvg::Shape::gen();
            shape1->appendRect(0, 0, 200, 200);                //x, y, w, h
            shape1->appendRect(100, 100, 300, 300, 100, 100);  //x, y, w, h, rx, ry
            shape1->appendCircle(400, 400, 100, 100);          //cx, cy, radiusW, radiusH
            shape1->appendCircle(400, 500, 170, 100);          //cx, cy, radiusW, radiusH
            shape1->fill(255, 0, 0);                         //r, g, b
            main_scene->push(std::move(shape1));
            auto picture = tvg::Picture::gen();
            if (picture->load(EXAMPLE_DIR"/cartman.svg") != tvg::Result::Success) {
                return false;
            }
            picture->translate(150, 150);
            picture->size(100, 100);
            main_scene->push(std::move(picture));
            canvas->push(std::move(main_scene));
printf("inited 1\n");
            return true;
        });
/*
        window->update([](tvg::Canvas* canvas) {
            canvas->update();
printf("updated 1\n");
            return true;
        });
*/
        window2 = tvg::Window::gen(WIDTH, HEIGHT, "GLFW Window Example 2 (Gl)", tvg::CanvasEngine::Gl);

        window2->init([](tvg::Canvas* canvas) {
            auto main_scene = tvg::Scene::gen();
            auto shape1 = tvg::Shape::gen();
            shape1->appendRect(0, 0, 200, 200);                //x, y, w, h
            shape1->appendRect(100, 100, 300, 300, 100, 100);  //x, y, w, h, rx, ry
            shape1->appendCircle(400, 400, 100, 100);          //cx, cy, radiusW, radiusH
            shape1->appendCircle(400, 500, 170, 100);          //cx, cy, radiusW, radiusH
            shape1->fill(0, 255, 0);                         //r, g, b
            main_scene->push(std::move(shape1));
            auto picture = tvg::Picture::gen();
            if (picture->load(EXAMPLE_DIR"/tiger.svg") != tvg::Result::Success) {
                return false;
            }
            picture->translate(150, 150);
            picture->size(100, 100);
            main_scene->push(std::move(picture));
            canvas->push(std::move(main_scene));
printf("inited 2\n");
            return true;
        });
/*
        window2->update([](tvg::Canvas* canvas) {
            canvas->update();
printf("updated 2\n");
            return true;
        });

*/
        window3 = tvg::Window::gen(WIDTH, HEIGHT, "GLFW Window Example 3 (Gl)", tvg::CanvasEngine::Gl);

        window3->init([](tvg::Canvas* canvas) {
            auto main_scene = tvg::Scene::gen();
            auto shape1 = tvg::Shape::gen();
            shape1->appendRect(0, 0, 200, 200);                //x, y, w, h
            shape1->appendRect(100, 100, 300, 300, 100, 100);  //x, y, w, h, rx, ry
            shape1->appendCircle(400, 400, 100, 100);          //cx, cy, radiusW, radiusH
            shape1->appendCircle(400, 500, 170, 100);          //cx, cy, radiusW, radiusH
            shape1->fill(0, 255, 0);                         //r, g, b
            main_scene->push(std::move(shape1));

            auto picture = tvg::Picture::gen();
            if (picture->load(EXAMPLE_DIR"/logo.svg") != tvg::Result::Success) {
                return false;
            }
            picture->translate(150, 150);
            picture->size(400, 400);
            main_scene->push(std::move(picture));
            canvas->push(std::move(main_scene));
printf("inited 3\n");
            return true;
        });
  /*      window3->update([](tvg::Canvas* canvas) {
            canvas->update();
printf("updated 3\n");
            return true;
        });
*/
        window4 = tvg::Window::gen(WIDTH, HEIGHT, "GLFW Window Example 4 (Sw)", tvg::CanvasEngine::Sw);

        auto ani = tvg::Animation::gen();
        auto animation = ani.get();//tvg::Animation::gen();
        window4->init([=](tvg::Canvas* canvas) {
            auto main_scene = tvg::Scene::gen();
            auto shape1 = tvg::Shape::gen();
            shape1->appendRect(0, 0, 200, 200);                //x, y, w, h
            shape1->appendRect(100, 100, 300, 300, 100, 100);  //x, y, w, h, rx, ry
            shape1->appendCircle(400, 400, 100, 100);          //cx, cy, radiusW, radiusH
            shape1->appendCircle(400, 500, 170, 100);          //cx, cy, radiusW, radiusH
            shape1->fill(0, 255, 0);                         //r, g, b
            main_scene->push(std::move(shape1));

            auto picture = animation->picture();
            if (picture->load(EXAMPLE_DIR"/alien.json") != tvg::Result::Success) {
                return false;
            }
            picture->translate(150, 150);
            picture->size(400, 400);
            canvas->push(std::move(main_scene));
            canvas->push(tvg::cast<tvg::Picture>(animation->picture()));
printf("inited 4\n");
            return true;
        });

        window4->update([=](tvg::Canvas* canvas) {
            uint32_t cur = animation->curFrame();
            cur = (cur + 1) % (uint32_t)animation->totalFrame();
            animation->frame(cur);
            canvas->update(animation->picture());
//printf("updated 4\n");
            return true;
        });

        tvg::Window::loop();

        tvg::Initializer::term();

    } else {
        cout << "engine is not supported" << endl;
    }
    return 0;
}
