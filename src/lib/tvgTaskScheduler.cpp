/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd. All rights reserved.

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
#include <deque>
#include <thread>
#include "tvgCommon.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

namespace tvg {

struct TaskQueue {
    deque<shared_ptr<Task>>  taskDeque;
    mutex                    mtx;
    condition_variable       ready;
    bool                     done = false;

    bool tryPop(shared_ptr<Task> &task)
    {
        unique_lock<mutex> lock{mtx, try_to_lock};
        if (!lock || taskDeque.empty()) return false;
        task = move(taskDeque.front());
        taskDeque.pop_front();

        return true;
    }

    bool tryPush(shared_ptr<Task> &&task)
    {
        {
            unique_lock<mutex> lock{mtx, try_to_lock};
            if (!lock) return false;
            taskDeque.push_back(move(task));
        }

        ready.notify_one();

        return true;
    }

    void complete()
    {
        {
            unique_lock<mutex> lock{mtx};
            done = true;
        }
        ready.notify_all();
    }

    bool pop(shared_ptr<Task> &task)
    {
        unique_lock<mutex> lock{mtx};

        while (taskDeque.empty() && !done) {
            ready.wait(lock);
        }

        if (taskDeque.empty()) return false;

        task = move(taskDeque.front());
        taskDeque.pop_front();

        return true;
    }

    void push(shared_ptr<Task> &&task)
    {
        {
            unique_lock<mutex> lock{mtx};
            taskDeque.push_back(move(task));
        }

        ready.notify_one();
    }

};


class TaskSchedulerImpl
{
public:
    unsigned                       threadCnt;
    vector<thread>                 threads;
    vector<TaskQueue>              taskQueues;
    atomic<unsigned>               idx{0};

    TaskSchedulerImpl(unsigned count) : taskQueues(count)
    {
        threadCnt = count;
        for (unsigned i = 0; i < threadCnt; ++i) {
            threads.emplace_back([&, i] { run(i); });
        }
    }

    ~TaskSchedulerImpl()
    {
        for (auto& queue : taskQueues) queue.complete();
        for (auto& thread : threads) thread.join();
    }

    void run(unsigned i)
    {
        shared_ptr<Task> task;

        //Thread Loop
        while (true) {
            auto success = false;

            for (unsigned i = 0; i < threadCnt * 2; ++i) {
                if (taskQueues[(i + i) % threadCnt].tryPop(task)) {
                    success = true;
                    break;
                }
            }

            if (!success && !taskQueues[i].pop(task)) break;

            (*task)();
        }
    }

    void request(shared_ptr<Task> task)
    {
        //Async
        if (threadCnt > 0) {
            task->prepare();
            auto i = idx++;
            for (unsigned n = 0; n < threadCnt; ++n) {
                if (taskQueues[(i + n) % threadCnt].tryPush(move(task))) return;
            }

            taskQueues[i % threadCnt].push(move(task));

        //Sync
        } else {
            task->run();
        }
    }
};

}

static TaskSchedulerImpl* inst = nullptr;

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

void TaskScheduler::init(unsigned threads)
{
    if (inst) return;
    inst = new TaskSchedulerImpl(threads);
}


void TaskScheduler::term()
{
    if (!inst) return;
    delete(inst);
    inst = nullptr;
}


void TaskScheduler::request(shared_ptr<Task> task)
{
    if (inst) {
        inst->request(move(task));
    }
}