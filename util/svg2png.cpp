#include <iostream>
#include <thread>
#include <thorvg.h>
#include <vector>
#include "lodepng.h"

using namespace std;


class PngBuilder {
public:
    ~PngBuilder()
    {
    }
    void build(const std::string &fileName , const uint32_t width, const uint32_t height, uint32_t *buffer)
    {
        std::vector<unsigned char> image;
        image.resize(width * height * 4);
        for(unsigned y = 0; y < height; y++)
        {
            for(unsigned x = 0; x < width; x++)
            {
                uint32_t n = buffer[ y * width + x ];
                image[4 * width * y + 4 * x + 0] = ( n >> 16 ) & 0xff;
                image[4 * width * y + 4 * x + 1] = ( n >> 8 ) & 0xff;
                image[4 * width * y + 4 * x + 2] = n & 0xff;
                image[4 * width * y + 4 * x + 3] = ( n >> 24 ) & 0xff;
            }
        }
        unsigned error = lodepng::encode(fileName, image, width, height);

        //if there's an error, display it
        if(error) std::cout << "encoder error " << error << ": "<< lodepng_error_text(error) << std::endl;
    }
};


class App {
public:
    void tvgDrawCmds(tvg::Canvas* canvas, bool useSvgSize)
    {
        if (!canvas) return;

        auto picture = tvg::Picture::gen();
        float fw, fh;
        if (picture->load(fileName) != tvg::Result::Success) return;

        picture->viewbox(nullptr, nullptr, &fw, &fh);

        if (useSvgSize)
        {
            width = fw;
            height = fh;
        }
        else
        {
            tvg::Matrix m = {width / fw, 0, 0, 0, height / fh, 0, 0, 0, 1};
            picture->transform(m);
        }

        if (bgColor != 0xffffffff)
        {
            uint8_t bgColorR = (uint8_t) ((bgColor & 0xff0000) >> 16);
            uint8_t bgColorG = (uint8_t) ((bgColor & 0x00ff00) >> 8);
            uint8_t bgColorB = (uint8_t) ((bgColor & 0x0000ff));

            //Background
            auto shape = tvg::Shape::gen();
            shape->appendRect(0, 0, width, height, 0, 0);    //x, y, w, h, rx, ry
            shape->fill(bgColorR, bgColorG, bgColorB, 255);                 //r, g, b, a

            if (canvas->push(move(shape)) != tvg::Result::Success) return;
        }

        /* This showcase shows you asynchrounous loading of svg.
           For this, pushing pictures at a certian sync time.
           This means it earns the time to finish loading svg resources,
           otherwise you can push pictures immediately. */
        canvas->push(move(picture));
    }

    int tvgRender(int w, int h)
    {
        tvg::CanvasEngine tvgEngine = tvg::CanvasEngine::Sw;

        //Threads Count
        auto threads = std::thread::hardware_concurrency();

        //Initialize ThorVG Engine
        if (tvg::Initializer::init(tvgEngine, threads) == tvg::Result::Success) {

            //Create a Canvas
            unique_ptr<tvg::SwCanvas> swCanvas = tvg::SwCanvas::gen();
            bool useSvgSize = false;
            if (w == 0 && h == 0)
            {
                w = h = 200; // Set temporary size
                useSvgSize = true;
            }
            else
            {
                width = w;
                height = h;
            }

            uint32_t *buffer = (uint32_t*)malloc(sizeof(uint32_t) * w * h);
            swCanvas->target(buffer, w, w, h, tvg::SwCanvas::ARGB8888);
            /* Push the shape into the Canvas drawing list
               When this shape is into the canvas list, the shape could update & prepare
               internal data asynchronously for coming rendering.
               Canvas keeps this shape node unless user call canvas->clear() */
            tvgDrawCmds(swCanvas.get(), useSvgSize);

            if (useSvgSize)
            {
                //Resize target buffer
                buffer = (uint32_t*)realloc(buffer, sizeof(uint32_t) * width * height);
                swCanvas->target(buffer, width, width, height, tvg::SwCanvas::ARGB8888);
            }


            if (swCanvas->draw() == tvg::Result::Success) {
                swCanvas->sync();
            }

            PngBuilder builder;
            builder.build(pngName.data(), width, height, buffer);

            //Terminate ThorVG Engine
            tvg::Initializer::term(tvg::CanvasEngine::Sw);

            free(buffer);

        } else {
            cout << "engine is not supported" << endl;
        }

        return result();
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
        if ( fileName.size() <= extn.size() ||
             fileName.substr(fileName.size()- extn.size()) != extn )
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

int
main(int argc, char **argv)
{
    App app;
    size_t w, h;

    if (app.setup(argc, argv, &w, &h)) return 1;

    app.tvgRender(w, h);

    return 0;
}
