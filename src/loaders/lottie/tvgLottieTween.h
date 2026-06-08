/*
 * Copyright (c) 2026 the ThorVG project. All rights reserved.

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

#ifndef _TVG_LOTTIE_TWEEN_
#define _TVG_LOTTIE_TWEEN_

#include <unordered_map>
#include "tvgLottieCommon.h"

struct LottieTween
{
    union Value {
        float vFloat;
        int8_t vInteger;
        Point vScalar;
        Point3 vScalar3;
        RGB32 vColor;
        uint8_t vOpacity;
        ColorStop vColorStop;
        RenderPath vPath;

        Value() {}
        ~Value() {}
    };

    ~LottieTween()
    {
        data.clear();
    }

    // for dynamic tween
    std::unordered_map<uintptr_t, LottieTween::Value> data;
    bool enable = false;

    // for legacy
    float frameNo = 0.0f;
    float progress = 0.0f;  // [0 - 1]
    bool active = false;

    template<typename T>
    T capture(const void* key, const T& data)
    {
        if (enable) {
            Value value;
            toValue(data, value);
            assign(key, value);
        }
        return data;
    }

    void on(float to, float progress)
    {
        frameNo = to;
        this->progress = progress;
        active = true;
    }

    void on()
    {
        data.reserve(300);
        enable = true;
        active = true;
    }

    void off()
    {
        if (enable) {
            enable = false;
            data.clear();
        }
        active = false;
    }

private:
    void toValue(float v, Value& out) { out.vFloat = v; }
    void toValue(int8_t v, Value& out) { out.vInteger = v; }
    void toValue(const Point& v, Value& out) { out.vScalar = v; }
    void toValue(const Point3& v, Value& out) { out.vScalar3 = v; }
    void toValue(const RGB32& v, Value& out) { out.vColor = v; }
    void toValue(uint8_t v, Value& out) { out.vOpacity = v; }
    void toValue(const RenderPath& v, Value& out) { out.vPath = v; }

    void assign(const void* key, Value& value)
    {
        data[reinterpret_cast<uintptr_t>(key)] = value;
    }
};

#endif  //_TVG_LOTTIE_TWEEN_
