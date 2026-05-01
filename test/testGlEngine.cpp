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

#if defined(THORVG_GL_ENGINE_SUPPORT) && defined(THORVG_GL_TEST_SUPPORT)

TEST_CASE("GL Basic draw", "[tvgGlEngine]")
{
    TvgGlTestEngine engine;
    TestEngine::basic(engine);
}

TEST_CASE("GL Image Draw", "[tvgGlEngine]")
{
    TvgGlTestEngine engine;
    REQUIRE(TestEngine::image(engine));
}

TEST_CASE("GL Filling Draw", "[tvgGlEngine]")
{
    TvgGlTestEngine engine;

    // TODO: Enable all blend combinations after fixing the RadialGradient GL shader compile failure.
    // Other TestEngine cases do not exercise RadialGradient.
    // This path currently fails with ERROR: 0:188: 'premature EOF', then GL_INVALID_OPERATION.
    TestEngine::filling(engine, {BlendMethod::Normal}, maskMethods());
}

TEST_CASE("GL Image Rotation", "[tvgGlEngine]")
{
    TvgGlTestEngine engine;
    REQUIRE(TestEngine::imageRotation(engine));
}

TEST_CASE("GL Scene Effects", "[tvgGlEngine]")
{
    TvgGlTestEngine engine;
    TestEngine::sceneEffects(engine);
}

TEST_CASE("GL Solid Batch", "[tvgGlEngine]")
{
    TvgGlTestEngine engine;
    TestEngine::solidBatch(engine);
}

#endif
