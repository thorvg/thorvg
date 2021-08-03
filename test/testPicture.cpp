/*
 * Copyright (c) 2021 Samsung Electronics Co., Ltd. All rights reserved.

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

#include <thorvg.h>
#include <string.h>
#include <fstream>
#include "catch.hpp"

using namespace tvg;
using namespace std;

TEST_CASE("Load SVG file", "[tvgPicture]")
{
    auto picture = Picture::gen();
    REQUIRE(picture);

    //Invalid file
    REQUIRE(picture->load("invalid.svg") == Result::InvalidArguments);

    //Load Svg file
    REQUIRE(picture->load(TEST_DIR"/logo.svg") == Result::Success);

    float w, h;
    REQUIRE(picture->size(&w, &h) == Result::Success);
}

TEST_CASE("Load SVG Data", "[tvgPicture]")
{
    static const char* svg = "<svg height=\"1000\" viewBox=\"0 0 1000 1000\" width=\"1000\" xmlns=\"http://www.w3.org/2000/svg\"><path d=\"M.10681413.09784845 1000.0527.01592069V1000.0851L.06005738 999.9983Z\" fill=\"#ffffff\" stroke-width=\"3.910218\"/><g fill=\"#252f35\"><g stroke-width=\"3.864492\"><path d=\"M256.61221 100.51736H752.8963V386.99554H256.61221Z\"/><path d=\"M201.875 100.51736H238.366478V386.99554H201.875Z\"/><path d=\"M771.14203 100.51736H807.633508V386.99554H771.14203Z\"/></g><path d=\"M420.82388 380H588.68467V422.805317H420.82388Z\" stroke-width=\"3.227\"/><path d=\"m420.82403 440.7101v63.94623l167.86079 25.5782V440.7101Z\"/><path d=\"M420.82403 523.07258V673.47362L588.68482 612.59701V548.13942Z\"/></g><g fill=\"#222f35\"><path d=\"M420.82403 691.37851 588.68482 630.5019 589 834H421Z\"/><path d=\"m420.82403 852.52249h167.86079v28.64782H420.82403v-28.64782 0 0\"/><path d=\"m439.06977 879.17031c0 0-14.90282 8.49429-18.24574 15.8161-4.3792 9.59153 0 31.63185 0 31.63185h167.86079c0 0 4.3792-22.04032 0-31.63185-3.34292-7.32181-18.24574-15.8161-18.24574-15.8161z\"/></g><g fill=\"#ffffff\"><path d=\"m280 140h15v55l8 10 8-10v-55h15v60l-23 25-23-25z\"/><path d=\"m335 140v80h45v-50h-25v10h10v30h-15v-57h18v-13z\"/></g></svg>";

    auto picture = Picture::gen();
    REQUIRE(picture);

    //Negative cases
    REQUIRE(picture->load(nullptr, 100, "") == Result::InvalidArguments);
    REQUIRE(picture->load(svg, 0, "") == Result::InvalidArguments);

    //Positive cases
    REQUIRE(picture->load(svg, strlen(svg), "svg") == Result::Success);

    float w, h;
    REQUIRE(picture->size(&w, &h) == Result::Success);
    REQUIRE(w == 1000);
    REQUIRE(h == 1000);
}

TEST_CASE("Load RAW Data", "[tvgPicture]")
{
    auto picture = Picture::gen();
    REQUIRE(picture);

    string path(TEST_DIR"/rawimage_200x300.raw");

    ifstream file(path);
    if (!file.is_open()) return;
    auto data = (uint32_t*)malloc(sizeof(uint32_t) * (200*300));
    file.read(reinterpret_cast<char *>(data), sizeof (uint32_t) * 200 * 300);
    file.close();

    //Negative cases
    REQUIRE(picture->load(nullptr, 200, 300, false) == Result::InvalidArguments);
    REQUIRE(picture->load(data, 0, 0, false) == Result::InvalidArguments);
    REQUIRE(picture->load(data, 200, 0, false) == Result::InvalidArguments);
    REQUIRE(picture->load(data, 0, 300, false) == Result::InvalidArguments);

    //Positive cases
    REQUIRE(picture->load(data, 200, 300, false) == Result::Success);
    REQUIRE(picture->load(data, 200, 300, true) == Result::Success);

    float w, h;
    REQUIRE(picture->size(&w, &h) == Result::Success);

    REQUIRE(w == 200);
    REQUIRE(h == 300);

    free(data);
}

TEST_CASE("Load PNG file from path", "[tvgPicture]")
{
    auto picture = Picture::gen();
    REQUIRE(picture);

    //Invalid file
    REQUIRE(picture->load("invalid.png") == Result::InvalidArguments);

    REQUIRE(picture->load(TEST_DIR"/test.png") == Result::Success);

    float w, h;
    REQUIRE(picture->size(&w, &h) == Result::Success);

    REQUIRE(w == 512);
    REQUIRE(h == 512);
}

TEST_CASE("Load PNG file from data", "[tvgPicture]")
{
    auto picture = Picture::gen();
    REQUIRE(picture);

    //Open file
    ifstream file(TEST_DIR"/test.png");
    REQUIRE(file.is_open());
    auto size = sizeof(uint32_t) * (1000*1000);
    auto data = (char*)malloc(size);
    file.read(data, size);
    file.close();

    REQUIRE(picture->load(data, size, "", false) == Result::Success);
    REQUIRE(picture->load(data, size, "png", true) == Result::Success);

    float w, h;
    REQUIRE(picture->size(&w, &h) == Result::Success);
    REQUIRE(w == 512);
    REQUIRE(h == 512);

    free(data);
}

TEST_CASE("Load JPG file from path", "[tvgPicture]")
{
    auto picture = Picture::gen();
    REQUIRE(picture);

    //Invalid file
    REQUIRE(picture->load("invalid.jpg") == Result::InvalidArguments);

    REQUIRE(picture->load(TEST_DIR"/test.jpg") == Result::Success);

    float w, h;
    REQUIRE(picture->size(&w, &h) == Result::Success);

    REQUIRE(w == 512);
    REQUIRE(h == 512);
}

TEST_CASE("Load JPG file from data", "[tvgPicture]")
{
    auto picture = Picture::gen();
    REQUIRE(picture);

    //Open file
    ifstream file(TEST_DIR"/test.jpg");
    REQUIRE(file.is_open());
    auto begin = file.tellg();
    file.seekg(0, std::ios::end);
    auto size = file.tellg() - begin;
    auto data = (char*)malloc(size);
    file.seekg(0, std::ios::beg);
    file.read(data, size);
    file.close();

    REQUIRE(picture->load(data, size, "", false) == Result::Success);
    REQUIRE(picture->load(data, size, "jpg", true) == Result::Success);

    float w, h;
    REQUIRE(picture->size(&w, &h) == Result::Success);
    REQUIRE(w == 512);
    REQUIRE(h == 512);

    free(data);
}

TEST_CASE("Picture Size", "[tvgPicture]")
{
    auto picture = Picture::gen();
    REQUIRE(picture);

    float w, h;
    REQUIRE(picture->size(&w, &h) == Result::InsufficientCondition);

    REQUIRE(picture->load(TEST_DIR"/logo.svg") == Result::Success);

    REQUIRE(picture->size(nullptr, nullptr) == Result::Success);
    REQUIRE(picture->size(100, 100) == Result::Success);
    REQUIRE(picture->size(&w, &h) == Result::Success);
    REQUIRE(w == 100);
    REQUIRE(h == 100);

    REQUIRE(picture->load(TEST_DIR"/tiger.svg") == Result::Success);
    REQUIRE(picture->size(&w, &h) == Result::Success);
    REQUIRE(picture->size(w, h) == Result::Success);
}

TEST_CASE("Load SVG file and render", "[tvgPicture]")
{
    REQUIRE(Initializer::init(CanvasEngine::Sw, 0) == Result::Success);

    auto canvas = SwCanvas::gen();
    REQUIRE(canvas);

    uint32_t buffer[100*100];
    REQUIRE(canvas->target(buffer, 100, 100, 100, SwCanvas::Colorspace::ABGR8888) == Result::Success);

    auto picture = Picture::gen();
    REQUIRE(picture);

    REQUIRE(picture->load(TEST_DIR"/tag.svg") == Result::Success);

    REQUIRE(canvas->push(move(picture)) == Result::Success);

    REQUIRE(Initializer::term(CanvasEngine::Sw) == Result::Success);
}

TEST_CASE("Load PNG file and render", "[tvgPicture]")
{
    REQUIRE(Initializer::init(CanvasEngine::Sw, 0) == Result::Success);

    auto canvas = SwCanvas::gen();
    REQUIRE(canvas);

    uint32_t buffer[100*100];
    REQUIRE(canvas->target(buffer, 100, 100, 100, SwCanvas::Colorspace::ABGR8888) == Result::Success);

    auto picture = Picture::gen();
    REQUIRE(picture);

    REQUIRE(picture->load(TEST_DIR"/test.png") == Result::Success);

    REQUIRE(canvas->push(move(picture)) == Result::Success);

    REQUIRE(Initializer::term(CanvasEngine::Sw) == Result::Success);
}

TEST_CASE("Load JPG file and render", "[tvgPicture]")
{
    REQUIRE(Initializer::init(CanvasEngine::Sw, 0) == Result::Success);

    auto canvas = SwCanvas::gen();
    REQUIRE(canvas);

    uint32_t buffer[100*100];
    REQUIRE(canvas->target(buffer, 100, 100, 100, SwCanvas::Colorspace::ABGR8888) == Result::Success);

    auto picture = Picture::gen();
    REQUIRE(picture);

    REQUIRE(picture->load(TEST_DIR"/test.jpg") == Result::Success);

    REQUIRE(canvas->push(move(picture)) == Result::Success);

    REQUIRE(Initializer::term(CanvasEngine::Sw) == Result::Success);
}
