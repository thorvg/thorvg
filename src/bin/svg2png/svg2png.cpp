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
#include <dirent.h>
#include <unistd.h>
#include <limits.h>

using namespace std;

#define FLAG_PARSE_DIRECTORY (1)
#define FLAG_PARSE_RECURSIVE (1 << 1)
#define FLAG_USE_ABGR8888_COLORSPACE (1 << 2)
#define FLAG_DO_NOT_SAVE_PNG (1 << 3)

struct PngBuilder
{
    void build(const string& fileName, const uint32_t width, const uint32_t height, uint32_t* buffer, bool altColorspace)
    {
        unsigned error = 0;
        if (!altColorspace) {
            //Used ARGB8888 so have to move pixels now
            vector<unsigned char> image;
            image.resize(width * height * 4);
            for (unsigned y = 0; y < height; y++) {
                for (unsigned x = 0; x < width; x++) {
                    uint32_t n = buffer[y * width + x];
                    image[4 * width * y + 4 * x + 0] = (n >> 16) & 0xff;
                    image[4 * width * y + 4 * x + 1] = (n >> 8) & 0xff;
                    image[4 * width * y + 4 * x + 2] = n & 0xff;
                    image[4 * width * y + 4 * x + 3] = (n >> 24) & 0xff;
                }
            }
            error = lodepng::encode(fileName, image, width, height);

        } else {
            //Used ABGR8888 so no need to move pixels
            error = lodepng::encode(fileName, (const unsigned char*) buffer, width, height);
        }

        //if there's an error, display it
        if (error) cout << "encoder error " << error << ": " << lodepng_error_text(error) << endl;
    }
};

struct Renderer
{
public:
    int render(const char* path, int w, int h, const string& dst, uint32_t bgColor, bool altColorspace, bool dontSavePng)
    {
        //Canvas
        if (!canvas) createCanvas();
        if (!canvas) {
            cout << "Error: Canvas failure" << endl;
            return 1;
        }

        //Picture
        auto picture = tvg::Picture::gen();
        if (picture->load(path) != tvg::Result::Success) {
            cout << "Error: Couldn't load image " << path << endl;
            return 1;
        }

        if (w == 0 || h == 0) {
            float fw, fh;
            picture->size(&fw, &fh);
            w = static_cast<uint32_t>(fw);
            h = static_cast<uint32_t>(fh);
        } else {
            picture->size(w, h);
        }

        //Buffer
        createBuffer(w, h);
        if (!buffer) {
            cout << "Error: Buffer failure" << endl;
            return 1;
        }

        if (canvas->target(buffer, w, w, h, altColorspace ? tvg::SwCanvas::ABGR8888 : tvg::SwCanvas::ARGB8888) != tvg::Result::Success) {
            cout << "Error: Canvas target failure" << endl;
            return 1;
        }

        //Background color if needed
        if (bgColor != 0xffffffff) {
            uint8_t r = (uint8_t)((bgColor & 0xff0000) >> 16);
            uint8_t g = (uint8_t)((bgColor & 0x00ff00) >> 8);
            uint8_t b = (uint8_t)((bgColor & 0x0000ff));

            auto shape = tvg::Shape::gen();
            shape->appendRect(0, 0, w, h, 0, 0);
            shape->fill(r, g, b, 255);

            if (canvas->push(move(shape)) != tvg::Result::Success) return 1;
        }

        //Drawing
        canvas->push(move(picture));
        canvas->draw();
        canvas->sync();

        //Build Png
        if (!dontSavePng) {
            PngBuilder builder;
            builder.build(dst, w, h, buffer, altColorspace);

            cout << "Generated PNG file: " << dst << endl;
        } else {
            cout << "Rendered: " << path << endl;
        }

        //Clear canvas
        canvas->clear(true);

        return 0;
    }

    void terminate()
    {
        //Terminate ThorVG Engine
        tvg::Initializer::term(tvg::CanvasEngine::Sw);

        //Free buffer
        free(buffer);
    }

private:
    void createCanvas()
    {
        //Canvas Engine
        tvg::CanvasEngine tvgEngine = tvg::CanvasEngine::Sw;

        //Threads Count
        auto threads = thread::hardware_concurrency();

        //Initialize ThorVG Engine
        if (tvg::Initializer::init(tvgEngine, threads) != tvg::Result::Success) {
            cout << "Error: Engine is not supported" << endl;
        }

        //Create a Canvas
        canvas = tvg::SwCanvas::gen();
    }

    void createBuffer(int w, int h)
    {
        uint32_t size = w * h;
        //Reuse old buffer if size is enough
        if (buffer && buffer_size >= size) return;

        //Alloc or realloc buffer
        buffer = (uint32_t*) realloc(buffer, sizeof(uint32_t) * size);
        buffer_size = size;
    }

private:
    unique_ptr<tvg::SwCanvas> canvas = nullptr;
    uint32_t* buffer = nullptr;
    uint32_t buffer_size = 0;
};

struct App
{
public:
    int setup(int argc, char** argv)
    {
        vector<const char*> paths;

        for (int i = 1; i < argc; i++) {
            const char* p = argv[i];
            if (*p == '-') {
                //flags
                const char* p_arg = (i + 1 < argc) ? argv[i] : nullptr;
                if (p[1] == 'r') {
                    //image resolution
                    if (!p_arg) {
                        cout << "Error: Missing resolution attribute. Expected eg. -r 200x200." << endl;
                        return 1;
                    }

                    const char* x = strchr(p_arg, 'x');
                    if (x) {
                        width = atoi(p_arg);
                        height = atoi(x + 1);
                    }
                    if (!x || width <= 0 || height <= 0) {
                        cout << "Error: Resolution (" << p_arg << ") is corrupted. Expected eg. -r 200x200." << endl;
                        return 1;
                    }

                } else if (p[1] == 'b') {
                    //image background color
                    if (!p_arg) {
                        cout << "Error: Missing background color attribute. Expected eg. -b #fa7410." << endl;
                        return 1;
                    }

                    if (*p_arg == '#') ++p_arg;
                    bgColor = (uint32_t) strtol(p, NULL, 16);

                } else if (p[1] == 'd') {
                    //directory parsing
                    flags |= FLAG_PARSE_DIRECTORY;
                    if (p[2] == 'r') flags |= FLAG_PARSE_RECURSIVE;

                } else if (p[1] == 'c') {
                    //use ABGR8888 colorspace
                    flags |= FLAG_USE_ABGR8888_COLORSPACE;

                } else if (p[1] == 's') {
                    //do not save png file. Useful for testing memory leaks
                    flags |= FLAG_DO_NOT_SAVE_PNG;

                } else {
                    cout << "Warning: Unknown flag (" << p << ")." << endl;
                }
            } else {
                //arguments
                paths.push_back(p);
            }
        }

        int ret = 0;
        if (flags & FLAG_PARSE_DIRECTORY) {
            const char * path = paths.empty()?nullptr:paths.front();
            ret = handleDirectoryBase(path);

        } else if (!paths.empty()) {
            for (auto path : paths) {
                if (!svgFile(path)) {
                    cout << "Warning: File \"" << path << "\" is not a proper svg file." << endl;
                    continue;
                }
                if ((ret = renderFile(path))) break;
            }

        } else {
            return help();
        }

        //Terminate renderer
        renderer.terminate();

        return ret;
    }

private:
    Renderer renderer;
    uint32_t bgColor = 0xffffffff;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t flags = 0;

private:
    int help()
    {
        cout << "Usage:\n   svg2png [svgFileName] [-r resolution] [-b bgColor] [flags]\n\nFlags:\n    -r set output image resolution.\n    -b set output image background color.\n    -d parse whole directory. If no directory specified, parse working directory.\n    -dr recursive. Same as -d, but include subdirectories.\n    -c Use ABGR8888 colorspace. Default is ARGB8888.\n    -s Don't save png file. Useful for testing memory leaks.\n\nExamples:\n    $ svg2png input.svg\n    $ svg2png input.svg -r 200x200\n    $ svg2png input.svg -r 200x200 -b ff00ff\n    $ svg2png input1.svg input2.svg -r 200x200 -b ff00ff\n    $ svg2png . -d\n    $ svg2png -dr\n    $ svg2png -d -c -s\n\n";
        return 1;
    }

    bool svgFile(const char* path)
    {
        size_t length = strlen(path);
        return length > 4 && (strcmp(&path[length - 4], ".svg") == 0);
    }

    int renderFile(const char* path)
    {
        //real path
        char full[PATH_MAX];
#ifdef _WIN32
        path = fullpath(full, path, PATH_MAX);
#else
        path = realpath(path, full);
#endif

        if (!path) return 1;

        //destination png file
        const char* dot = strrchr(path, '.');
        if (!dot) return 1;
        string dst(path, dot - path);
        dst += ".png";

        return renderer.render(path, width, height, dst, bgColor, (flags & FLAG_USE_ABGR8888_COLORSPACE), (flags & FLAG_DO_NOT_SAVE_PNG));
    }

    int handleDirectory(const string& path)
    {
        //open directory
        DIR* dir = opendir(path.c_str());
        if (!dir) {
            cout << "Couldn't open directory \"" << path.c_str() << "\"." << endl;
            return 1;
        }

        //list directory
        int ret = 0;
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            if (*entry->d_name == '.' || *entry->d_name == '$') continue;
            if (entry->d_type == DT_DIR) {
                if (!(flags & FLAG_PARSE_RECURSIVE)) continue;
                string subpath = string(path);
#ifdef _WIN32
                subpath += '\\';
#else
                subpath += '/';
#endif
                subpath += entry->d_name;
                ret = handleDirectory(subpath);
                if (ret) break;

            } else {
                if (!svgFile(entry->d_name)) continue;
                string fullpath = string(path);
#ifdef _WIN32
                fullpath += '\\';
#else
                fullpath += '/';
#endif
                fullpath += entry->d_name;
                ret = renderFile(fullpath.c_str());
                if (ret) break;
            }
        }
        closedir(dir);
        return ret;
    }

    int handleDirectoryBase(const char* path)
    {
        //create base path
        string basepath;
        if (path) {
            //copy path to basepath
            basepath = string(path);

        } else {
            //copy working directory into basepath
            char cwd[PATH_MAX];
            if (getcwd(cwd, sizeof(cwd)) != NULL) {
                basepath = string(cwd);
            } else {
                cout << "Error: Couldn't get working directory" << endl;
            }
        }

        return handleDirectory(basepath);
    }
};

int main(int argc, char** argv)
{
    App app;
    return app.setup(argc, argv);
}
