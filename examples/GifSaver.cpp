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

#include "Example.h"

using namespace std;

/************************************************************************/
/* ThorVG Saving Contents                                               */
/************************************************************************/


void exportGif()
{
    auto animation = tvg::Animation::gen();
    auto picture = animation->picture();
    if (!tvgexam::verify(picture->load(EXAMPLE_DIR"/lottie/walker.json"))) return;

    picture->size(800, 800);

    auto saver = tvg::Saver::gen();
    if (!tvgexam::verify(saver->save(std::move(animation), "./test.gif"))) return;
    saver->sync();

    cout << "Successfully exported to test.gif." << endl;
}


/************************************************************************/
/* Entry Point                                                          */
/************************************************************************/

int main(int argc, char **argv)
{
    if (tvgexam::verify(tvg::Initializer::init(0))) {

        exportGif();

        tvg::Initializer::term();
    }
    return 0;
}

