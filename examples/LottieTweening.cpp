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
#include "Example.h"

/************************************************************************/
/* ThorVG Drawing Contents                                              */
/************************************************************************/

struct UserExample : tvgexam::Example
{
    tvg::LottieAnimation* lottie;

    //designed the states: [angry, sad, mourn, wink, laughing]
    struct AnimState {
        std::string name;           //state name
        float begin;                //state begin frame number
    };

    std::vector<AnimState> states;  //states list
    int stateIdx = 0;               //current state index

    struct {
        float from;           //tweening from frame number
        float to;             //tweening to frame number
        float beginTime;      //tweening begin time
        bool active = false;  //whether on-tweening or not
    } tween;

    ~UserExample()
    {
        delete(lottie);
    }

    void init()
    {
        //get the AnimState info (state name and its begin frame number)
        for (uint32_t i = 0; i < lottie->markersCnt(); ++i) {
            //specify the current segment to retrieve the segment's starting frame.
            float begin;
            auto name = lottie->marker(i);
            lottie->segment(name);
            static_cast<tvg::Animation*>(lottie)->segment(&begin);

            //save the current AnimState to the state list
            states.push_back({string(name), begin});
        }

        //set the default state (Angry)
        lottie->segment(states[stateIdx].name.c_str());
    }

    //stateIdx is the next desired state
    void tweening(int stateIdx)
    {
        //don't allow the overlapped tweening
        if (tween.active || stateIdx == this->stateIdx) return;

        //reset the current state
        lottie->segment(nullptr);

        //tweening trigger time
        tween.beginTime = timestamp();

        //the current animation frame as the tweening "from" frame
        tween.from = lottie->curFrame();

        //the next state begin frame as the tweening "to" frame
        tween.to = states[stateIdx].begin;

        tween.active = true;

        this->stateIdx = stateIdx;

        cout << "tween to: " << states[stateIdx].name << endl;
    }

    bool clickdown(tvg::Canvas* canvas, int32_t x, int32_t y) override
    {
        auto intersect = [&](float x, float y, tvg::Point* obb) -> bool {
            //compute edge vectors
            tvg::Point e1 = {obb[1].x - obb[0].x, obb[1].y - obb[0].y};  // Edge from obb[0] to obb[1]
            tvg::Point e2 = {obb[3].x - obb[0].x, obb[3].y - obb[0].y};  // Edge from obb[0] to obb[3]

            //compute vectors from obb[0] to the test point
            tvg::Point o = {x - obb[0].x, y - obb[0].y};

            /* compute dot products to express `o` in terms of edge1 and edge2
               and then compute barycentric coordinates (u, v) within the box space */
            auto u = (o.x * e1.x + o.y * e1.y) / (e1.x * e1.x + e1.y * e1.y);
            auto v = (o.x * e2.x + o.y * e2.y) / (e2.x * e2.x + e2.y * e2.y);

            // Check if point is inside the OBB
            return (u >= 0.0f && u <= 1.0f && v >= 0.0f && v <= 1.0f);
        };

        int i = 0;
        for (auto& state : states) {
            if (auto paint = lottie->picture()->paint(tvg::Accessor::id(state.name.c_str()))) {
                tvg::Point obb[4];
                paint->bounds(obb);
                //hit a emoji layer!
                if (intersect(x, y, obb)) {
                    tweening(i);
                    return true;
                }
            }
            ++i;
        }
        return false;
    }

    bool content(tvg::Canvas* canvas, uint32_t w, uint32_t h) override
    {
        //Animation Controller
        lottie = tvg::LottieAnimation::gen();
        auto picture = lottie->picture();

        //Background
        auto shape = tvg::Shape::gen();
        shape->appendRect(0, 0, w, h);
        shape->fill(50, 50, 50);

        canvas->push(shape);

        if (!tvgexam::verify(picture->load(EXAMPLE_DIR"/lottie/emoji.json"))) return false;

        //image scaling preserving its aspect ratio
        float scale;
        float shiftX = 0.0f, shiftY = 0.0f;
        float w2, h2;
        picture->size(&w2, &h2);

        if (w2 > h2) {
            scale = w / w2;
            shiftY = (h - h2 * scale) * 0.5f;
        } else {
            scale = h / h2;
            shiftX = (w - w2 * scale) * 0.5f;
        }

        picture->scale(scale);
        picture->translate(shiftX, shiftY);

        canvas->push(picture);

        init();

        return true;
    }

    bool tweening(tvg::Canvas* canvas)
    {
        //perform tweening for 0.25 seconds.
        //in this sample, we use linear interpolation. You can vary the progress
        //with a specific interpolation style (e.g., sine, cosine, or spring curves).
        auto progress = (timestamp() - tween.beginTime) / 0.25f;

        //perform the tweening effect
        if (progress < 1.0f) {
            if (lottie->tween(tween.from, tween.to, progress) == tvg::Result(0)) {
                canvas->update();
                return true;
            }
        //tweening is over, set to the desired state
        } else {
            lottie->segment(states[stateIdx].name.c_str());
            tween.active = false;
            elapsed = 0;

            //tweening is over, start to the desired state play
            if (lottie->frame(0) == tvg::Result(0)) {
                canvas->update();
                return true;
            }
        }

        return false;
    }

    bool update(tvg::Canvas* canvas, uint32_t elapsed) override
    {
        //on state tweening
        if (tween.active) return tweening(canvas);

        //play the current state
        auto progress = tvgexam::progress(elapsed, lottie->duration());

        //Update animation frame only when it's changed
        if (lottie->frame(lottie->totalFrame() * progress) == tvg::Result(0)) {
            canvas->update();
            return true;
        }

        return false;
    }
};


/************************************************************************/
/* Entry Point                                                          */
/************************************************************************/

int main(int argc, char **argv)
{
    return tvgexam::main(new UserExample, argc, argv, false, 1024, 1024);
}