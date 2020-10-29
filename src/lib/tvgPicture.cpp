/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd. All rights reserved.

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

#include "tvgPictureImpl.h"

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

Picture::Picture() : pImpl(new Impl(this))
{
    Paint::pImpl->method(new PaintMethod<Picture::Impl>(pImpl));
}


Picture::~Picture()
{
    delete(pImpl);
}


unique_ptr<Picture> Picture::gen() noexcept
{
    return unique_ptr<Picture>(new Picture);
}


Result Picture::load(const std::string& path, uint32_t width, uint32_t height) noexcept
{
    if (path.empty()) return Result::InvalidArguments;

    return pImpl->load(path, width, height);
}


Result Picture::load(const char* data, uint32_t size) noexcept
{
    if (!data || size <= 0) return Result::InvalidArguments;

    return pImpl->load(data, size);
}


Result Picture::load(uint32_t* data, uint32_t width, uint32_t height, bool isCopy) noexcept
{
    if (!data || width <= 0 || height <= 0) return Result::InvalidArguments;

    return pImpl->load(data, width, height, isCopy);
}


Result Picture::viewbox(float* x, float* y, float* w, float* h) const noexcept
{
    if (pImpl->viewbox(x, y, w, h)) return Result::Success;
    return Result::InsufficientCondition;
}
