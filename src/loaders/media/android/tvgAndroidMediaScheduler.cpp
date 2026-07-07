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

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "tvgAndroidMediaScheduler.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

static std::mutex _mtx;
static std::condition_variable _cv;
static std::thread* _worker = nullptr;
static MediaTask* _head = nullptr;
static AndroidMediaScheduler _scheduler;
static bool _done = false;
static bool _stopping = false;

//sweep every task under the lock, then sleep to the earliest requested wake-up (forever when all parked)
static void _run()
{
    std::unique_lock<std::mutex> lock(_mtx);
    while (!_done) {
        int64_t waitUs = -1;
        for (auto task = _head; task; task = task->next) {
            auto delayUs = task->pumpFrame();
            if (delayUs < 0) continue;
            if (waitUs < 0 || delayUs < waitUs) waitUs = delayUs;
        }

        if (waitUs < 0) _cv.wait(lock);
        else _cv.wait_for(lock, std::chrono::microseconds(waitUs));
    }
}

//tear down the worker (requires _mtx): the lock drops around join() so the worker can exit its wait
static void _stop(std::unique_lock<std::mutex>& lock)
{
    _done = true;
    _stopping = true;
    _cv.notify_all();
    auto stale = _worker;
    _worker = nullptr;

    lock.unlock();
    stale->join();
    delete(stale);
    lock.lock();

    _stopping = false;   //lets a waiting attach() spawn a fresh worker
    _cv.notify_all();
}

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

void AndroidMediaScheduler::attach(MediaTask* task)
{
    std::unique_lock<std::mutex> lock(_mtx);
    while (_stopping) _cv.wait(lock);   //an old worker may still be shutting down: wait the teardown out
    if (task->attached) return;

    if (!_worker) {
        _done = false;
        _worker = new std::thread(_run);
    }
    task->next = _head;
    task->attached = true;
    _head = task;
    _cv.notify_one();
}

void AndroidMediaScheduler::detach(MediaTask* task)
{
    std::unique_lock<std::mutex> lock(_mtx);
    if (!task->attached) return;

    if (_head == task) _head = task->next;
    else {
        for (auto prev = _head; prev; prev = prev->next) {
            if (prev->next != task) continue;
            prev->next = task->next;
            break;
        }
    }

    task->attached = false;
    task->next = nullptr;
    if (!_head && _worker) _stop(lock);   //the last task detaching parks the worker
}

void AndroidMediaScheduler::wake()
{
    _cv.notify_one();
}

AndroidMediaScheduler& androidMediaScheduler()
{
    return _scheduler;
}