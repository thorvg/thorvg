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

#include "Example.h"

using namespace std;

/************************************************************************/
/* ThorVG Saving Contents                                               */
/************************************************************************/

unique_ptr<tvg::Paint> tvgTexmap(uint32_t * data, int width, int heigth)
{
    auto texmap = tvg::Picture::gen();
    if (!tvgexam::verify(texmap->load(data, width, heigth, true))) return nullptr;
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

    texmap->mesh(triangles, 4);

    return texmap;
}

unique_ptr<tvg::Paint> tvgClippedImage(uint32_t * data, int width, int heigth)
{
    auto image = tvg::Picture::gen();
    if (!tvgexam::verify(image->load(data, width, heigth, true))) return nullptr;
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
    if (!tvgexam::verify(svg->load(EXAMPLE_DIR"/svg/tiger.svg"))) return nullptr;
    svg->opacity(200);
    svg->scale(0.3);
    svg->translate(50, 450);

    auto svgMask = tvg::Shape::gen();

    //star
    svgMask->moveTo(199, 34);
    svgMask->lineTo(253, 143);
    svgMask->lineTo(374, 160);
    svgMask->lineTo(287, 244);
    svgMask->lineTo(307, 365);
    svgMask->lineTo(199, 309);
    svgMask->lineTo(97, 365);
    svgMask->lineTo(112, 245);
    svgMask->lineTo(26, 161);
    svgMask->lineTo(146, 143);
    svgMask->close();

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
    shape->stroke(20);
    shape->stroke(dashPattern, 2);
    shape->stroke(std::move(fillStroke));

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
    file.read(reinterpret_cast<char*>(data), sizeof(uint32_t) * width * height);
    file.close();

    //texmap image
    auto texmap = tvgTexmap(data, width, height);
    scene->push(std::move(texmap));

    //clipped image
    auto image = tvgClippedImage(data, width, height);
    scene->push(std::move(image));

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
    scene->push(std::move(shape1));

    //nested paints
    auto scene2 = tvgNestedPaints(colorStops2, 2);
    scene->push(std::move(scene2));

    //masked svg file
    auto svg = tvgMaskedSvg();
    scene->push(std::move(svg));

    //solid top circle and gradient bottom circle
    auto circ1 = tvgCircle1(colorStops3, 2);
    scene->push(std::move(circ1));

    auto circ2 = tvgCircle2(colorStops3, 2);
    scene->push(std::move(circ2));

    //inv mask applied to the main scene
    auto mask = tvg::Shape::gen();
    mask->appendCircle(400, 400, 15, 15);
    mask->fill(0, 0, 0);
    scene->composite(std::move(mask), tvg::CompositeMethod::InvAlphaMask);

    //save the tvg file
    auto saver = tvg::Saver::gen();
    if (!tvgexam::verify(saver->save(std::move(scene), EXAMPLE_DIR"/tvg/test.tvg"))) return;
    saver->sync();
    cout << "Successfully exported to test.tvg, Please check the result using PictureTvg!" << endl;
}


/************************************************************************/
/* Entry Point                                                          */
/************************************************************************/

int main(int argc, char **argv)
{
    if (tvgexam::verify(tvg::Initializer::init(tvg::CanvasEngine::Sw, 0))) {

        exportTvg();

        tvg::Initializer::term(tvg::CanvasEngine::Sw);
    }
    return 0;
}
