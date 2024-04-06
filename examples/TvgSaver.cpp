/*
 * Copyright (c) 2021 - 2024 the ThorVG project. All rights reserved.

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

unique_ptr<tvg::Paint> tvgTexmap(uint32_t * data, int width, int heigth)
{
    auto texmap = tvg::Picture::gen();
    if (texmap->load(data, width, heigth, true, true) != tvg::Result::Success) return nullptr;
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

    if (texmap->mesh(triangles, 4) != tvg::Result::Success) return nullptr;

    return texmap;
}

unique_ptr<tvg::Paint> tvgClippedImage(uint32_t * data, int width, int heigth)
{
    auto image = tvg::Picture::gen();
    if (image->load(data, width, heigth, true, true) != tvg::Result::Success) return nullptr;
    image->translate(400, 0);
    image->scale(2);

    auto imageClip = tvg::Shape::gen();
    imageClip->appendCircle(400, 200, 80, 180);
    imageClip->translate(200, 0);
    image->composite(std::move(imageClip), tvg::CompositeMethod::ClipPath);

    return image;
}

unique_ptr<tvg::Paint> tvgMaskedSvg()
{
    auto svg = tvg::Picture::gen();
    if (svg->load(EXAMPLE_DIR"/svg/tiger.svg") != tvg::Result::Success) return nullptr;
    svg->opacity(200);
    svg->scale(0.3);
    svg->translate(50, 450);

    auto svgMask = tvg::Shape::gen();
    tvgDrawStar(svgMask.get());
    svgMask->fill(0, 0, 0);
    svgMask->translate(30, 440);
    svgMask->opacity(200);
    svgMask->scale(0.7);
    svg->composite(std::move(svgMask), tvg::CompositeMethod::AlphaMask);

    return svg;
}

unique_ptr<tvg::Paint> tvgNestedPaints(tvg::Fill::ColorStop* colorStops, int colorStopsCnt)
{
    auto scene = tvg::Scene::gen();
    scene->translate(100, 100);

    auto scene2 = tvg::Scene::gen();
    scene2->rotate(10);
    scene2->scale(2);
    scene2->translate(400,400);

    auto shape = tvg::Shape::gen();
    shape->appendRect(50, 0, 50, 100, 10, 40);
    shape->fill(0, 0, 255, 125);
    scene2->push(std::move(shape));

    scene->push(std::move(scene2));

    auto shape2 = tvg::Shape::gen();
    shape2->appendRect(0, 0, 50, 100, 10, 40);
    auto fillShape = tvg::RadialGradient::gen();
    fillShape->radial(25, 50, 25);
    fillShape->colorStops(colorStops, colorStopsCnt);
    shape2->fill(std::move(fillShape));
    shape2->scale(2);
    shape2->opacity(200);
    shape2->translate(400, 400);
    scene->push(std::move(shape2));

    return scene;
}

unique_ptr<tvg::Paint> tvgGradientShape(tvg::Fill::ColorStop* colorStops, int colorStopsCnt)
{
    float dashPattern[2] = {30, 40};

    //gradient shape + dashed stroke
    auto fillStroke = tvg::LinearGradient::gen();
    fillStroke->linear(20, 120, 380, 280);
    fillStroke->colorStops(colorStops, colorStopsCnt);

    auto fillShape = tvg::LinearGradient::gen();
    fillShape->linear(20, 120, 380, 280);
    fillShape->colorStops(colorStops, colorStopsCnt);

    auto shape = tvg::Shape::gen();
    shape->appendCircle(200, 200, 180, 80);
    shape->fill(std::move(fillShape));
    shape->strokeWidth(20);
    shape->strokeDash(dashPattern, 2);
    shape->strokeFill(std::move(fillStroke));

    return shape;
}

unique_ptr<tvg::Paint> tvgCircle1(tvg::Fill::ColorStop* colorStops, int colorStopsCnt)
{
    auto circ = tvg::Shape::gen();
    circ->appendCircle(400, 375, 50, 50);
    circ->fill(0, 255, 0, 155);

    return circ;
}

unique_ptr<tvg::Paint> tvgCircle2(tvg::Fill::ColorStop* colorStops, int colorStopsCnt)
{
    auto circ = tvg::Shape::gen();
    circ->appendCircle(400, 425, 50, 50);
    auto fill = tvg::RadialGradient::gen();
    fill->radial(400, 425, 50);
    fill->colorStops(colorStops, colorStopsCnt);
    circ->fill(std::move(fill));

    return circ;
}

void exportTvg()
{
    //prepare the main scene
    auto scene = tvg::Scene::gen();

    //prepare image source
    const int width = 200;
    const int height = 300;
    ifstream file(EXAMPLE_DIR"/image/rawimage_200x300.raw", ios::binary);
    if (!file.is_open()) return;
    uint32_t *data = (uint32_t*) malloc(sizeof(uint32_t) * width * height);
    if (!data) return;
    file.read(reinterpret_cast<char*>(data), sizeof(uint32_t) * width * height);
    file.close();

    //texmap image
    auto texmap = tvgTexmap(data, width, height);
    if (scene->push(std::move(texmap)) != tvg::Result::Success) return;

    //clipped image
    auto image = tvgClippedImage(data, width, height);
    if (scene->push(std::move(image)) != tvg::Result::Success) return;

    free(data);

    //prepare gradient common data
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

    //gradient shape + dashed stroke
    auto shape1 = tvgGradientShape(colorStops1, 3);
    if (scene->push(std::move(shape1)) != tvg::Result::Success) return;

    //nested paints
    auto scene2 = tvgNestedPaints(colorStops2, 2);
    if (scene->push(std::move(scene2)) != tvg::Result::Success) return;

    //masked svg file
    auto svg = tvgMaskedSvg();
    if (scene->push(std::move(svg)) != tvg::Result::Success) return;

    //solid top circle and gradient bottom circle
    auto circ1 = tvgCircle1(colorStops3, 2);
    if (scene->push(std::move(circ1)) != tvg::Result::Success) return;

    auto circ2 = tvgCircle2(colorStops3, 2);
    if (scene->push(std::move(circ2)) != tvg::Result::Success) return;

    //inv mask applied to the main scene
    auto mask = tvg::Shape::gen();
    mask->appendCircle(400, 400, 15, 15);
    mask->fill(0, 0, 0);
    scene->composite(std::move(mask), tvg::CompositeMethod::InvAlphaMask);

    //save the tvg file
    auto saver = tvg::Saver::gen();
    if (saver->save(std::move(scene), EXAMPLE_DIR"/tvg/test.tvg") == tvg::Result::Success) {
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
    //Threads Count
    auto threads = std::thread::hardware_concurrency();
    if (threads > 0) --threads;    //Allow the designated main thread capacity

    //Initialize ThorVG Engine
    if (tvg::Initializer::init(threads) == tvg::Result::Success) {

        exportTvg();

        //Terminate ThorVG Engine
        tvg::Initializer::term();

    } else {
        cout << "engine is not supported" << endl;
    }
    return 0;
}
