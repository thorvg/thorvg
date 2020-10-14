#include <thorvg.h>

#include <emscripten/bind.h>

using namespace emscripten;
using namespace std;
using namespace tvg;

string defaultData("<svg height=\"1000\" viewBox=\"0 0 1000 1000\" width=\"1000\" xmlns=\"http://www.w3.org/2000/svg\"><path d=\"M.10681413.09784845 1000.0527.01592069V1000.0851L.06005738 999.9983Z\" fill=\"#09bbf1\" stroke-width=\"3.910218\"/><g fill=\"#252f35\"><g stroke-width=\"3.864492\"><path d=\"M256.61221 100.51736H752.8963V386.99554H256.61221Z\"/><path d=\"M201.875 100.51736H238.366478V386.99554H201.875Z\"/><path d=\"M771.14203 100.51736H807.633508V386.99554H771.14203Z\"/></g><path d=\"M420.82388 380H588.68467V422.805317H420.82388Z\" stroke-width=\"3.227\"/><path d=\"m420.82403 440.7101v63.94623l167.86079 25.5782V440.7101Z\"/><path d=\"M420.82403 523.07258V673.47362L588.68482 612.59701V548.13942Z\"/></g><g fill=\"#222f35\"><path d=\"M420.82403 691.37851 588.68482 630.5019 589 834H421Z\"/><path d=\"m420.82403 852.52249h167.86079v28.64782H420.82403v-28.64782 0 0\"/><path d=\"m439.06977 879.17031c0 0-14.90282 8.49429-18.24574 15.8161-4.3792 9.59153 0 31.63185 0 31.63185h167.86079c0 0 4.3792-22.04032 0-31.63185-3.34292-7.32181-18.24574-15.8161-18.24574-15.8161z\"/></g><g fill=\"#09bbf1\"><path d=\"m280 140h15v55l8 10 8-10v-55h15v60l-23 25-23-25z\"/><path d=\"m335 140v80h45v-50h-25v10h10v30h-15v-57h18v-13z\"/></g></svg>");

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

    string getDefaultData()
    {
        return defaultData;
    }

    bool load(string data, int width, int height)
    {
        uint32_t pw, ph;
        float w, h;

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
        const char *cdata = data.c_str();
        if (mPicture->load(cdata, strlen(cdata)) != Result::Success) {

            /* mPicture is not handled as unique_ptr yet, so delete here */
            delete(mPicture);
            mPicture = nullptr;

            mErrorMsg = "Load failed";
            return false;
        }

        /* get default size */
        mPicture->viewbox(nullptr, nullptr, &w, &h);

        pw = mDefaultWidth;
        ph = mDefaultHeight;
        mDefaultWidth = static_cast<uint32_t>(w);
        mDefaultHeight = static_cast<uint32_t>(h);

        if (pw != mDefaultWidth || ph != mDefaultHeight) {
            updateScale();
        }

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

    float getScale()
    {
        return mScale;
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

        updateScale();
    }

    void updateScale()
    {
        if (!mPicture) return;

        float scaleX = static_cast<float>(mWidth) / static_cast<float>(mDefaultWidth);
        float scaleY = static_cast<float>(mHeight) / static_cast<float>(mDefaultHeight);
        mScale = scaleX < scaleY ? scaleX : scaleY;

        mPicture->scale(mScale);
    }

private:
    string                 mErrorMsg;
    unique_ptr< SwCanvas > mSwCanvas = nullptr;
    Picture*               mPicture = nullptr;
    unique_ptr<uint8_t[]>  mBuffer = nullptr;

    uint32_t               mWidth{0};
    uint32_t               mHeight{0};
    uint32_t               mDefaultWidth{0};
    uint32_t               mDefaultHeight{0};
    float                  mScale{0};
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
    .function("getScale", &ThorvgWasm::getScale);
}
