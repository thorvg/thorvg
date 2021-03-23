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

#ifndef _TVG_GL_RENDER_PROPERTIES_H_
#define _TVG_GL_RENDER_PROPERTIES_H_

#include <vector>
#include "tvgGlCommon.h"
#include "tvgGlProgram.h"

class PropertyValue
{
public:
    void setStride(uint32_t s)
    {
        stride = s;
        if (values.capacity() == values.size())
        {
            values.reserve(values.size() + stride);
        }
    }

    uint32_t getStride() const
    {
        return stride;
    }

    uint32_t getSize() const
    {
        return values.size();
    }

    uint32_t getCount() const
    {
        return (values.size() / stride);
    }

    void clear()
    {
        values.clear();
    }

    const float* getData()
    {
        return values.data();
    }

    void set(float v)
    {
        values.push_back(v);
    }

    template<typename... Args>
    void set(float first, Args... args)
    {
        if (values.capacity() == values.size())
        {
            values.reserve(values.size() + stride);
        }
        set(first);
        set(args...);
    }

private:
    std::vector<float> values;
    uint32_t stride = 0;
};

struct VertexProperty
{
public:
    enum class DataType
    {
        INT = 0,
        FLOAT,
        MATRIX
    };
    enum class PropertyType
    {
        ATTRIBUTE = 0,
        UNIFORM
    };

    int32_t propertyId = -1;
    std::string propertyName = "";
    PropertyType propType = PropertyType::UNIFORM;
    DataType dataType = DataType::FLOAT;
    PropertyValue propertyValues;
};


#endif /* _TVG_GL_RENDER_PROPERTIES_H_ */
