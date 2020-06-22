/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd All Rights Reserved
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *               http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */
#ifndef _TVG_BEZIER_CPP_
#define _TVG_BEZIER_CPP_

#include "tvgCommon.h"


/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

static float _lineLength(const Point& pt1, const Point& pt2)
{
    /* approximate sqrt(x*x + y*y) using alpha max plus beta min algorithm.
       With alpha = 1, beta = 3/8, giving results with the largest error less
       than 7% compared to the exact value. */
    Point diff = {pt2.x - pt1.x, pt2.y - pt1.y};
    if (diff.x < 0) diff.x = -diff.x;
    if (diff.y < 0) diff.y = -diff.y;
    return (diff.x > diff.y) ? (diff.x + diff.y * 0.375f) : (diff.y + diff.x * 0.375f);
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

namespace tvg
{

void bezSplit(const Bezier&cur, Bezier& left, Bezier& right)
{
    auto c = (cur.ctrl1.x + cur.ctrl2.x) * 0.5f;
    left.ctrl1.x = (cur.start.x + cur.ctrl1.x) * 0.5f;
    right.ctrl2.x = (cur.ctrl2.x + cur.end.x) * 0.5f;
    left.start.x = cur.start.x;
    right.end.x = cur.end.x;
    left.ctrl2.x = (left.ctrl1.x + c) * 0.5f;
    right.ctrl1.x = (right.ctrl2.x + c) * 0.5f;
    left.end.x = right.start.x = (left.ctrl2.x + right.ctrl1.x) * 0.5f;

    c = (cur.ctrl1.y + cur.ctrl2.y) * 0.5f;
    left.ctrl1.y = (cur.start.y + cur.ctrl1.y) * 0.5f;
    right.ctrl2.y = (cur.ctrl2.y + cur.end.y) * 0.5f;
    left.start.y = cur.start.y;
    right.end.y = cur.end.y;
    left.ctrl2.y = (left.ctrl1.y + c) * 0.5f;
    right.ctrl1.y = (right.ctrl2.y + c) * 0.5f;
    left.end.y = right.start.y = (left.ctrl2.y + right.ctrl1.y) * 0.5f;
}


float bezLength(const Bezier& cur)
{
    Bezier left, right;
    auto len = _lineLength(cur.start, cur.ctrl1) + _lineLength(cur.ctrl1, cur.ctrl2) + _lineLength(cur.ctrl2, cur.end);
    auto chord = _lineLength(cur.start, cur.end);

    if (fabs(len - chord) > FLT_EPSILON) {
        bezSplit(cur, left, right);
        return bezLength(left) + bezLength(right);
    }
    return len;
}


void bezSplitLeft(Bezier& cur, float at, Bezier& left)
{
    left.start = cur.start;

    left.ctrl1.x = cur.start.x + at * (cur.ctrl1.x - cur.start.x);
    left.ctrl1.y = cur.start.y + at * (cur.ctrl1.y - cur.start.y);

    left.ctrl2.x = cur.ctrl1.x + at * (cur.ctrl2.x - cur.ctrl1.x); // temporary holding spot
    left.ctrl2.y = cur.ctrl1.y + at * (cur.ctrl2.y - cur.ctrl1.y); // temporary holding spot

    cur.ctrl2.x = cur.ctrl2.x + at * (cur.end.x - cur.ctrl2.x);
    cur.ctrl2.y = cur.ctrl2.y + at * (cur.end.y - cur.ctrl2.y);

    cur.ctrl1.x = left.ctrl2.x + at * (cur.ctrl2.x - left.ctrl2.x);
    cur.ctrl1.y = left.ctrl2.y + at * (cur.ctrl2.y - left.ctrl2.y);

    left.ctrl2.x = left.ctrl1.x + at * (left.ctrl2.x - left.ctrl1.x);
    left.ctrl2.y = left.ctrl1.y + at * (left.ctrl2.y - left.ctrl1.y);

    left.end.x = cur.start.x = left.ctrl2.x + at * (cur.ctrl1.x - left.ctrl2.x);
    left.end.y = cur.start.y = left.ctrl2.y + at * (cur.ctrl1.y - left.ctrl2.y);
}


float bezAt(const Bezier& bz, float at)
{
    auto len = bezLength(bz);
    auto biggest = 1.0f;

    if (at >= len) return 1.0f;

    at *= 0.5f;

    while (true) {
        auto right = bz;
        Bezier left;
        bezSplitLeft(right, at, left);
        auto len2 = bezLength(left);

        if (fabs(len2 - len) < FLT_EPSILON) break;

        if (len2 < len) {
            at += (biggest - at) * 0.5f;
        } else {
            biggest = at;
            at -= (at * 0.5f);
        }
    }
    return at;
}


void bezSplitAt(const Bezier& cur, float at, Bezier& left, Bezier& right)
{
    right = cur;
    auto t = bezAt(right, at);
    bezSplitLeft(right, t, left);
}


}

#endif //_TVG_BEZIER_CPP_
