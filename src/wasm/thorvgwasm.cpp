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

#include <thorvg.h>
#include "tvgIteratorModule.h"

#include <emscripten/bind.h>

using namespace emscripten;
using namespace std;
using namespace tvg;

string defaultData("<svg height=\"1000\" viewBox=\"0 0 1000 1000\" width=\"1000\" xmlns=\"http://www.w3.org/2000/svg\"><path d=\"M.10681413.09784845 1000.0527.01592069V1000.0851L.06005738 999.9983Z\" fill=\"#09bbf1\" stroke-width=\"3.910218\"/><g fill=\"#252f35\"><g stroke-width=\"3.864492\"><path d=\"M256.61221 100.51736H752.8963V386.99554H256.61221Z\"/><path d=\"M201.875 100.51736H238.366478V386.99554H201.875Z\"/><path d=\"M771.14203 100.51736H807.633508V386.99554H771.14203Z\"/></g><path d=\"M420.82388 380H588.68467V422.805317H420.82388Z\" stroke-width=\"3.227\"/><path d=\"m420.82403 440.7101v63.94623l167.86079 25.5782V440.7101Z\"/><path d=\"M420.82403 523.07258V673.47362L588.68482 612.59701V548.13942Z\"/></g><g fill=\"#222f35\"><path d=\"M420.82403 691.37851 588.68482 630.5019 589 834H421Z\"/><path d=\"m420.82403 852.52249h167.86079v28.64782H420.82403v-28.64782 0 0\"/><path d=\"m439.06977 879.17031c0 0-14.90282 8.49429-18.24574 15.8161-4.3792 9.59153 0 31.63185 0 31.63185h167.86079c0 0 4.3792-22.04032 0-31.63185-3.34292-7.32181-18.24574-15.8161-18.24574-15.8161z\"/></g><g fill=\"#09bbf1\"><path d=\"m280 140h15v55l8 10 8-10v-55h15v60l-23 25-23-25z\"/><path d=\"m335 140v80h45v-50h-25v10h10v30h-15v-57h18v-13z\"/></g></svg>");

class __attribute__((visibility("default"))) ThorvgWasm : public IteratorModule
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

    string getDefaultData()
    {
        return defaultData;
    }

    bool load(string data, int width, int height)
    {
        mErrorMsg = "None";

        if (!mSwCanvas) {
            mErrorMsg = "Canvas is NULL";
            return false;
        }

        mPicture = Picture::gen().release();
        if (!mPicture) {
            mErrorMsg = "Picture get failed";
            return false;
        }

        mSwCanvas->clear();

        if (data.empty()) data = defaultData;
        if (mPicture->load(data.c_str(), data.size()) != Result::Success) {
            /* mPicture is not handled as unique_ptr yet, so delete here */
            delete(mPicture);
            mPicture = nullptr;

            mErrorMsg = "Load failed";
            return false;
        }

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

    void update(int width, int height)
    {
        mErrorMsg = "None";

        if (!mSwCanvas) {
            mErrorMsg = "Canvas is NULL";
            return;
        }

        if (!mPicture) {
            mErrorMsg = "Picture is NULL";
            return;
        }

        if (mWidth == width && mHeight == height) {
            return;
        }

        updateSize(width, height);

        if (mSwCanvas->update(mPicture) != Result::Success) {
            mErrorMsg = "Update failed";
            return;
        }

        return;
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

        return val(typed_memory_view(mWidth * mHeight * 4, mBuffer.get()));
    }

    bool saveTvg()
    {
        mErrorMsg = "None";

        auto saver = tvg::Saver::gen();
        auto duplicate = unique_ptr<tvg::Picture>(static_cast<tvg::Picture*>(mPicture->duplicate()));
        if (!saver || !duplicate) {
            mErrorMsg = "Saving initialization failed";
            return false;
        }
        if (saver->save(move(duplicate), "file.tvg") != tvg::Result::Success) {
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
        paint->bounds(&mBounds[0], &mBounds[1], &mBounds[2], &mBounds[3]);

        mBounds[2] += mBounds[0];
        mBounds[3] += mBounds[1];

        for (auto paint = parents.data; paint < (parents.data + parents.count); ++paint) {
           auto m = const_cast<Paint*>(*paint)->transform();
           mBounds[0] = mBounds[0] * m.e11 + m.e13;
           mBounds[1] = mBounds[1] * m.e22 + m.e23;
           mBounds[2] = mBounds[2] * m.e11 + m.e13;
           mBounds[3] = mBounds[3] * m.e22 + m.e23;
        }

        mBounds[2] -= mBounds[0];
        mBounds[3] -= mBounds[1];

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

    void updateSize(int width, int height)
    {
        if (!mSwCanvas) return;
        if (mWidth == width && mHeight == height) return;

        mWidth = width;
        mHeight = height;
        mBuffer = make_unique<uint8_t[]>(mWidth * mHeight * 4);
        mSwCanvas->target((uint32_t *)mBuffer.get(), mWidth, mWidth, mHeight, SwCanvas::ABGR8888);

        if (mPicture) mPicture->size(width, height);
    }

    struct Layer
    {
        uint32_t paint; //cast of a paint pointer
        uint32_t depth;
        uint32_t type;
        uint32_t composite;
    };
    void sublayers(Array<Layer>* layers, const Paint* paint, uint32_t depth)
    {
        //paint
        if (paint->id() != TVG_CLASS_ID_SHAPE) {
            auto it = this->iterator(paint);
            if (it->count() > 0) {
                layers->reserve(layers->count + it->count());
                it->begin();
                while (auto child = it->next()) {
                    uint32_t type = child->id();
                    layers->push({.paint = reinterpret_cast<uint32_t>(child), .depth = depth + 1, .type = type, .composite = static_cast<uint32_t>(CompositeMethod::None)});
                    sublayers(layers, child, depth + 1);
                }
            }
        }
        //composite
        const Paint* compositeTarget = nullptr;
        CompositeMethod composite = paint->composite(&compositeTarget);
        if (compositeTarget && composite != CompositeMethod::None) {
            uint32_t type = compositeTarget->id();
            layers->push({.paint = reinterpret_cast<uint32_t>(compositeTarget), .depth = depth, .type = type, .composite = static_cast<uint32_t>(composite)});
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
        if (parent->id() != TVG_CLASS_ID_SHAPE) {
            auto it = this->iterator(parent);
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
};

// Binding code
EMSCRIPTEN_BINDINGS(thorvg_bindings) {
  class_<ThorvgWasm>("ThorvgWasm")
    .constructor(&ThorvgWasm::create)
    .function("getError", &ThorvgWasm::getError, allow_raw_pointers())
    .function("getDefaultData", &ThorvgWasm::getDefaultData, allow_raw_pointers())
    .function("load", &ThorvgWasm::load)
    .function("update", &ThorvgWasm::update)
    .function("render", &ThorvgWasm::render)

    .function("saveTvg", &ThorvgWasm::saveTvg)

    .function("layers", &ThorvgWasm::layers)
    .function("bounds", &ThorvgWasm::bounds)
    .function("setOpacity", &ThorvgWasm::setOpacity);
}
