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

#include <thorvg.h>
#include "catch.hpp"
#include "testEngine.h"

using namespace tvg;

#if defined(THORVG_WG_ENGINE_SUPPORT) && defined(THORVG_WG_TEST_SUPPORT)

namespace
{

#if defined(__APPLE__)
// WGPU on macOS can time out when every blend/mask combination is batched into one draw.
constexpr bool SyncEachCombination = true;
#else
constexpr bool SyncEachCombination = false;
#endif

}

TEST_CASE("WG Basic draw", "[tvgWgEngine]")
{
    TvgWgTestEngine engine;
    TestEngine::basic(engine, {}, {}, SyncEachCombination);
}

TEST_CASE("WG Image Draw", "[tvgWgEngine]")
{
    TvgWgTestEngine engine;
    REQUIRE(TestEngine::image(engine, {}, {}, SyncEachCombination));
}

TEST_CASE("WG Filling Draw", "[tvgWgEngine]")
{
    TvgWgTestEngine engine;
    TestEngine::filling(engine);
}

TEST_CASE("WG Image Rotation", "[tvgWgEngine]")
{
    TvgWgTestEngine engine;
    REQUIRE(TestEngine::imageRotation(engine));
}

TEST_CASE("WG Scene Effects", "[tvgWgEngine]")
{
    TvgWgTestEngine engine;
    TestEngine::sceneEffects(engine);
}

TEST_CASE("WG Solid Batch", "[tvgWgEngine]")
{
    TvgWgTestEngine engine;
    TestEngine::solidBatch(engine);
}

#endif
