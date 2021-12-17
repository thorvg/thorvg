/*
 * Copyright (c) 2020-2021 Samsung Electronics Co., Ltd. All rights reserved.

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

#include "tvgLottieImpl.h"

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

Lottie::Lottie() : pImpl(new Impl)
{
    Paint::pImpl->id = TVG_CLASS_ID_LOTTIE;
    Paint::pImpl->method(new PaintMethod<Lottie::Impl>(pImpl));
    //for Picture::pImpl access
    pImpl->pictureImpl = Picture::pImpl;
    pImpl->animationImpl = Animation::pImpl;
}


Lottie::~Lottie()
{
    delete(pImpl);
}


unique_ptr<Lottie> Lottie::gen() noexcept
{
    return unique_ptr<Lottie>(new Lottie);
}


uint32_t Lottie::identifier() noexcept
{
    return TVG_CLASS_ID_LOTTIE;
}


Result Lottie::load(const std::string& path) noexcept
{
    if (path.empty()) return Result::InvalidArguments;
    return pImpl->load(path);
}

