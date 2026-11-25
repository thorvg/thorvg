/*
 * Copyright (c) 2026 ThorVG project. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "testFramework.h"
#include "../../src/renderer/gpu_engine/wg/tvgWgPreparedScopes.h"
#include <thread>

namespace
{
struct ScheduledCallbacks
{
    WgPreparedScopeResult* scopes;
    unsigned calls = 0;
    bool firstSucceeded = true;
    bool secondSucceeded = true;

    static void progress(void* data)
    {
        auto callbacks = static_cast<ScheduledCallbacks*>(data);
        bool succeeded = callbacks->calls++ == 0 ? callbacks->firstSucceeded : callbacks->secondSucceeded;
        callbacks->scopes->callback(succeeded);
    }
};
}  // namespace

TEST_CASE("Prepared WebGPU error scopes", "[tvgWgPreparedScopes]")
{
    SUBCASE("accepts callbacks delivered synchronously")
    {
        auto scopes = new WgPreparedScopeResult;
        scopes->callback(true);
        scopes->callback(true);
        unsigned progressCalls = 0;
        auto progress = [](void* data) {
            ++*static_cast<unsigned*>(data);
        };
        CHECK(waitForPreparedScopes(scopes, progress, &progressCalls, std::chrono::milliseconds(5)));
        CHECK(progressCalls == 0);
    }

    SUBCASE("waits for both callbacks")
    {
        auto scopes = new WgPreparedScopeResult;
        ScheduledCallbacks callbacks{};
        callbacks.scopes = scopes;
        CHECK(waitForPreparedScopes(scopes, ScheduledCallbacks::progress, &callbacks, std::chrono::milliseconds(50)));
        CHECK(callbacks.calls == 2);
    }

    SUBCASE("accepts callbacks delivered from separate worker threads")
    {
        auto scopes = new WgPreparedScopeResult;
        std::thread allocation([scopes] {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            scopes->callback(true);
        });
        std::thread validation([scopes] {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            scopes->callback(true);
        });

        CHECK(waitForPreparedScopes(scopes, nullptr, nullptr, std::chrono::milliseconds(100)));
        allocation.join();
        validation.join();
    }

    SUBCASE("fails when the first callback reports an error")
    {
        auto scopes = new WgPreparedScopeResult;
        ScheduledCallbacks callbacks{};
        callbacks.scopes = scopes;
        callbacks.firstSucceeded = false;
        CHECK_FALSE(waitForPreparedScopes(scopes, ScheduledCallbacks::progress, &callbacks, std::chrono::milliseconds(50)));
        CHECK(callbacks.calls == 2);
    }

    SUBCASE("fails when the second callback reports an error")
    {
        auto scopes = new WgPreparedScopeResult;
        ScheduledCallbacks callbacks{};
        callbacks.scopes = scopes;
        callbacks.secondSucceeded = false;
        CHECK_FALSE(waitForPreparedScopes(scopes, ScheduledCallbacks::progress, &callbacks, std::chrono::milliseconds(50)));
        CHECK(callbacks.calls == 2);
    }

    SUBCASE("keeps callback state alive until callbacks arrive after timeout")
    {
        auto scopes = new WgPreparedScopeResult;
        CHECK_FALSE(waitForPreparedScopes(scopes, nullptr, nullptr, std::chrono::milliseconds(0)));
        std::thread lateCallbacks([scopes] {
            scopes->callback(true);
            scopes->callback(true);
        });
        lateCallbacks.join();
    }
}
