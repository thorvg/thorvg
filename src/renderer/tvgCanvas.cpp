/*
 * Copyright (c) 2020 - 2025 the ThorVG project. All rights reserved.

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

#include "tvgCanvas.h"

Canvas::Canvas():pImpl(new Impl)
{
}


Canvas::~Canvas()
{
    delete(pImpl);
}


const list<Paint*>& Canvas::paints() const noexcept
{
    return pImpl->scene->paints();
}


Result Canvas::push(Paint* target, Paint* at) noexcept
{
    return pImpl->push(target, at);
}


Result Canvas::draw(bool clear) noexcept
{
    TVGLOG("RENDERER", "Draw S. -------------------------------- Canvas(%p)", this);
    auto ret = pImpl->draw(clear);
    TVGLOG("RENDERER", "Draw E. -------------------------------- Canvas(%p)", this);

    return ret;
}


Result Canvas::update() noexcept
{
    TVGLOG("RENDERER", "Update S. ------------------------------ Canvas(%p)", this);
    if (pImpl->scene->paints().empty() || pImpl->status == Status::Drawing) return Result::InsufficientCondition;
    auto ret = pImpl->update(nullptr, false);
    TVGLOG("RENDERER", "Update E. ------------------------------ Canvas(%p)", this);

    return ret;
}


Result Canvas::remove(Paint* paint) noexcept
{
    return pImpl->remove(paint);
}


Result Canvas::viewport(int32_t x, int32_t y, int32_t w, int32_t h) noexcept
{
    return pImpl->viewport(x, y, w, h);
}


Result Canvas::sync() noexcept
{
    return pImpl->sync();
}