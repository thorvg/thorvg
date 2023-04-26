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

#ifndef _TVG_TASK_SCHEDULER_H_
#define _TVG_TASK_SCHEDULER_H_

#include <mutex>
#include <condition_variable>
#include "tvgCommon.h"

namespace tvg
{

struct Task;

struct TaskScheduler
{
    static unsigned threads();
    static void init(unsigned threads);
    static void term();
    static void request(Task* task);
};

struct Task
{
private:
    mutex                   finishedMtx;
    mutex                   preparedMtx;
    condition_variable      cv;
    bool                    finished = true;        //if run() finished
    bool                    prepared = false;       //the task is requested

public:
    virtual ~Task()
    {
        if (!prepared) return;

        //Guarantee the task is finished by TaskScheduler.
        unique_lock<mutex> lock(preparedMtx);

        while (prepared) {
            cv.wait(lock);
        }
    }

    void done(unsigned tid = 0)
    {
        if (finished) return;

        lock_guard<mutex> lock(finishedMtx);

        if (finished) return;

        //the job hasn't been launched yet.

        //set finished so that operator() quickly returns.
        finished = true;

        run(tid);
    }

protected:
    virtual void run(unsigned tid) = 0;

private:
    void finish()
    {
        lock_guard<mutex> lock(preparedMtx);
        prepared = false;
        cv.notify_one();
    }

    void operator()(unsigned tid)
    {
        if (finished) {
            finish();
            return;
        }

        lock_guard<mutex> lock(finishedMtx);

        if (finished) {
            finish();
            return;
        }

        run(tid);

        finished = true;

        finish();
    }

    void prepare()
    {
        finished = false;
        prepared = true;
    }

    friend struct TaskSchedulerImpl;
};

}

#endif //_TVG_TASK_SCHEDULER_H_
 