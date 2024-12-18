/*
 * Copyright (c) 2024 the ThorVG project. All rights reserved.

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

#ifndef _TVG_TRIM_H
#define _TVG_TRIM_H

struct Trim
{
    float start = 0.0f;
    float end = 1.0f;
    bool simultaneous = true;

    bool valid() const
    {
        if (start == 0.0f && end == 1.0f) return false;
        if (fabsf(end - start) >= 1.0f) return false;
        return true;
    }

    bool trim(float& start, float& end) const
    {
        start = this->start;
        end = this->end;

        if (fabsf(end - start) >= 1.0f) {
            start = 0.0f;
            end = 1.0f;
            return false;
        }

        auto loop = true;

        if (start > 1.0f && end > 1.0f) loop = false;
        if (start < 0.0f && end < 0.0f) loop = false;
        if (start >= 0.0f && start <= 1.0f && end >= 0.0f  && end <= 1.0f) loop = false;

        if (start > 1.0f) start -= 1.0f;
        if (start < 0.0f) start += 1.0f;
        if (end > 1.0f) end -= 1.0f;
        if (end < 0.0f) end += 1.0f;

        if ((loop && start < end) || (!loop && start > end)) std::swap(start, end);
        return true;
    }
};



#endif //_TVG_TRIM_H
