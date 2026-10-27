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


#include "tvgArray.h"
#include "tvgTaskScheduler.h"

#ifdef THORVG_THREAD_SUPPORT
    #include <atomic>
    #include <thread>
#endif

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

namespace tvg
{

#ifdef THORVG_THREAD_SUPPORT

struct TaskQueue
{
    Inlist<Task>             taskDeque;
    mutex                    mtx;
    condition_variable       ready;
    bool                     done = false;

    // try to get a task without waiting
    bool tryPop(Task** task)
    {
        unique_lock<mutex> lock{mtx, try_to_lock};
        if (!lock || taskDeque.empty()) return false;
        *task = taskDeque.front();
        return true;
    }

    // wait for an available task
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

    // try to enqueue a task without waiting
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

    // wait for the lock to enqueue a task
    void push(Task* task)
    {
        {
            lock_guard<mutex> lock{mtx};
            taskDeque.back(task);
        }
        ready.notify_one();
    }

    void complete()
    {
        {
            lock_guard<mutex> lock{mtx};
            done = true;
        }
        ready.notify_all();
    }
};

struct TaskSchedulerImpl
{
    Array<thread*>                 threads;
    Array<TaskQueue*>              taskQueues;
    atomic<uint32_t>               idx{0};

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

    void complete(Task* task)
    {
        lock_guard<mutex> lock(task->mtx);

        ARRAY_FOREACH(p, task->nexts) {
            auto waiter = *p;
            bool runnable;
            {
                lock_guard<mutex> lock(waiter->mtx);
                runnable = (--waiter->deps == 0 && waiter->pending);
            }
            if (runnable) enqueue(waiter);
        }
        task->nexts.clear();
        task->ready = true;
        task->cv.notify_one();
    }

    void run(unsigned i)
    {
        Task* task;

        // thread loop
        while (true) {
            auto success = false;
            // try to steal a task for load balancing
            for (uint32_t x = 0; x < threads.count; ++x) {
                if (taskQueues[(i + x) % threads.count]->tryPop(&task)) {
                    success = true;
                    break;
                }
            }
            if (!success && !taskQueues[i]->pop(&task)) break;
            task->run(i + 1);
            complete(task);
        }
    }

    void enqueue(Task* task)
    {
        auto i = idx.fetch_add(1, memory_order_relaxed);
        for (uint32_t n = 0; n < taskQueues.count; ++n) {
            if (taskQueues[(i + n) % taskQueues.count]->tryPush(task)) return;
        }
        taskQueues[i % taskQueues.count]->push(task);
    }

    void request(Task* task)
    {
        if (threads.empty()) task->run(0);        // sync
        else if (task->prepare()) enqueue(task);  // async
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

}  // namespace tvg

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
