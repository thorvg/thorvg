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

#include "Common.h"

/************************************************************************/
/* Drawing Commands                                                     */
/************************************************************************/

void tvgDrawCmds(tvg::Canvas* canvas)
{
    if (!canvas) return;

    /* Star */

    //Prepare Path Commands
    tvg::PathCommand cmds[11];
    cmds[0] = tvg::PathCommand::MoveTo;
    cmds[1] = tvg::PathCommand::LineTo;
    cmds[2] = tvg::PathCommand::LineTo;
    cmds[3] = tvg::PathCommand::LineTo;
    cmds[4] = tvg::PathCommand::LineTo;
    cmds[5] = tvg::PathCommand::LineTo;
    cmds[6] = tvg::PathCommand::LineTo;
    cmds[7] = tvg::PathCommand::LineTo;
    cmds[8] = tvg::PathCommand::LineTo;
    cmds[9] = tvg::PathCommand::LineTo;
    cmds[10] = tvg::PathCommand::Close;

    //Prepare Path Points
    tvg::Point pts[10];
    pts[0] = {199, 34};    //MoveTo
    pts[1] = {253, 143};   //LineTo
    pts[2] = {374, 160};   //LineTo
    pts[3] = {287, 244};   //LineTo
    pts[4] = {307, 365};   //LineTo
    pts[5] = {199, 309};   //LineTo
    pts[6] = {97, 365};    //LineTo
    pts[7] = {112, 245};   //LineTo
    pts[8] = {26, 161};    //LineTo
    pts[9] = {146, 143};   //LineTo

    auto shape1 = tvg::Shape::gen();
    shape1->appendPath(cmds, 11, pts, 10);     //copy path data
    shape1->fill(0, 255, 0);
    if (canvas->push(std::move(shape1)) != tvg::Result::Success) return;

    /* Circle */
    auto cx = 550.0f;
    auto cy = 550.0f;
    auto radius = 125.0f;
    auto halfRadius = radius * 0.552284f;

    //Prepare Path Commands
    tvg::PathCommand cmds2[6];
    cmds2[0] = tvg::PathCommand::MoveTo;
    cmds2[1] = tvg::PathCommand::CubicTo;
    cmds2[2] = tvg::PathCommand::CubicTo;
    cmds2[3] = tvg::PathCommand::CubicTo;
    cmds2[4] = tvg::PathCommand::CubicTo;
    cmds2[5] = tvg::PathCommand::Close;

    //Prepare Path Points
    tvg::Point pts2[13];
    pts2[0] = {cx, cy - radius};    //MoveTo
    //CubicTo 1
    pts2[1] = {cx + halfRadius, cy - radius};      //Ctrl1
    pts2[2] = {cx + radius, cy - halfRadius};      //Ctrl2
    pts2[3] = {cx + radius, cy};                   //To
    //CubicTo 2
    pts2[4] = {cx + radius, cy + halfRadius};      //Ctrl1
    pts2[5] = {cx + halfRadius, cy + radius};      //Ctrl2
    pts2[6] = {cx, cy+ radius};                    //To
    //CubicTo 3
    pts2[7] = {cx - halfRadius, cy + radius};      //Ctrl1
    pts2[8] = {cx - radius, cy + halfRadius};      //Ctrl2
    pts2[9] = {cx - radius, cy};                   //To
    //CubicTo 4
    pts2[10] = {cx - radius, cy - halfRadius};     //Ctrl1
    pts2[11] = {cx - halfRadius, cy - radius};     //Ctrl2
    pts2[12] = {cx, cy - radius};                  //To

    auto shape2 = tvg::Shape::gen();
    shape2->appendPath(cmds2, 6, pts2, 13);     //copy path data
    shape2->fill(255, 255, 0);
    if (canvas->push(std::move(shape2)) != tvg::Result::Success) return;

}

/************************************************************************/
/* Main Code                                                            */
/************************************************************************/

int main(int argc, char **argv)
{
    auto tvgEngine = tvg::CanvasEngine::Sw;

    if (argc > 1) {
        if (!strcmp(argv[1], "gl")) tvgEngine = tvg::CanvasEngine::Gl;
    }

    //Threads Count
    auto threads = std::thread::hardware_concurrency();
    if (threads > 0) --threads;    //Allow the designated main thread capacity

    //Initialize ThorVG Engine
    if (tvg::Initializer::init(threads) == tvg::Result::Success) {

        plat_init(argc, argv);

        if (tvgEngine == tvg::CanvasEngine::Sw) {
            createSwView();
        } else {
            createGlView();
        }

        plat_run();
        plat_shutdown();

        //Terminate ThorVG Engine
        tvg::Initializer::term();

    } else {
        cout << "engine is not supported" << endl;
    }
    return 0;
}
