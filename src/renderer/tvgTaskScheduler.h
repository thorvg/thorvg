/*
 * Copyright (c) 2020 - 2026 ThorVG project. All rights reserved.

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

#ifndef _TVG_TASK_SCHEDULER_H_
#define _TVG_TASK_SCHEDULER_H_

#include "tvgCommon.h"
#include "tvgInlist.h"
#include "tvgArray.h"

#ifdef THORVG_THREAD_SUPPORT
    #include <mutex>
    #include <condition_variable>
#endif

namespace tvg
{

#ifdef THORVG_THREAD_SUPPORT

struct Task
{
private:
    Array<Task*> nexts;     // dependent tasks that must wait for this task to finish
    mutex mtx;              // protects task state shared between threads
    condition_variable cv;  // wakes done() when task execution completes
    uint16_t deps = 0;      // number of unfinished dependencies this task must wait for
    bool ready = true;      // true when no execution is outstanding
    bool pending = false;   // true when a submitted task has not been consumed by done()

    bool prepare()
    {
        lock_guard<mutex> lock(mtx);
        ready = false;
        pending = true;
        return deps == 0;
    }

protected:
    virtual void run(unsigned tid) = 0;

public:
    INLIST_ITEM(Task);

    virtual ~Task() = default;

    void done()
    {
        if (!pending) return;

        unique_lock<mutex> lock(mtx);
        while (!ready) cv.wait(lock);
        pending = false;
    }

    void depend(Task* task)
    {
        lock_guard<mutex> lock(task->mtx);
        if (task->ready) return;
        ++deps;
        task->nexts.push(this);
    }

    friend struct TaskSchedulerImpl;
};

#else  //THORVG_THREAD_SUPPORT

struct Task
{
public:
    INLIST_ITEM(Task);

    virtual ~Task() = default;
    void done() {}
    void depend(TVG_UNUSED Task* task) {}

protected:
    virtual void run(unsigned tid) = 0;

private:
    friend struct TaskSchedulerImpl;
};

#endif  //THORVG_THREAD_SUPPORT


struct TaskScheduler
{
    static uint32_t threads();
    static void init(uint32_t threads);
    static void term();
    static void request(Task* task);
};

}  // namespace tvg

#endif //_TVG_TASK_SCHEDULER_H_
