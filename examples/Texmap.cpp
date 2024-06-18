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

#include "Example.h"

/************************************************************************/
/* ThorVG Drawing Contents                                              */
/************************************************************************/

struct UserExample : tvgexam::Example
{
    bool content(tvg::Canvas* canvas, uint32_t w, uint32_t h) override
    {
        if (!canvas) return false;

        //Background
        auto shape = tvg::Shape::gen();
        shape->appendRect(0, 0, w, h);
        shape->fill(255, 255, 255);

        canvas->push(std::move(shape));

        //Raw Image
        string path(EXAMPLE_DIR"/image/rawimage_200x300.raw");

        ifstream file(path, ios::binary);
        if (!file.is_open()) return false;
        auto data = (uint32_t*)malloc(sizeof(uint32_t) * (200*300));
        file.read(reinterpret_cast<char *>(data), sizeof (uint32_t) * 200 * 300);
        file.close();

        //Picture
        auto picture = tvg::Picture::gen();
        if (!tvgexam::verify(picture->load(data, 200, 300, true, true))) return false;

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

        if (!tvgexam::verify(picture->mesh(triangles, 4))) return false;

        //Masking + Opacity
        auto picture2 = tvg::cast<tvg::Picture>(picture->duplicate());
        picture2->translate(400, 400);
        picture2->opacity(200);

        auto mask = tvg::Shape::gen();
        mask->appendCircle(700, 700, 200, 200);
        mask->fill(255, 255, 255);
        picture2->composite(std::move(mask), tvg::CompositeMethod::AlphaMask);

        canvas->push(std::move(picture));
        canvas->push(std::move(picture2));

        free(data);

        return true;
    }
};


/************************************************************************/
/* Entry Point                                                          */
/************************************************************************/

int main(int argc, char **argv)
{
    return tvgexam::main(new UserExample, argc, argv, 1024, 1024);
}