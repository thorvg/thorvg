/*
 * Copyright (c) 2020 - 2023 the ThorVG project. All rights reserved.

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
#include <emscripten/bind.h>
#include "tvgIteratorAccessor.h"


using namespace emscripten;
using namespace std;
using namespace tvg;

static const char* NoError = "None";

class __attribute__((visibility("default"))) TvgWasm
{
    //This structure data should be aligned with the ThorVG Viewer implementation.
    struct Layer
    {
        uint32_t paint;         //cast of a paint pointer
        uint32_t depth;
        uint32_t type;
        uint8_t opacity;
        CompositeMethod method;
    };

public:
    ~TvgWasm()
    {
        free(buffer);
        Initializer::term(CanvasEngine::Sw);
    }

    static unique_ptr<TvgWasm> create()
    {
        return unique_ptr<TvgWasm>(new TvgWasm());
    }

    string error()
    {
        return errorMsg;
    }

    bool load(string data, string mimetype, int width, int height)
    {
        errorMsg = NoError;

        if (!canvas) return false;

        if (data.empty()) {
            errorMsg = "Invalid data";
            return false;
        }

        canvas->clear(true);

        animation = Animation::gen();

        if (animation->picture()->load(data.c_str(), data.size(), mimetype, false) != Result::Success) {
            errorMsg = "load() fail";
            return false;
        }

        animation->picture()->size(&psize[0], &psize[1]);

        /* need to reset size to calculate scale in Picture.size internally before calling resize() */
        this->width = 0;
        this->height = 0;

        resize(width, height);

        if (canvas->push(cast(animation->picture())) != Result::Success) {
            errorMsg = "push() fail";
            return false;
        }

        updated = true;

        return true;
    }

    bool update()
    {
        if (!updated) return true;

        errorMsg = NoError;

        if (canvas->update() != Result::Success) {
            errorMsg = "update() fail";
            return false;
        }

        return true;
    }

    val render()
    {
        errorMsg = NoError;

        if (!canvas || !animation) return val(typed_memory_view<uint8_t>(0, nullptr));

        if (!updated) return val(typed_memory_view(width * height * 4, buffer));

        if (canvas->draw() != Result::Success) {
            errorMsg = "draw() fail";
            return val(typed_memory_view<uint8_t>(0, nullptr));
        }

        canvas->sync();

        updated = false;

        return val(typed_memory_view(width * height * 4, buffer));
    }

    val size()
    {
        return val(typed_memory_view(2, psize));
    }

    val duration()
    {
        if (!canvas || !animation) return val(0);
        return val(animation->duration());
    }

    val totalFrame()
    {
        if (!canvas || !animation) return val(0);
        return val(animation->totalFrame());
    }

    bool frame(float no)
    {
        if (!canvas || !animation) return false;
        if (animation->frame(no) == Result::Success) {
            updated = true;
        }
        return true;
    }

    void resize(int width, int height)
    {
        if (!canvas || !animation) return;
        if (this->width == width && this->height == height) return;

        this->width = width;
        this->height = height;

        free(buffer);
        buffer = (uint8_t*)malloc(width * height * sizeof(uint32_t));
        canvas->target((uint32_t *)buffer, width, width, height, SwCanvas::ABGR8888S);

        float scale;
        float shiftX = 0.0f, shiftY = 0.0f;
        if (psize[0] > psize[1]) {
            scale = width / psize[0];
            shiftY = (height - psize[1] * scale) * 0.5f;
        } else {
            scale = height / psize[1];
            shiftX = (width - psize[0] * scale) * 0.5f;
        }
        animation->picture()->scale(scale);
        animation->picture()->translate(shiftX, shiftY);

        updated = true;
    }

    bool save2Tvg()
    {
        errorMsg = NoError;

        if (!animation) return false;

        auto duplicate = cast<Picture>(animation->picture()->duplicate());

        if (!duplicate) {
            errorMsg = "duplicate(), fail";
            return false;
        }

        auto saver = Saver::gen();
        if (!saver) {
            errorMsg = "Invalid saver";
            return false;
        }

        if (saver->save(std::move(duplicate), "output.tvg") != tvg::Result::Success) {
            errorMsg = "save(), fail";
            return false;
        }

        saver->sync();

        return true;
    }

    bool save2Gif(string data, string mimetype, int width, int height, int fps)
    {
        errorMsg = NoError;

        auto animation = Animation::gen();

        if (!animation) {
            errorMsg = "Invalid animation";
            return false;
        }
        
        if (animation->picture()->load(data.c_str(), data.size(), mimetype, false) != Result::Success) {
            errorMsg = "load() fail";
            return false;
        }

        animation->picture()->size(width, height);

        auto saver = Saver::gen();
        if (!saver) {
            errorMsg = "Invalid saver";
            return false;
        }

        if (saver->save(std::move(animation), "output.gif", 100, fps) != tvg::Result::Success) {
            errorMsg = "save(), fail";
            return false;
        }

        saver->sync();

        return true;
    }

    val layers()
    {
        if (!canvas || !animation) return val(nullptr);

        //returns an array of a structure Layer: [id] [depth] [type] [composite]
        children.reset();
        sublayers(&children, animation->picture(), 0);

        return val(typed_memory_view(children.count * sizeof(Layer) / sizeof(uint32_t), (uint32_t *)(children.data)));
    }

    bool opacity(uint32_t id, uint8_t opacity)
    {
        if (!canvas || !animation) return false;

        auto paint = findPaintById(animation->picture(), id, nullptr);
        if (!paint) return false;
        const_cast<Paint*>(paint)->opacity(opacity);
        return true;
    }

    val geometry(uint32_t id)
    {
        if (!canvas || !animation) return val(typed_memory_view<float>(0, nullptr));

        Array<const Paint*> parents;
        auto paint = findPaintById(animation->picture(), id, &parents);
        if (!paint) return val(typed_memory_view<float>(0, nullptr));
        paint->bounds(&bounds[0], &bounds[1], &bounds[2], &bounds[3], false);

        float points[8] = { //clockwise points
            bounds[0], bounds[1], //(x1, y1)
            bounds[0] + bounds[2], bounds[1], //(x2, y1)
            bounds[0] + bounds[2], bounds[1] + bounds[3], //(x2, y2)
            bounds[0], bounds[1] + bounds[3], //(x1, y2)
        };

        for (auto paint = parents.data; paint < parents.end(); ++paint) {
            auto m = const_cast<Paint*>(*paint)->transform();
            for (int i = 0; i < 8; i += 2) {
                float x = points[i] * m.e11 + points[i+1] * m.e12 + m.e13;
                points[i] = x;
                points[i + 1] = points[i] * m.e21 + points[i + 1] * m.e22 + m.e23;
            }
        }

        bounds[0] = points[0]; //x(p1)
        bounds[1] = points[3]; //y(p2)
        bounds[2] = points[4] - bounds[0]; //x(p3)
        bounds[3] = points[7] - bounds[1]; //y(p4)

        return val(typed_memory_view(4, bounds));
    }

private:
    explicit TvgWasm()
    {
        errorMsg = NoError;

        if (Initializer::init(CanvasEngine::Sw, 0) != Result::Success) {
            errorMsg = "init() fail";
            return;
        }

        canvas = SwCanvas::gen();
        if (!canvas) errorMsg = "Invalid canvas";

        animation = Animation::gen();
        if (!animation) errorMsg = "Invalid animation";
    }

    void sublayers(Array<Layer>* layers, const Paint* paint, uint32_t depth)
    {
        //paint
        if (paint->identifier() != Shape::identifier()) {
            auto it = IteratorAccessor::iterator(paint);
            if (it->count() > 0) {
                it->begin();
                layers->grow(it->count());
                while (auto child = it->next()) {
                    uint32_t type = child->identifier();
                    layers->push({.paint = reinterpret_cast<uint32_t>(child), .depth = depth + 1, .type = type, .opacity = child->opacity(), .method = CompositeMethod::None});
                    sublayers(layers, child, depth + 1);
                }
            }
        }
        //composite
        const Paint* target = nullptr;
        CompositeMethod method = paint->composite(&target);
        if (target && method != CompositeMethod::None) {
            layers->push({.paint = reinterpret_cast<uint32_t>(target), .depth = depth, .type = target->identifier(), .opacity = target->opacity(), .method = method});
            sublayers(layers, target, depth);
        }
    }

    const Paint* findPaintById(const Paint* parent, uint32_t id, Array<const Paint *>* parents) {
        //validate paintId is correct and exists in the picture
        if (reinterpret_cast<uint32_t>(parent) == id) {
            if (parents) parents->push(parent);
            return parent;
        }
        //paint
        if (parent->identifier() != Shape::identifier()) {
            auto it = IteratorAccessor::iterator(parent);
            if (it->count() > 0) {
                it->begin();
                while (auto child = it->next()) {
                    if (auto paint = findPaintById(child, id, parents)) {
                        if (parents) parents->push(parent);
                        return paint;
                    }
                }
            }
        }
        //composite
        const Paint* target = nullptr;
        CompositeMethod method = parent->composite(&target);
        if (target && method != CompositeMethod::None) {
            if (auto paint = findPaintById(target, id, parents)) {
                if (parents) parents->push(parent);
                return paint;
            }
        }
        return nullptr;
    }

private:
    string                 errorMsg;
    unique_ptr<SwCanvas>   canvas = nullptr;
    unique_ptr<Animation>  animation = nullptr;
    uint8_t*               buffer = nullptr;
    Array<Layer>           children;
    uint32_t               width = 0;
    uint32_t               height = 0;
    float                  bounds[4];
    float                  psize[2];         //picture size
    bool                   updated = false;
};


EMSCRIPTEN_BINDINGS(thorvg_bindings) {
  class_<TvgWasm>("TvgWasm")
    .constructor(&TvgWasm::create)
    .function("error", &TvgWasm::error, allow_raw_pointers())
    .function("load", &TvgWasm::load)
    .function("update", &TvgWasm::update)
    .function("resize", &TvgWasm::resize)
    .function("render", &TvgWasm::render)
    .function("size", &TvgWasm::size)
    .function("duration", &TvgWasm::duration)
    .function("totalFrame", &TvgWasm::totalFrame)
    .function("frame", &TvgWasm::frame)
    .function("save2Tvg", &TvgWasm::save2Tvg)
    .function("save2Gif", &TvgWasm::save2Gif)
    .function("layers", &TvgWasm::layers)
    .function("geometry", &TvgWasm::geometry)
    .function("opacity", &TvgWasm::opacity);
}