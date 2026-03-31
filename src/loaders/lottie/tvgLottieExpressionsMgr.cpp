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

#include "tvgLottieModel.h"
#include "tvgLottieExpressionsMgr.h"
#include "tvgLottieExpressions.h"

#ifdef THORVG_LOTTIE_EXPRESSIONS_SUPPORT

static uint32_t _refCnt = 0;

#ifdef THORVG_THREAD_SUPPORT

#include "jerryscript-port.h"
#include "tvgLock.h"

#include <thread>

/* defined in jerry-port-context.cpp */
extern void tvg_jerry_context_set(jerry_context_t *context_p);

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

struct LottieExpressionsInstance
{
    LottieExpressions* exps;
    jerry_context_t* ctx;
    thread::id tid;
};

static Array<LottieExpressionsInstance> _instances;
static Key _key;

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

LottieExpressions* LottieExpressionsMgr::expressions()
{
    auto tid = this_thread::get_id();
    ScopedLock lock(_key);

    for (auto& inst : _instances) {
        if (inst.tid == tid) return inst.exps;
    }

    auto& inst = _instances.next();
    inst.tid = tid;
    inst.exps = new LottieExpressions();
    inst.ctx = jerry_port_context_get();
    return inst.exps;
}


void LottieExpressionsMgr::ref()
{
    ScopedLock lock(_key);
    ++_refCnt;
}


void LottieExpressionsMgr::unref()
{
    ScopedLock lock(_key);
    if (--_refCnt == 0) {
        for (auto& inst : _instances) {
            tvg_jerry_context_set(inst.ctx);
            delete(inst.exps);
        }
        _instances.clear();
    }
}

#else //THORVG_THREAD_SUPPORT

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

static LottieExpressions* _exps = nullptr;

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

LottieExpressions* LottieExpressionsMgr::expressions()
{
    if (!_exps) _exps = new LottieExpressions;
    return _exps;
}


void LottieExpressionsMgr::ref()
{
    ++_refCnt;
}


void LottieExpressionsMgr::unref()
{
    if (--_refCnt == 0) {
        delete(_exps);
        _exps = nullptr;
    }
}

#endif //THORVG_THREAD_SUPPORT

#endif //THORVG_LOTTIE_EXPRESSIONS_SUPPORT
