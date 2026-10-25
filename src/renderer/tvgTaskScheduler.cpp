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


#include "tvgTaskScheduler.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

namespace tvg {

#ifdef THORVG_THREAD_SUPPORT

struct TaskQueue {
    Inlist<Task>             taskDeque;
    mutex                    mtx;
    condition_variable       ready;
    bool                     done = false;

    bool tryPop(Task** task)
    {
        unique_lock<mutex> lock{mtx, try_to_lock};
        if (!lock || taskDeque.empty()) return false;
        *task = taskDeque.front();
        return true;
    }

    bool tryPush(Task* task)
    {
        {
            unique_lock<mutex> lock{mtx, try_to_lock};
            if (!lock) return false;
            taskDeque.back(task);
        }
        ready.notify_one();
        return true;
    }

    void complete()
    {
        {
            lock_guard<mutex> lock{mtx};
            done = true;
        }
        ready.notify_all();
    }

    bool pop(Task** task)
    {
        unique_lock<mutex> lock{mtx};

        while (taskDeque.empty() && !done) {
            ready.wait(lock);
        }

        if (taskDeque.empty()) return false;

        *task = taskDeque.front();
        return true;
    }

    void push(Task* task)
    {
        {
            lock_guard<mutex> lock{mtx};
            taskDeque.back(task);
        }
        ready.notify_one();
    }
};


struct TaskSchedulerImpl
{
    Array<thread*>                 threads;
    Array<TaskQueue*>              taskQueues;
    atomic<uint32_t>               idx{0};
    mutex drainMtx;
    condition_variable drained;
    uint32_t deferredCount = 0;

    TaskSchedulerImpl(uint32_t threadCnt)
    {
        threads.reserve(threadCnt);
        taskQueues.reserve(threadCnt);

        for (uint32_t i = 0; i < threadCnt; ++i) {
            taskQueues.push(new TaskQueue);
            threads.push(new thread);
        }
        for (uint32_t i = 0; i < threadCnt; ++i) {
            *threads.data[i] = thread([&, i] { run(i); });
        }
    }

    ~TaskSchedulerImpl()
    {
        // Waiting tasks can still enqueue successors. Keep all workers alive until drained.
        {
            unique_lock<mutex> lock(drainMtx);
            drained.wait(lock, [&] { return deferredCount == 0; });
        }
        ARRAY_FOREACH(p, taskQueues) {
            (*p)->complete();
        }
        ARRAY_FOREACH(p, threads) {
            (*p)->join();
            delete(*p);
        }
        ARRAY_FOREACH(p, taskQueues) {
            delete(*p);
        }
    }

    void run(unsigned i)
    {
        Task* task;

        //Thread Loop
        while (true) {
            auto success = false;
            for (uint32_t x = 0; x < threads.count * 2; ++x) {
                if (taskQueues[(i + x) % threads.count]->tryPop(&task)) {
                    success = true;
                    break;
                }
            }

            if (!success && !taskQueues[i]->pop(&task)) break;
            auto deferred = task->dependencies.count > 0;
            task->run(i + 1);

            // Keep prerequisite results alive until this task has finished reading them.
            ARRAY_FOREACH(p, task->dependencies) {
                lock_guard<mutex> lock((*p)->mtx);
                if (--(*p)->users == 0) (*p)->cv.notify_all();
            }
            task->dependencies.clear();

            {
                lock_guard<mutex> lock(task->mtx);
                task->ready = true;
                ARRAY_FOREACH(p, task->dependents) {
                    if ((*p)->remaining.fetch_sub(1) == 1) enqueue(*p);
                }
                task->dependents.clear();
                task->cv.notify_all();
            }
            // done() may now return and destroy task. Do not access it again.
            if (deferred) {
                lock_guard<mutex> lock(drainMtx);
                if (--deferredCount == 0) drained.notify_one();
            }
        }
    }

    void enqueue(Task* task)
    {
        auto i = idx++;
        for (uint32_t n = 0; n < threads.count; ++n) {
            if (taskQueues[(i + n) % threads.count]->tryPush(task)) return;
        }
        taskQueues[i % threads.count]->push(task);
    }

    void request(Task* task)
    {
        //Async
        if (threads.count > 0) {
            task->prepare();
            if (task->dependencies.count == 0) {
                enqueue(task);
                return;
            }
            {
                lock_guard<mutex> lock(drainMtx);
                ++deferredCount;
            }
            // The submission reference prevents execution before registration finishes.
            task->remaining = 1;
            ARRAY_FOREACH(p, task->dependencies) {
                auto dependency = *p;
                lock_guard<mutex> lock(dependency->mtx);
                ++dependency->users;
                dependency->pending = true;
                if (!dependency->ready) {
                    ++task->remaining;
                    dependency->dependents.push(task);
                }
            }
            if (task->remaining.fetch_sub(1) == 1) enqueue(task);
        // Sync
        } else {
            task->run(0);
            task->dependencies.clear();
        }
    }

    uint32_t threadCnt()
    {
        return threads.count;
    }
};

#else //THORVG_THREAD_SUPPORT

struct TaskSchedulerImpl
{
    TaskSchedulerImpl(TVG_UNUSED uint32_t threadCnt) {}
    void request(Task* task) { task->run(0); }
    uint32_t threadCnt() { return 0; }
};

#endif //THORVG_THREAD_SUPPORT


} //namespace

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

static TaskSchedulerImpl* _inst = nullptr;

void TaskScheduler::init(uint32_t threads)
{
    if (_inst) return;
    _inst = new TaskSchedulerImpl(threads);
}


void TaskScheduler::term()
{
    delete(_inst);
    _inst = nullptr;
}


void TaskScheduler::request(Task* task)
{
    if (_inst) _inst->request(task);
}


uint32_t TaskScheduler::threads()
{
    return _inst ? _inst->threadCnt() : 0;
}
