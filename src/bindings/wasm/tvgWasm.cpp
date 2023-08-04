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
#include "tvgIteratorAccessor.h"

#include <emscripten/bind.h>

using namespace emscripten;
using namespace std;
using namespace tvg;

class __attribute__((visibility("default"))) ThorvgWasm
{
public:
    static unique_ptr<ThorvgWasm> create()
    {
        return unique_ptr<ThorvgWasm>(new ThorvgWasm());
    }

    string getError()
    {
        return mErrorMsg;
    }

    bool load(string data, string mimetype, int width, int height)
    {
        mErrorMsg = "None";

        if (!mSwCanvas) {
             mErrorMsg = "Canvas is NULL";
             return false;
        }

        if (data.empty()) {
            mErrorMsg = "Data is empty";
            return false;
        }

        mPicture = Picture::gen().release();
        if (!mPicture) {
            mErrorMsg = "Picture get failed";
            return false;
        }

        mSwCanvas->clear();

        if (mPicture->load(data.c_str(), data.size(), mimetype, false) != Result::Success) {
            /* mPicture is not handled as unique_ptr yet, so delete here */
            delete(mPicture);
            mPicture = nullptr;

            mErrorMsg = "Load failed";
            return false;
        }

        mPicture->size(&mOriginalSize[0], &mOriginalSize[1]);

        /* need to reset size to calculate scale in Picture.size internally
           before calling updateSize */
        mWidth = 0;
        mHeight = 0;

        updateSize(width, height);

        if (mSwCanvas->push(unique_ptr<Picture>(mPicture)) != Result::Success) {
            mErrorMsg = "Push failed";
            return false;
        }

        return true;
    }

    bool update(int width, int height, bool force)
    {
        mErrorMsg = "None";

        if (!mSwCanvas || !mPicture) {
            mErrorMsg = "Invalid Conditions";
            return false;
        }

        if (!force && mWidth == width && mHeight == height) {
            return true;
        }

        updateSize(width, height);

        if (mSwCanvas->update(mPicture) != Result::Success) {
            mErrorMsg = "Update failed";
            return false;
        }
        return true;
    }

    val render()
    {
        mErrorMsg = "None";

        if (!mSwCanvas) {
            mErrorMsg = "Canvas is NULL";
            return val(typed_memory_view<uint8_t>(0, nullptr));
        }

        if (mSwCanvas->draw() != Result::Success) {
            mErrorMsg = "Draw failed";
            return val(typed_memory_view<uint8_t>(0, nullptr));
        }

        mSwCanvas->sync();

        //Change ARGB into ABGR and unpremultiply
        unpremultiplyBuffer();

        return val(typed_memory_view(mWidth * mHeight * 4, mBuffer.get()));
    }

    val originalSize()
    {
        return val(typed_memory_view(2, mOriginalSize));
    }

    bool saveTvg(bool compress)
    {
        mErrorMsg = "None";

        auto saver = tvg::Saver::gen();
        auto duplicate = tvg::cast<tvg::Picture>(mPicture->duplicate());
        if (!saver || !duplicate) {
            mErrorMsg = "Saving initialization failed";
            return false;
        }
        if (saver->save(std::move(duplicate), "file.tvg", compress) != tvg::Result::Success) {
            mErrorMsg = "Tvg saving failed";
            return false;
        }
        saver->sync();

        return true;
    }

    val layers()
    {
        //returns an array of a structure Layer: [id] [depth] [type] [composite]
        mLayers.reset();
        sublayers(&mLayers, mPicture, 0);

        return val(typed_memory_view(mLayers.count * sizeof(Layer) / sizeof(uint32_t), (uint32_t *)(mLayers.data)));
    }

    bool setOpacity(uint32_t paintId, uint8_t opacity)
    {
        const Paint* paint = findPaintById(mPicture, paintId, nullptr);
        if (!paint) return false;
        const_cast<Paint*>(paint)->opacity(opacity);
        return true;
    }

    val bounds(uint32_t paintId)
    {
        Array<const Paint *> parents;
        const Paint* paint = findPaintById(mPicture, paintId, &parents);
        if (!paint) return val(typed_memory_view<float>(0, nullptr));
        paint->bounds(&mBounds[0], &mBounds[1], &mBounds[2], &mBounds[3], false);

        float points[8] = { //clockwise points
            mBounds[0], mBounds[1], //(x1, y1)
            mBounds[0] + mBounds[2], mBounds[1], //(x2, y1)
            mBounds[0] + mBounds[2], mBounds[1] + mBounds[3], //(x2, y2)
            mBounds[0], mBounds[1] + mBounds[3], //(x1, y2)
        };

        for (auto paint = parents.data; paint < parents.end(); ++paint) {
            auto m = const_cast<Paint*>(*paint)->transform();
            for (int i = 0; i<8; i += 2) {
                float x = points[i] * m.e11 + points[i+1] * m.e12 + m.e13;
                points[i+1] = points[i] * m.e21 + points[i+1] * m.e22 + m.e23;
                points[i] = x;
            }
        }

        mBounds[0] = points[0];//x(p1)
        mBounds[1] = points[3];//y(p2)
        mBounds[2] = points[4] - mBounds[0];//x(p3)
        mBounds[3] = points[7] - mBounds[1];//y(p4)

        return val(typed_memory_view(4, mBounds));
    }

private:
    explicit ThorvgWasm()
    {
        mErrorMsg = "None";

        Initializer::init(CanvasEngine::Sw, 0);
        mSwCanvas = SwCanvas::gen();
        if (!mSwCanvas) {
            mErrorMsg = "Canvas get failed";
            return;
        }
    }

    void unpremultiplyBuffer() {
        for (uint32_t y = 0; y < mHeight; y++) {
            auto buffer = (uint32_t *) mBuffer.get() + mWidth * y;
            for (uint32_t x = 0; x < mWidth; ++x) {
                uint8_t a = buffer[x] >> 24;
                if (a == 255) {
                    uint8_t b = (buffer[x] & 0xff);
                    uint8_t r = ((buffer[x] >> 16) & 0xff);
                    buffer[x] = (buffer[x] & 0xff00ff00) | (b << 16) | (r);

                } else if (a == 0) {
                    buffer[x] = 0x00ffffff;

                } else {
                    uint16_t b = ((buffer[x] << 8) & 0xff00) / a;
                    uint16_t g = ((buffer[x]) & 0xff00) / a;
                    uint16_t r = ((buffer[x] >> 8) & 0xff00) / a;
                    if (b > 0xff) b = 0xff;
                    if (g > 0xff) g = 0xff;
                    if (r > 0xff) r = 0xff;
                    buffer[x] = (a << 24) | (b << 16) | (g << 8) | (r);
                }
            }
        }
    }

    void updateSize(int width, int height)
    {
        if (!mSwCanvas) return;
        if (mWidth == width && mHeight == height) return;

        mWidth = width;
        mHeight = height;
        mBuffer = make_unique<uint8_t[]>(mWidth * mHeight * 4);
        mSwCanvas->target((uint32_t *)mBuffer.get(), mWidth, mWidth, mHeight, SwCanvas::ARGB8888);

        if (mPicture) {
            float scale;
            float shiftX = 0.0f, shiftY = 0.0f;
            if (mOriginalSize[0] > mOriginalSize[1]) {
                scale = width / mOriginalSize[0];
                shiftY = (height - mOriginalSize[1] * scale) * 0.5f;
            } else {
                scale = height / mOriginalSize[1];
                shiftX = (width - mOriginalSize[0] * scale) * 0.5f;
            }
            mPicture->scale(scale);
            mPicture->translate(shiftX, shiftY);
        }
    }

    struct Layer
    {
        uint32_t paint; //cast of a paint pointer
        uint32_t depth;
        uint32_t type;
        uint32_t composite;
        uint32_t opacity;
    };
    void sublayers(Array<Layer>* layers, const Paint* paint, uint32_t depth)
    {
        //paint
        if (paint->identifier() != Shape::identifier()) {
            auto it = IteratorAccessor::iterator(paint);
            if (it->count() > 0) {
                layers->reserve(layers->count + it->count());
                it->begin();
                while (auto child = it->next()) {
                    uint32_t type = child->identifier();
                    uint32_t opacity = child->opacity();
                    layers->push({.paint = reinterpret_cast<uint32_t>(child), .depth = depth + 1, .type = type, .composite = static_cast<uint32_t>(CompositeMethod::None), .opacity = opacity});
                    sublayers(layers, child, depth + 1);
                }
            }
        }
        //composite
        const Paint* compositeTarget = nullptr;
        CompositeMethod composite = paint->composite(&compositeTarget);
        if (compositeTarget && composite != CompositeMethod::None) {
            uint32_t type = compositeTarget->identifier();
            uint32_t opacity = compositeTarget->opacity();
            layers->push({.paint = reinterpret_cast<uint32_t>(compositeTarget), .depth = depth, .type = type, .composite = static_cast<uint32_t>(composite), .opacity = opacity});
            sublayers(layers, compositeTarget, depth);
        }
    }

    const Paint* findPaintById(const Paint* parent, uint32_t paintId, Array<const Paint *>* parents) {
        //validate paintId is correct and exists in the picture
        if (reinterpret_cast<uint32_t>(parent) == paintId) {
            if (parents) parents->push(parent);
            return parent;
        }
        //paint
        if (parent->identifier() != Shape::identifier()) {
            auto it = IteratorAccessor::iterator(parent);
            if (it->count() > 0) {
                it->begin();
                while (auto child = it->next()) {
                    if (auto paint = findPaintById(child, paintId, parents)) {
                        if (parents) parents->push(parent);
                        return paint;
                    }
                }
            }
        }
        //composite
        const Paint* compositeTarget = nullptr;
        CompositeMethod composite = parent->composite(&compositeTarget);
        if (compositeTarget && composite != CompositeMethod::None) {
            if (auto paint = findPaintById(compositeTarget, paintId, parents)) {
                if (parents) parents->push(parent);
                return paint;
            }
        }
        return nullptr;
    }

private:
    string                 mErrorMsg;
    unique_ptr< SwCanvas > mSwCanvas = nullptr;
    Picture*               mPicture = nullptr;
    unique_ptr<uint8_t[]>  mBuffer = nullptr;

    uint32_t               mWidth{0};
    uint32_t               mHeight{0};

    Array<Layer>           mLayers;
    float                  mBounds[4];
    float                  mOriginalSize[2];
};

//Binding code
EMSCRIPTEN_BINDINGS(thorvg_bindings) {
  class_<ThorvgWasm>("ThorvgWasm")
    .constructor(&ThorvgWasm::create)
    .function("getError", &ThorvgWasm::getError, allow_raw_pointers())
    .function("load", &ThorvgWasm::load)
    .function("update", &ThorvgWasm::update)
    .function("render", &ThorvgWasm::render)
    .function("originalSize", &ThorvgWasm::originalSize)

    .function("saveTvg", &ThorvgWasm::saveTvg)

    .function("layers", &ThorvgWasm::layers)
    .function("bounds", &ThorvgWasm::bounds)
    .function("setOpacity", &ThorvgWasm::setOpacity);
}
