/*
 * Copyright (c) 2021 - 2026 ThorVG project. All rights reserved.

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
#include <fstream>
#include <cstring>
#include "config.h"
#include "testFramework.h"

using namespace tvg;
using namespace std;


TEST_CASE("Picture Creation", "[tvgPicture]")
{
    auto picture = Picture::gen();
    REQUIRE(picture);

    REQUIRE(picture->type() == Type::Picture);

    Paint::rel(picture);
}

TEST_CASE("Load RAW Data", "[tvgPicture]")
{
    auto picture = Picture::gen();
    REQUIRE(picture);

    ifstream file(TEST_DIR"/rawimage_200x300.raw");
    if (!file.is_open()) return;
    auto data = (uint32_t*)malloc(sizeof(uint32_t) * (200*300));
    file.read(reinterpret_cast<char *>(data), sizeof (uint32_t) * 200 * 300);
    file.close();

    //Negative cases
    REQUIRE(picture->load(nullptr, 200, 300, ColorSpace::ARGB8888, false) == Result::InvalidArguments);
    REQUIRE(picture->load(data, 0, 0, ColorSpace::ARGB8888, false) == Result::InvalidArguments);
    REQUIRE(picture->load(data, 200, 0, ColorSpace::ARGB8888, false) == Result::InvalidArguments);
    REQUIRE(picture->load(data, 0, 300, ColorSpace::ARGB8888, false) == Result::InvalidArguments);

    //Positive cases
    REQUIRE(picture->load(data, 200, 300, ColorSpace::ARGB8888, false) == Result::Success);
    REQUIRE(picture->load(data, 200, 300, ColorSpace::ARGB8888, true) == Result::Success);

    float w, h;
    REQUIRE(picture->size(&w, &h) == Result::Success);

    REQUIRE(w == 200);
    REQUIRE(h == 300);

    Paint::rel(picture);

    free(data);
}

TEST_CASE("Picture Size", "[tvgPicture]")
{
    auto picture = Picture::gen();
    REQUIRE(picture);

    float w, h;
    REQUIRE(picture->size(&w, &h) == Result::InsufficientCondition);

    //Primary
    ifstream file(TEST_DIR"/rawimage_200x300.raw");
    if (!file.is_open()) return;
    auto data = (uint32_t*)malloc(sizeof(uint32_t) * (200*300));
    file.read(reinterpret_cast<char *>(data), sizeof (uint32_t) * 200 * 300);
    file.close();

    REQUIRE(picture->load(data, 200, 300, ColorSpace::ARGB8888, false) == Result::Success);

    REQUIRE(picture->size(nullptr, nullptr) == Result::Success);
    REQUIRE(picture->size(100, 100) == Result::Success);
    REQUIRE(picture->size(&w, &h) == Result::Success);
    REQUIRE(w == 100);
    REQUIRE(h == 100);

    free(data);

    //Secondary
    ifstream file2(TEST_DIR"/rawimage_250x375.raw");
    if (!file2.is_open()) return;
    data = (uint32_t*)malloc(sizeof(uint32_t) * (250*375));
    file2.read(reinterpret_cast<char *>(data), sizeof (uint32_t) * 250 * 375);
    file2.close();

    REQUIRE(picture->load(data, 250, 375, ColorSpace::ARGB8888, false) == Result::Success);

    REQUIRE(picture->size(&w, &h) == Result::Success);
    REQUIRE(picture->size(w, h) == Result::Success);

    free(data);

    Paint::rel(picture);
}

TEST_CASE("Picture Origin", "[tvgPicture]")
{
    auto picture = Picture::gen();
    REQUIRE(picture);

    float w, h;
    REQUIRE(picture->size(&w, &h) == Result::InsufficientCondition);

    //Primary
    ifstream file(TEST_DIR"/rawimage_200x300.raw");
    if (!file.is_open()) return;
    auto data = (uint32_t*)malloc(sizeof(uint32_t) * (200*300));
    file.read(reinterpret_cast<char *>(data), sizeof (uint32_t) * 200 * 300);
    file.close();

    REQUIRE(picture->load(data, 200, 300, ColorSpace::ARGB8888, false) == Result::Success);
    REQUIRE(picture->origin(0.0f, 0.0f) == Result::Success);
    REQUIRE(picture->origin(0.5f, 0.5f) == Result::Success);
    REQUIRE(picture->origin(1.0f, 1.0f) == Result::Success);
    REQUIRE(picture->origin(-1.0f, -1.0f) == Result::Success);

    free(data);

    Paint::rel(picture);
}

TEST_CASE("Picture Resolver", "[tvgPicture]")
{
    auto picture = Picture::gen();
    REQUIRE(picture);

    float w, h;
    REQUIRE(picture->size(&w, &h) == Result::InsufficientCondition);

    //Primary
    ifstream file(TEST_DIR"/rawimage_200x300.raw");
    if (!file.is_open()) return;
    auto data = (uint32_t*)malloc(sizeof(uint32_t) * (200*300));
    file.read(reinterpret_cast<char *>(data), sizeof (uint32_t) * 200 * 300);
    file.close();

    auto resolver = [](Paint* paint, const char* src, void *data) -> bool
    {
        return false;
    };

    REQUIRE(picture->resolver(resolver, nullptr) == Result::Success);

    REQUIRE(picture->load(data, 200, 300, ColorSpace::ARGB8888, false) == Result::Success);

    free(data);

    Paint::rel(picture);
}

TEST_CASE("Picture Duplication", "[tvgPicture]")
{
    auto picture = Picture::gen();
    REQUIRE(picture);

    //Primary
    ifstream file(TEST_DIR"/rawimage_200x300.raw");
    if (!file.is_open()) return;
    auto data = (uint32_t*)malloc(sizeof(uint32_t) * (200*300));
    file.read(reinterpret_cast<char *>(data), sizeof (uint32_t) * 200 * 300);
    file.close();

    REQUIRE(picture->load(data, 200, 300, ColorSpace::ARGB8888, false) == Result::Success);
    REQUIRE(picture->size(100, 100) == Result::Success);

    auto dup = (Picture*)picture->duplicate();
    REQUIRE(dup);

    float w, h;
    REQUIRE(picture->size(&w, &h) == Result::Success);
    REQUIRE(w == 100);
    REQUIRE(h == 100);

    free(data);

    Paint::rel(dup);
    Paint::rel(picture);
}

#ifdef THORVG_SVG_LOADER_SUPPORT

TEST_CASE("Load SVG file", "[tvgPicture]")
{
    REQUIRE(Initializer::init() == Result::Success);
    {
        // for test coverage
        Text::load(TEST_DIR"/PublicSans-Regular.ttf");

        auto picture = Picture::gen();
        REQUIRE(picture);

        //Invalid file
        REQUIRE(picture->load("invalid.svg") == Result::InvalidArguments);

        //Load Svg file
        REQUIRE(picture->load(TEST_DIR"/test1.svg") == Result::Success);
        REQUIRE(picture->load(TEST_DIR"/test2.svg") == Result::Success);
        REQUIRE(picture->load(TEST_DIR"/test3.svg") == Result::Success);
        REQUIRE(picture->load(TEST_DIR"/test4.svg") == Result::Success);

        float w, h;
        REQUIRE(picture->size(&w, &h) == Result::Success);

        Paint::rel(picture);
    }
    REQUIRE(Initializer::term() == Result::Success);
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

    Paint::rel(picture);
}

TEST_CASE("SVG Color and URL Decoding", "[tvgPicture]")
{
    REQUIRE(Initializer::init() == Result::Success);
    {
        // 1. SVG Color Specification (HSL, RGB, Hex, Alpha)
        struct ColorTestCase {
            const char* desc;
            const char* color;
            uint8_t expR, expG, expB, expA;
        };

        static const ColorTestCase cases[] = {
            // HSL Hue sectors (0..5)
            {"HSL Sector 0 (Red-Yellow)",    "hsl(30, 100%, 50%)",     255, 128,   0, 255},
            {"HSL Sector 1 (Yellow-Green)",  "hsl(90, 100%, 50%)",     128, 255,   0, 255},
            {"HSL Sector 2 (Green-Cyan)",    "hsl(150, 100%, 50%)",      0, 255, 128, 255},
            {"HSL Sector 3 (Cyan-Blue)",     "hsl(210, 100%, 50%)",      0, 128, 255, 255},
            {"HSL Sector 4 (Blue-Magenta)",  "hsl(270, 100%, 50%)",    128,   0, 255, 255},
            {"HSL Sector 5 (Magenta-Red)",   "hsl(330, 100%, 50%)",    255,   0, 128, 255},

            // HSL Boundary & Precision checks
            {"HSL Zero Saturation (Gray)",   "hsl(0, 0%, 50%)",        128, 128, 128, 255},
            {"HSL Hue 360",                  "hsl(360, 100%, 50%)",    255,   0,   0, 255},
            {"HSL Negative Hue",             "hsl(-60, 100%, 50%)",    255,   0, 255, 255},
            {"HSL Hue > 360",                "hsl(420, 100%, 50%)",    255, 255,   0, 255},
            {"HSL Lightness <= 0.5",         "hsl(0, 100%, 25%)",      128,   0,   0, 255},
            {"HSL Lightness > 0.5",          "hsl(0, 100%, 75%)",      255, 128, 128, 255},
            {"HSL Zero Lightness (Black)",   "hsl(0, 100%, 0%)",         0,   0,   0, 255},
            {"HSL Precision Default Branch", "hsl(-0.00001, 100%, 50%)",   0,   0,   0, 255},

            // RGB Percentage, RGBA with Alpha, Hex Colors
            {"RGB Percentage",               "rgb(100%, 50%, 0%)",     255, 128,   0, 255},
            {"RGBA Alpha",                   "rgba(255, 0, 0, 0.5)",   255,   0,   0, 128},
            {"Short Hex",                    "#f00",                   255,   0,   0, 255},
        };

        for (const auto& tc : cases) {
            CAPTURE(tc.desc);
            CAPTURE(tc.color);

            char svg[256];
            snprintf(svg, sizeof(svg),
                     "<svg viewBox=\"0 0 10 10\" xmlns=\"http://www.w3.org/2000/svg\">"
                     "<rect id=\"target\" fill=\"%s\" width=\"10\" height=\"10\"/>"
                     "</svg>",
                     tc.color);

            auto picture = Picture::gen();
            REQUIRE(picture);
            picture->accessible = true;
            REQUIRE(picture->load(svg, strlen(svg), "svg") == Result::Success);

            auto shape = static_cast<const Shape*>(picture->paint(Accessor::id("target")));
            REQUIRE(shape);

            uint8_t r = 0, g = 0, b = 0, a = 0;
            REQUIRE(shape->fill(&r, &g, &b, &a) == Result::Success);
            CHECK(r == tc.expR);
            CHECK(g == tc.expG);
            CHECK(b == tc.expB);
            CHECK(a == tc.expA);

            Paint::rel(picture);
        }

        // 2. SVG Data URI URL Decoding (+ for space, uppercase hex %3C, %3E, %2F)
        struct UrlTestCase {
            const char* desc;
            const char* href;
            bool valid;
        };

        static const UrlTestCase urlCases[] = {
            {"Empty Data URI", "data:image/svg+xml,", false},
            {"URL-encoded SVG with '+' for space", "data:image/svg+xml,%3Csvg+viewBox%3D%220%200%2010%2010%22%3E%3Crect+fill%3D%22%23F00%22+width%3D%2210%22+height%3D%2210%22%2F%3E%3C%2Fsvg%3E", true},
        };

        for (const auto& tc : urlCases) {
            CAPTURE(tc.desc);
            CAPTURE(tc.href);

            char svg[512];
            snprintf(svg, sizeof(svg),
                     "<svg viewBox=\"0 0 10 10\" xmlns=\"http://www.w3.org/2000/svg\">"
                     "<image id=\"target_img\" href=\"%s\" width=\"10\" height=\"10\"/>"
                     "</svg>",
                     tc.href);

            auto picture = Picture::gen();
            REQUIRE(picture);
            picture->accessible = true;
            REQUIRE(picture->load(svg, strlen(svg), "svg") == Result::Success);

            auto img = picture->paint(Accessor::id("target_img"));
            if (tc.valid) {
                CHECK(img != nullptr);
            }

            Paint::rel(picture);
        }
    }
    REQUIRE(Initializer::term() == Result::Success);
}

#endif

#ifdef THORVG_PNG_LOADER_SUPPORT

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

    Paint::rel(picture);
}

TEST_CASE("Load PNG file from data", "[tvgPicture]")
{
    auto picture = Picture::gen();
    REQUIRE(picture);

    //Open file
    ifstream file(TEST_DIR"/test.png", ios::in | ios::binary);
    REQUIRE(file.is_open());
    auto size = sizeof(uint32_t) * (1000*1000);
    auto data = (char*)malloc(size);
    file.read(data, size);
    file.close();

    REQUIRE(picture->load(data, size, "") == Result::Success);
    REQUIRE(picture->load(data, size, "png", "", true) == Result::Success);

    float w, h;
    REQUIRE(picture->size(&w, &h) == Result::Success);
    REQUIRE(w == 512);
    REQUIRE(h == 512);

    free(data);

    Paint::rel(picture);
}

TEST_CASE("Load PNG file and render", "[tvgPicture]")
{
    REQUIRE(Initializer::init() == Result::Success);
    {
        auto canvas = unique_ptr<SwCanvas>(SwCanvas::gen());
        REQUIRE(canvas);

        uint32_t buffer[100*100] = {};
        REQUIRE(canvas->target(buffer, 100, 100, 100, ColorSpace::ARGB8888) == Result::Success);

        auto picture = Picture::gen();
        REQUIRE(picture);

        REQUIRE(picture->load(TEST_DIR"/test.png") == Result::Success);
        REQUIRE(picture->opacity(192) == Result::Success);
        REQUIRE(picture->scale(5.0) == Result::Success);

        REQUIRE(canvas->add(picture) == Result::Success);
    }
    REQUIRE(Initializer::term() == Result::Success);
}

#endif

#ifdef THORVG_JPG_LOADER_SUPPORT

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

    Paint::rel(picture);
}

TEST_CASE("Load JPG file from data", "[tvgPicture]")
{
    auto picture = Picture::gen();
    REQUIRE(picture);

    //Open file
    ifstream file(TEST_DIR"/test.jpg", ios::in | ios::binary);
    REQUIRE(file.is_open());
    auto begin = file.tellg();
    file.seekg(0, ios::end);
    auto size = file.tellg() - begin;
    auto data = (char*)malloc(size);
    file.seekg(0, ios::beg);
    file.read(data, size);
    file.close();

    REQUIRE(picture->load(data, size, "") == Result::Success);
    REQUIRE(picture->load(data, size, "jpg", "", true) == Result::Success);

    float w, h;
    REQUIRE(picture->size(&w, &h) == Result::Success);
    REQUIRE(w == 512);
    REQUIRE(h == 512);

    free(data);

    Paint::rel(picture);
}

TEST_CASE("Load JPG file and render", "[tvgPicture]")
{
    REQUIRE(Initializer::init() == Result::Success);
    {

        auto canvas = unique_ptr<SwCanvas>(SwCanvas::gen());
        REQUIRE(canvas);

        uint32_t buffer[100*100] = {};
        REQUIRE(canvas->target(buffer, 100, 100, 100, ColorSpace::ARGB8888) == Result::Success);

        auto picture = Picture::gen();
        REQUIRE(picture);

        REQUIRE(picture->load(TEST_DIR"/test.jpg") == Result::Success);

        REQUIRE(canvas->add(picture) == Result::Success);
    }
    REQUIRE(Initializer::term() == Result::Success);
}

#endif

#ifdef THORVG_WEBP_LOADER_SUPPORT

TEST_CASE("Load WEBP file from path", "[tvgPicture]")
{
    auto picture = Picture::gen();
    REQUIRE(picture);

    //Invalid file
    REQUIRE(picture->load("invalid.webp") == Result::InvalidArguments);

    REQUIRE(picture->load(TEST_DIR"/test.webp") == Result::Success);

    float w, h;
    REQUIRE(picture->size(&w, &h) == Result::Success);

    REQUIRE(w == 512);
    REQUIRE(h == 512);

    Paint::rel(picture);
}

TEST_CASE("Load WEBP file from data", "[tvgPicture]")
{
    auto picture = Picture::gen();
    REQUIRE(picture);

    //Open file
    ifstream file(TEST_DIR"/test.webp", ios::in | ios::binary);
    REQUIRE(file.is_open());
    auto size = sizeof(uint32_t) * (1000*1000);
    auto data = (char*)malloc(size);
    file.read(data, size);
    file.close();

    REQUIRE(picture->load(data, size, "") == Result::Success);
    REQUIRE(picture->load(data, size, "webp", "", true) == Result::Success);

    float w, h;
    REQUIRE(picture->size(&w, &h) == Result::Success);
    REQUIRE(w == 512);
    REQUIRE(h == 512);

    free(data);

    Paint::rel(picture);
}

TEST_CASE("Load WEBP file and render", "[tvgPicture]")
{
    REQUIRE(Initializer::init() == Result::Success);
    {
        auto canvas = unique_ptr<SwCanvas>(SwCanvas::gen());
        REQUIRE(canvas);

        uint32_t buffer[100*100] = {};
        REQUIRE(canvas->target(buffer, 100, 100, 100, ColorSpace::ARGB8888) == Result::Success);

        auto picture = Picture::gen();
        REQUIRE(picture);

        REQUIRE(picture->load(TEST_DIR"/test.webp") == Result::Success);
        REQUIRE(picture->opacity(192) == Result::Success);
        REQUIRE(picture->scale(5.0) == Result::Success);

        REQUIRE(canvas->add(picture) == Result::Success);
    }
    REQUIRE(Initializer::term() == Result::Success);
}

TEST_CASE("Filter Method", "[tvgPicture]")
{
    auto picture = Picture::gen();
    REQUIRE(picture);

    REQUIRE(picture->filter(FilterMethod::Bilinear) == Result::Success);
    REQUIRE(picture->filter(FilterMethod::Nearest) == Result::Success);

    Paint::rel(picture);
}

#endif
