/*
 * Copyright (c) 2025 the ThorVG project. All rights reserved.

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

#include <thorvg_lottie.h>
#include <math.h>
#include "Example.h"

/************************************************************************/
/* ThorVG Drawing Contents                                              */
/************************************************************************/

struct UserExample : tvgexam::Example
{
    unique_ptr<tvg::LottieAnimation> lottie;

    tvg::Point down, prv;
    tvg::Point origin;
    float rotation = 0.0f;
    uint32_t time = 0;
    bool pressed = false;


    struct {
        uint32_t duration = 2000;  //5secs
        float target = 0.0f;
        uint32_t time = 0.0f;
        bool on = false;
    } effect;


    float calculate(tvg::Point& prv, tvg::Point& cur)
    {
        //degree with dot product
        auto degree = acos((prv.x * cur.x + prv.y * cur.y) / (sqrt(prv.x * prv.x + prv.y * prv.y) * sqrt(cur.x * cur.x + cur.y * cur.y)));
        degree *= 30.f;  //weight x30

        //direction with cross product
        auto dir = (prv.x * cur.y - (float)prv.y * (float)cur.x);
        if (dir < 0) degree *= -1.0f;

        return degree;
    }


    bool clickdown(tvg::Canvas* canvas, int32_t x, int32_t y) override
    {
        down = {float(x), float(y)};
        prv = {float(x) - origin.x, float(y) - origin.y};
        time = elapsed;
        pressed = true;
        effect.on = false;
        effect.target = rotation;
        return false;
    }

    bool clickup(tvg::Canvas* canvas, int32_t x, int32_t y) override
    {
        pressed = false;

        //flicking in 500ms
        if (elapsed - time > 500) return false;
        if (abs(down.x - x) < 10 && abs(down.y - y) < 10) return false;

        tvg::Point cur = {float(x) - origin.x, float(y) - origin.y};
        tvg::Point prv = {float(down.x) - origin.x, float(down.y) - origin.y};

        effect.target = rotation + calculate(prv, cur) * 20.0f;    //target to spinning effect
        effect.time = elapsed;
        effect.on = true;

        return false;
    }

    bool motion(tvg::Canvas* canvas, int32_t x, int32_t y) override
    {
        //update cursor
        tvgexam::verify(lottie->assign("FingerCursor", 3, "ct_xcoord", float(x)));
        tvgexam::verify(lottie->assign("FingerCursor", 3, "ct_ycoord", float(y)));

        if (!pressed) return false;

        tvg::Point cur = {float(x) - origin.x, float(y) - origin.y};

        rotation = fmodf(rotation + calculate(prv, cur), 360.0f);   //current rotation

        tvgexam::verify(lottie->assign("SpinBoard", 10, "ct_val", rotation));

        prv = cur;

        return true;
    }

    bool content(tvg::Canvas* canvas, uint32_t w, uint32_t h) override
    {
        //LottieAnimation Controller
        lottie = unique_ptr<tvg::LottieAnimation>(tvg::LottieAnimation::gen());
        auto picture = lottie->picture();

        //Background
        {
            auto shape = tvg::Shape::gen();
            shape->appendRect(0, 0, w, h);
            shape->fill(0, 0, 0);
            canvas->push(std::move(shape));
        }

        //Lottie Boundary
        {
            auto shape = tvg::Shape::gen();
            shape->appendRect(100, 100, w - 200, h - 200);
            shape->fill(50, 50, 50);
            canvas->push(std::move(shape));
        }

        if (!tvgexam::verify(picture->load(EXAMPLE_DIR"/lottie/extensions/spin.json"))) return false;

        //image scaling preserving its aspect ratio
        float scale;
        float shiftX = 0.0f, shiftY = 0.0f;
        float w2, h2;
        picture->size(&w2, &h2);

        if (w2 > h2) {
            scale = w / w2 * 0.8f;
            shiftX = w2 * 0.2f;
            shiftY = (h - h2 * scale) * 0.5f;
        } else {
            scale = h / h2 * 0.8f;
            shiftX = (w - w2 * scale) * 0.5f;
            shiftY = h2 * 0.2f;
        }

        picture->scale(scale);
        picture->translate(shiftX, shiftY);

        canvas->push(picture);

        origin.x = float(w / 2);
        origin.y = float(h / 2);

        return true;
    }

    bool update(tvg::Canvas* canvas, uint32_t elapsed) override
    {
        if (!canvas) return false;

        //spinning effect
        if (effect.on) {
            auto elapsedTime = elapsed - effect.time;
            auto progress = float(elapsedTime) / float(effect.duration);
            if (progress >= 1.0f) {
                progress = 1.0f;
                effect.on = false;
            }
            rotation = fmodf(effect.target * sin(progress), 360.0f);
            tvgexam::verify(lottie->assign("SpinBoard", 10, "ct_val", rotation));
        }

        auto progress = tvgexam::progress(elapsed, lottie->duration());

        //Update animation frame only when it's changed
        lottie->frame(lottie->totalFrame() * progress);
        canvas->update();

        return true;
    }
};


/************************************************************************/
/* Entry Point                                                          */
/************************************************************************/

int main(int argc, char **argv)
{
    return tvgexam::main(new UserExample, argc, argv, true, 1024, 1024, 0);
}