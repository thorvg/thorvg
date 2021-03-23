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

#ifndef _TVG_GL_PROPERTY_INTERFACE_H_
#define _TVG_GL_PROPERTY_INTERFACE_H_

#include "tvgGlRenderTask.h"


#define ADD_ATTRIBUTE_PROPERTY(var, rtask, prog, varName, formatSize, location) \
    var = &PropertyInterface::addProperty(rtask, prog, varName, formatSize, VertexProperty::PropertyType::ATTRIBUTE); \
    if (var->propertyId != -1) \
        location = var->propertyId

#define ADD_UNIFORM_PROPERTY(var, rtask, prog, varName, formatSize, location) \
    var = &PropertyInterface::addProperty(rtask, prog, varName, formatSize, VertexProperty::PropertyType::UNIFORM); \
    if (var->propertyId != -1) \
        location = var->propertyId

#define ADD_UNIFORM_PROPERTY_2(var, rtask, prog, varName, formatSize, location, datatype) \
    var = &PropertyInterface::addProperty(rtask, prog, varName, formatSize, VertexProperty::PropertyType::UNIFORM, datatype); \
    if (var->propertyId != -1) \
        location = var->propertyId

#define FORMAT_SIZE_FLOAT   1
#define FORMAT_SIZE_VEC_2   2
#define FORMAT_SIZE_VEC_3   3
#define FORMAT_SIZE_VEC_4   4
#define FORMAT_SIZE_MAT_4x4   16

class PropertyInterface
{
public:
    static VertexProperty& addProperty(GlRenderTask* rTask, std::shared_ptr<GlProgram> prog, std::string name, uint32_t propFormatSize, VertexProperty::PropertyType propType, VertexProperty::DataType dataType = VertexProperty::DataType::FLOAT);
    template<typename... Args>
    static void setProperty(GlRenderTask* rTask, std::string name, float first, Args... args)
    {
        VertexProperty& prop = getProperty(rTask, name);
        if (prop.propertyId == -1)
        {
            return;
        }
        setProperty(prop.propertyId, first, args...);
    }
    template<typename... Args>
    static void setProperty(GlRenderTask* rTask, int32_t propId, float first, Args... args)
    {
        std::map<int32_t, VertexProperty>::iterator itr = rTask->getUniformVertexProperty().find(propId);
        if (itr->second.propertyId == -1)
        {
            return;
        }
        VertexProperty& prop = itr->second;

        prop.propertyValues.set(first, args...);
    }

    static void setProperty(GlRenderTask* rTask, int32_t propId, int32_t count, float* data);
    static int32_t getPropertyId(GlRenderTask* rTask, std::string name);
    static VertexProperty& getProperty(GlRenderTask* rTask, std::string name);
    static VertexProperty& getProperty(GlRenderTask* rTask, int32_t propId);
    static void clearData(GlRenderTask* rTask);

private:
    PropertyInterface() {}
    static VertexProperty  mEmptyProperty;
};


#endif /* _TVG_GL_PROPERTY_INTERFACE_H_ */
