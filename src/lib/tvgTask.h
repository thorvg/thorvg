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
#ifndef _TVG_TASK_H_
#define _TVG_TASK_H_

#include <memory>
#include <future>

namespace tvg
{

/*
  Task Interface.
  Able to run a task in the thread pool. derive from the
  task interface and implement run method.

  To get the result call task->get() which will return immidiately if the
  task is already finishd otherwise will wait till task completion.
 */

class Task
{
public:
    virtual ~Task() = default;
    void get() { if (_receiver.valid()) _receiver.get(); }

protected:
    virtual void run() = 0;
private:
    void operator()()
    {
        run();
        _sender.set_value();
    }
    void prepare()
    {
        _sender = std::promise<void>();
        _receiver = _sender.get_future();
    }
    friend class Executor;

    std::promise<void> _sender;
    std::future<void>  _receiver;
};


using shared_task = std::shared_ptr<Task>;

/*
  async() function takes a shared task and runs it in
  a thread pool asyncronously. call get() on the shared_task
  to get the ressult out of the shared_task.
 */
void async(shared_task task);

}

#endif //_TVG_TASK_H_