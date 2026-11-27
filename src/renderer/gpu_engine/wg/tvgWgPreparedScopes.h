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

#ifndef _TVG_WG_PREPARED_SCOPES_H_
#define _TVG_WG_PREPARED_SCOPES_H_

#include <atomic>
#include <chrono>
#include <thread>

class WgPreparedScopeResult
{
private:
    // The preparation worker owns one reference and each pop callback owns one.
    std::atomic<unsigned> refs{3};
    std::atomic<bool> failed{false};

    void release()
    {
        if (refs.fetch_sub(1, std::memory_order_acq_rel) == 1) delete this;
    }

public:
    bool complete() const { return refs.load(std::memory_order_acquire) == 1; }
    bool success() const { return complete() && !failed.load(std::memory_order_acquire); }

    void callback(bool succeeded)
    {
        if (!succeeded) failed.store(true, std::memory_order_release);
        release();
    }

    void releaseWorker() { release(); }
};

inline bool waitForPreparedScopes(
    WgPreparedScopeResult* scopes,
    void (*progress)(void*),
    void* userData,
    std::chrono::milliseconds timeout)
{
    auto deadline = std::chrono::steady_clock::now() + timeout;
    while (!scopes->complete() && std::chrono::steady_clock::now() < deadline) {
        if (progress) progress(userData);
        if (!scopes->complete()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    bool succeeded = scopes->success();
    scopes->releaseWorker();
    return succeeded;
}

#endif
