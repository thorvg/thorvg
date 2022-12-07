/*
 * Copyright (c) 2021 - 2022 Samsung Electronics Co., Ltd. All rights reserved.

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
#include <fstream>
#include <thread>
#include <string.h>
#include <thorvg.h>

using namespace std;

/************************************************************************/
/* Drawing Commands                                                     */
/************************************************************************/

void tvgDrawStar(tvg::Shape* star)
{
    star->moveTo(199, 34);
    star->lineTo(253, 143);
    star->lineTo(374, 160);
    star->lineTo(287, 244);
    star->lineTo(307, 365);
    star->lineTo(199, 309);
    star->lineTo(97, 365);
    star->lineTo(112, 245);
    star->lineTo(26, 161);
    star->lineTo(146, 143);
    star->close();
}

void exportTvg()
{
    //prepare the main scene
    auto scene = tvg::Scene::gen();

    //image data
    ifstream file(EXAMPLE_DIR"/rawimage_200x300.raw");
    if (!file.is_open()) return;
    uint32_t *data = (uint32_t*) malloc(sizeof(uint32_t) * 200 * 300);
    if (!data) return;
    file.read(reinterpret_cast<char*>(data), sizeof(uint32_t) * 200 * 300);
    file.close();

    //texmap image
    auto texmap = tvg::Picture::gen();
    if (texmap->load(data, 200, 300, true) != tvg::Result::Success) return;
    texmap->translate(100, 100);

    //Composing Meshes
    tvg::Polygon triangles[4];
    triangles[0].vertex[0] = {{100, 125}, {0, 0}};
    triangles[0].vertex[1] = {{300, 100}, {0.5, 0}};
    triangles[0].vertex[2] = {{200, 550}, {0, 1}};

    triangles[1].vertex[0] = {{300, 100}, {0.5, 0}};
    triangles[1].vertex[1] = {{350, 450}, {0.5, 1}};
    triangles[1].vertex[2] = {{200, 550}, {0, 1}};

    triangles[2].vertex[0] = {{300, 100}, {0.5, 0}};
    triangles[2].vertex[1] = {{500, 200}, {1, 0}};
    triangles[2].vertex[2] = {{350, 450}, {0.5, 1}};

    triangles[3].vertex[0] = {{500, 200}, {1, 0}};
    triangles[3].vertex[1] = {{450, 450}, {1, 1}};
    triangles[3].vertex[2] = {{350, 450}, {0.5, 1}};

    if (texmap->mesh(triangles, 4) != tvg::Result::Success) return;
    if (scene->push(move(texmap)) != tvg::Result::Success) return;

    //clipped image
    auto image = tvg::Picture::gen();
    if (image->load(data, 200, 300, true) != tvg::Result::Success) return;
    image->translate(400, 0);
    image->scale(2);

    auto imageClip = tvg::Shape::gen();
    imageClip->appendCircle(400, 200, 80, 180);
    imageClip->fill(0, 0, 0, 155);
    imageClip->translate(200, 0);
    image->composite(move(imageClip), tvg::CompositeMethod::ClipPath);

    if (scene->push(move(image)) != tvg::Result::Success) return;
    free(data);

    tvg::Fill::ColorStop colorStops1[3];
    colorStops1[0] = {0, 255, 0, 0, 255};
    colorStops1[1] = {0.5, 0, 0, 255, 127};
    colorStops1[2] = {1, 127, 127, 127, 127};

    tvg::Fill::ColorStop colorStops2[2];
    colorStops2[0] = {0, 255, 0, 0, 255};
    colorStops2[1] = {1, 50, 0, 255, 255};

    tvg::Fill::ColorStop colorStops3[2];
    colorStops3[0] = {0, 0, 0, 255, 155};
    colorStops3[1] = {1, 0, 255, 0, 155};

    float dashPattern[2] = {30, 40};

    //gradient shape + dashed stroke
    auto fillStroke1 = tvg::LinearGradient::gen();
    fillStroke1->linear(20, 120, 380, 280);
    fillStroke1->colorStops(colorStops1, 3);

    auto fillShape1 = tvg::LinearGradient::gen();
    fillShape1->linear(20, 120, 380, 280);
    fillShape1->colorStops(colorStops1, 3);

    auto shape1 = tvg::Shape::gen();
    shape1->appendCircle(200, 200, 180, 80);
    shape1->fill(move(fillShape1));
    shape1->stroke(20);
    shape1->stroke(dashPattern, 2);
    shape1->stroke(move(fillStroke1));

    if (scene->push(move(shape1)) != tvg::Result::Success) return;

    //nested paints
    auto scene2 = tvg::Scene::gen();
    scene2->translate(100, 100);

    auto scene3 = tvg::Scene::gen();
    scene3->rotate(10);
    scene3->scale(2);
    scene3->translate(400,400);

    auto shape2 = tvg::Shape::gen();
    shape2->appendRect(50, 0, 50, 100, 10, 40);
    shape2->fill(0, 0, 255, 125);
    scene3->push(move(shape2));

    scene2->push(move(scene3));

    auto shape3 = tvg::Shape::gen();
    shape3->appendRect(0, 0, 50, 100, 10, 40);
    auto fillShape3 = tvg::RadialGradient::gen();
    fillShape3->radial(25, 50, 25);
    fillShape3->colorStops(colorStops2, 2);
    shape3->fill(move(fillShape3));
    shape3->scale(2);
    shape3->opacity(200);
    shape3->translate(400, 400);
    scene2->push(move(shape3));

    if (scene->push(move(scene2)) != tvg::Result::Success) return;

    //masked svg file
    auto svg = tvg::Picture::gen();
    if (svg->load(EXAMPLE_DIR"/tiger.svg") != tvg::Result::Success) return;
    svg->opacity(200);
    svg->scale(0.3);
    svg->translate(50, 450);

    auto svgMask = tvg::Shape::gen();
    tvgDrawStar(svgMask.get());
    svgMask->fill(0, 0, 0, 255);
    svgMask->translate(30, 440);
    svgMask->opacity(200);
    svgMask->scale(0.7);
    svg->composite(move(svgMask), tvg::CompositeMethod::AlphaMask);

    if (scene->push(move(svg)) != tvg::Result::Success) return;

    //solid top circle and gradient bottom circle
    auto circ1 = tvg::Shape::gen();
    circ1->appendCircle(400, 375, 50, 50);
    auto fill1 = tvg::RadialGradient::gen();
    fill1->radial(400, 375, 50);
    fill1->colorStops(colorStops3, 2);
    circ1->fill(move(fill1));
    circ1->fill(0, 255, 0, 155);

    auto circ2 = tvg::Shape::gen();
    circ2->appendCircle(400, 425, 50, 50);
    circ2->fill(0, 255, 0, 155);
    auto fill2 = tvg::RadialGradient::gen();
    fill2->radial(400, 425, 50);
    fill2->colorStops(colorStops3, 2);
    circ2->fill(move(fill2));

    if (scene->push(move(circ1)) != tvg::Result::Success) return;
    if (scene->push(move(circ2)) != tvg::Result::Success) return;

    //inv mask applied to the main scene
    auto mask = tvg::Shape::gen();
    mask->appendCircle(400, 400, 15, 15);
    mask->fill(0, 0, 0, 255);
    scene->composite(move(mask), tvg::CompositeMethod::InvAlphaMask);

    //save the tvg file
    auto saver = tvg::Saver::gen();
    if (saver->save(move(scene), EXAMPLE_DIR"/test.tvg") == tvg::Result::Success) {
        saver->sync();
        cout << "Successfully exported to test.tvg, Please check the result using PictureTvg!" << endl;
        return;
    }
    cout << "Problem with saving the test.tvg file. Did you enable TVG Saver?" << endl;
}


/************************************************************************/
/* Main Code                                                            */
/************************************************************************/

int main(int argc, char **argv)
{
    tvg::CanvasEngine tvgEngine = tvg::CanvasEngine::Sw;

    if (argc > 1) {
        if (!strcmp(argv[1], "gl")) tvgEngine = tvg::CanvasEngine::Gl;
    }

    //Initialize ThorVG Engine
    if (tvgEngine == tvg::CanvasEngine::Sw) {
        cout << "tvg engine: software" << endl;
    } else {
        cout << "tvg engine: opengl" << endl;
    }

    //Threads Count
    auto threads = std::thread::hardware_concurrency();

    //Initialize ThorVG Engine
    if (tvg::Initializer::init(tvgEngine, threads) == tvg::Result::Success) {

        exportTvg();

        //Terminate ThorVG Engine
        tvg::Initializer::term(tvgEngine);

    } else {
        cout << "engine is not supported" << endl;
    }
    return 0;
}
