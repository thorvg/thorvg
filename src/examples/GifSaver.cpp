/*
 * Copyright (c) 2023 - 2024 the ThorVG project. All rights reserved.

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

using namespace std;

void exportGif()
{
    auto animation = tvg::Animation::gen();
    auto picture = animation->picture();
    if (picture->load(EXAMPLE_DIR"/walker.json") != tvg::Result::Success) {
        cout << "Lottie is not supported. Did you enable Lottie Loader?" << endl;
        return;
    }

    picture->size(800, 800);

    auto saver = tvg::Saver::gen();
    if (saver->save(std::move(animation), "./test.gif") != tvg::Result::Success) {
        cout << "Problem with saving the json file. Did you enable Gif Saver?" << endl;
        return;
    }
    saver->sync();
    cout << "Successfully exported to test.gif." << endl;

    animation = tvg::Animation::gen();
    picture = animation->picture();
    if (picture->load(EXAMPLE_DIR"/walker.json") != tvg::Result::Success) {
        cout << "Lottie is not supported. Did you enable Lottie Loader?" << endl;
        return;
    }

    picture->size(800, 800);

    saver = tvg::Saver::gen();

    //set a white opaque background
    auto bg = tvg::Shape::gen();
    bg->fill(255, 255, 255, 255);
    bg->appendRect(0, 0, 800, 800);

    saver->background(std::move(bg));

    if (saver->save(std::move(animation), "./test_60fps.gif", 100, 60) != tvg::Result::Success) {
        cout << "Problem with saving the json file. Did you enable Gif Saver?" << endl;
        return;
    }
    saver->sync();
    cout << "Successfully exported to test_60fps.gif." << endl;
}


/************************************************************************/
/* Main Code                                                            */
/************************************************************************/

int main(int argc, char **argv)
{
    //Initialize ThorVG Engine
    if (tvg::Initializer::init(0) == tvg::Result::Success) {

        exportGif();

        //Terminate ThorVG Engine
        tvg::Initializer::term();

    } else {
        cout << "engine is not supported" << endl;
    }
    return 0;
}

