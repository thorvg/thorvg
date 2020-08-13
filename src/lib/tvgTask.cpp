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

#include <tvgTask.h>

#include <deque>

template <typename Task>
class TaskQueue {
    using lock_t = std::unique_lock<std::mutex>;
    std::deque<Task>        _q;
    bool                    _done{false};
    std::mutex              _mutex;
    std::condition_variable _ready;

public:
    bool try_pop(Task &task)
    {
        lock_t lock{_mutex, std::try_to_lock};
        if (!lock || _q.empty()) return false;
        task = std::move(_q.front());
        _q.pop_front();
        return true;
    }

    bool try_push(Task &&task)
    {
        {
            lock_t lock{_mutex, std::try_to_lock};
            if (!lock) return false;
            _q.push_back(std::move(task));
        }
        _ready.notify_one();
        return true;
    }

    void done()
    {
        {
            lock_t lock{_mutex};
            _done = true;
        }
        _ready.notify_all();
    }

    bool pop(Task &task)
    {
        lock_t lock{_mutex};
        while (_q.empty() && !_done) _ready.wait(lock);
        if (_q.empty()) return false;
        task = std::move(_q.front());
        _q.pop_front();
        return true;
    }

    void push(Task &&task)
    {
        {
            lock_t lock{_mutex};
            _q.push_back(std::move(task));
        }
        _ready.notify_one();
    }

};

#include <thread>
#include <vector>

namespace tvg
{

class Executor
{
    const unsigned                      _count{std::thread::hardware_concurrency()};
    std::vector<std::thread>            _threads;
    std::vector<TaskQueue<shared_task>> _q{_count};
    std::atomic<unsigned>               _index{0};
    void run(unsigned i)
    {
        // Task Loop
        shared_task task;
        while (true) {
            bool success = false;

            for (unsigned n = 0; n != _count * 2; ++n) {
                if (_q[(i + n) % _count].try_pop(task)) {
                    success = true;
                    break;
                }
            }

            if (!success && !_q[i].pop(task)) break;

            (*task)();
        }
    }

    Executor()
    {
        for (unsigned n = 0; n != _count; ++n) {
            _threads.emplace_back([&, n] { run(n); });
        }
    }
    ~Executor()
    {
        for (auto &e : _q) e.done();

        for (auto &e : _threads) e.join();
    }

public:

    static Executor& instance() {
        static Executor singleton;
        return singleton;
    }

    void post(shared_task task)
    {
        task->prepare();

        auto i = _index++;

        for (unsigned n = 0; n != _count; ++n) {
            if (_q[(i + n) % _count].try_push(std::move(task))) return;
        }

        if (_count > 0) {
            _q[i % _count].push(std::move(task));
        }
    }
};

void async(shared_task task)
{
    Executor::instance().post(std::move(task));
}

}


