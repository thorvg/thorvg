/*
 * Copyright (c) 2020-2021 Samsung Electronics Co., Ltd. All rights reserved.

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
#include <vector>
#include "lodepng.h"

using namespace std;


struct PngBuilder
{
    void build(const std::string &fileName , const uint32_t width, const uint32_t height, uint32_t *buffer)
    {
        std::vector<unsigned char> image;
        image.resize(width * height * 4);
        for (unsigned y = 0; y < height; y++) {
            for (unsigned x = 0; x < width; x++) {
                uint32_t n = buffer[ y * width + x ];
                image[4 * width * y + 4 * x + 0] = (n >> 16) & 0xff;
                image[4 * width * y + 4 * x + 1] = (n >> 8) & 0xff;
                image[4 * width * y + 4 * x + 2] = n & 0xff;
                image[4 * width * y + 4 * x + 3] = (n >> 24) & 0xff;
            }
        }
        unsigned error = lodepng::encode(fileName, image, width, height);

        //if there's an error, display it
        if (error) std::cout << "encoder error " << error << ": "<< lodepng_error_text(error) << std::endl;
    }
};


struct App
{
    void tvgRender(int w, int h)
    {
        tvg::CanvasEngine tvgEngine = tvg::CanvasEngine::Sw;

        //Threads Count
        auto threads = std::thread::hardware_concurrency();

        //Initialize ThorVG Engine
        if (tvg::Initializer::init(tvgEngine, threads) == tvg::Result::Success) {
            //Create a Canvas
            auto canvas = tvg::SwCanvas::gen();

            //Create a Picture
            auto picture = tvg::Picture::gen();
            if (picture->load(fileName) != tvg::Result::Success) return;

            float fw, fh;
            picture->size(&fw, &fh);

            //Proper size is not specified, Get default size.
            if (w == 0 || h == 0) {
                width = static_cast<uint32_t>(fw);
                height = static_cast<uint32_t>(fh);
            //Otherwise, transform size to keep aspect ratio
            } else {
                width = w;
                height = h;
                picture->size(w, h);
            }

            //Setup the canvas
            auto buffer = (uint32_t*)malloc(sizeof(uint32_t) * width * height);
            canvas->target(buffer, width, width, height, tvg::SwCanvas::ARGB8888);

            //Background color?
            if (bgColor != 0xffffffff) {
                 uint8_t bgColorR = (uint8_t) ((bgColor & 0xff0000) >> 16);
                 uint8_t bgColorG = (uint8_t) ((bgColor & 0x00ff00) >> 8);
                 uint8_t bgColorB = (uint8_t) ((bgColor & 0x0000ff));

                 auto shape = tvg::Shape::gen();
                 shape->appendRect(0, 0, width, height, 0, 0);
                 shape->fill(bgColorR, bgColorG, bgColorB, 255);

                 if (canvas->push(move(shape)) != tvg::Result::Success) return;
            }

            //Drawing
            canvas->push(move(picture));
            canvas->draw();
            canvas->sync();

            //Build Png
            PngBuilder builder;
            builder.build(pngName.data(), width, height, buffer);

            //Terminate ThorVG Engine
            tvg::Initializer::term(tvg::CanvasEngine::Sw);

            free(buffer);

        } else {
            cout << "engine is not supported" << endl;
        }
    }

    int setup(int argc, char **argv, size_t *w, size_t *h)
    {
        char *path{nullptr};

        *w = *h = 0;

        if (argc > 1) path = argv[1];
        if (argc > 2) {
            char tmp[20];
            char *x = strstr(argv[2], "x");
            if (x) {
                snprintf(tmp, x - argv[2] + 1, "%s", argv[2]);
                *w = atoi(tmp);
                snprintf(tmp, sizeof(tmp), "%s", x + 1);
                *h = atoi(tmp);
            }
        }
        if (argc > 3) bgColor = strtol(argv[3], NULL, 16);

        if (!path) return help();

        std::array<char, 5000> memory;

#ifdef _WIN32
        path = _fullpath(memory.data(), path, memory.size());
#else
        path = realpath(path, memory.data());
#endif
        if (!path) return help();

        fileName = std::string(path);

        if (!svgFile()) return help();

        pngName = basename(fileName);
        pngName.append(".png");
        return 0;
    }

private:
    std::string basename(const std::string &str)
    {
        return str.substr(str.find_last_of("/\\") + 1);
    }

    bool svgFile() {
        std::string extn = ".svg";
        if (fileName.size() <= extn.size() || fileName.substr(fileName.size() - extn.size()) != extn)
            return false;

        return true;
    }

    int result() {
        std::cout<<"Generated PNG file : "<<pngName<<std::endl;
        return 0;
    }

    int help() {
        std::cout<<"Usage: \n   svg2png [svgFileName] [Resolution] [bgColor]\n\nExamples: \n    $ svg2png input.svg\n    $ svg2png input.svg 200x200\n    $ svg2png input.svg 200x200 ff00ff\n\n";
        return 1;
    }

private:
    uint32_t bgColor = 0xffffffff;
    uint32_t width = 0;
    uint32_t height = 0;
    std::string fileName;
    std::string pngName;
};

int main(int argc, char **argv)
{
    App app;
    size_t w, h;

    if (app.setup(argc, argv, &w, &h)) return 1;

    app.tvgRender(w, h);

    return 0;
}
