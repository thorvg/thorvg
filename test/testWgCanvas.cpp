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
#include "testCanvas.h"

using namespace tvg;

#if defined(THORVG_WG_ENGINE_SUPPORT) && defined(THORVG_WG_TEST_SUPPORT)

TEST_CASE("WG Missing Initialization", "[tvgWgCanvas]")
{
    TvgWgTestEngine engine;
    TestCanvas::missingInitialization(engine);
}

TEST_CASE("WG Basic Creation", "[tvgWgCanvas]")
{
    TvgWgTestEngine engine;
    TestCanvas::basicCreation(engine);
}

TEST_CASE("WG Pushing Paints", "[tvgWgCanvas]")
{
    TvgWgTestEngine engine;
    TestCanvas::pushingPaints(engine);
}

TEST_CASE("WG Update", "[tvgWgCanvas]")
{
    TvgWgTestEngine engine;
    TestCanvas::update(engine);
}

TEST_CASE("WG Synchronized Drawing", "[tvgWgCanvas]")
{
    TvgWgTestEngine engine;
    TestCanvas::synchronizedDrawing(engine);
}

TEST_CASE("WG Viewport", "[tvgWgCanvas]")
{
    TvgWgTestEngine engine;
    TestCanvas::viewport(engine);
}

#endif
