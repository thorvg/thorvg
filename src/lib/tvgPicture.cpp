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
#ifndef _TVG_PICTURE_CPP_
#define _TVG_PICTURE_CPP_

#include "tvgPictureImpl.h"

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

Picture::Picture() : pImpl(make_unique<Impl>())
{
    Paint::IMPL->method(new PaintMethod<Picture::Impl>(IMPL));
}


Picture::~Picture()
{
}


unique_ptr<Picture> Picture::gen() noexcept
{
    return unique_ptr<Picture>(new Picture);
}


Result Picture::load(const std::string& path) noexcept
{
    if (path.empty()) return Result::InvalidArguments;

    return IMPL->load(path);
}


Result Picture::size(float* w, float* h) const noexcept
{
    if (IMPL->size(w, h)) return Result::Success;
    return Result::InsufficientCondition;
}

#endif /* _TVG_PICTURE_CPP_ */