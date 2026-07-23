/*
 * Copyright (c) 2026 ThorVG project. All rights reserved.

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

#ifndef _TVG_ANDROID_MEDIA_SCHEDULER_H_
#define _TVG_ANDROID_MEDIA_SCHEDULER_H_

#include <cstdint>

struct MediaTask
{
    MediaTask* next = nullptr;
    bool attached = false;

    virtual ~MediaTask() = default;
    virtual int64_t pumpFrame() = 0;   //µs until the next call, or -1 to park until wake()
};

struct AndroidMediaScheduler
{
    void attach(MediaTask* task);
    void detach(MediaTask* task);
    void wake();
};

AndroidMediaScheduler& androidMediaScheduler();

#endif  //_TVG_ANDROID_MEDIA_SCHEDULER_H_