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

#include "tvgGlPropertyInterface.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

VertexProperty  PropertyInterface::mEmptyProperty;


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

VertexProperty& PropertyInterface::addProperty(GlRenderTask* rTask, std::shared_ptr<GlProgram> prog, std::string name, uint32_t propFormatSize, VertexProperty::PropertyType propType, VertexProperty::DataType dataType)
{
    std::map<int32_t, VertexProperty>* vertexProperty = nullptr;
    int32_t id;
    switch (propType) {
    case VertexProperty::PropertyType::ATTRIBUTE: {
        id = prog->getAttributeLocation(name.c_str());
        vertexProperty = &rTask->getAttributeVertexProperty();
        break;
    }
    case VertexProperty::PropertyType::UNIFORM: {
        id = prog->getUniformLocation(name.c_str());
        vertexProperty = &rTask->getUniformVertexProperty();
        break;
    }
    default: break;
    }
    if (id != -1)
    {
        VertexProperty property = { id, name, propType, dataType };
        property.propertyValues.setStride(propFormatSize);
        if (vertexProperty)
        {
            (*vertexProperty)[id] = property;
            return (*vertexProperty)[id];
        }
    }
    return mEmptyProperty;
}


void PropertyInterface::setProperty(GlRenderTask* rTask, int32_t propId, int32_t count, float* data)
{
    std::map<int32_t, VertexProperty>::iterator itr = rTask->getUniformVertexProperty().find(propId);
    if (itr->second.propertyId == -1) return;

    VertexProperty& prop = itr->second;

    for (int i = 0; i < count; ++i) {
        prop.propertyValues.set(data[i]);
    }
}

int32_t PropertyInterface::getPropertyId(GlRenderTask* rTask, std::string name)
{
    std::map<int32_t, VertexProperty>& vertexProperty = rTask->getUniformVertexProperty();
    for (auto& property : vertexProperty) {
        if (property.second.propertyName == name) return property.second.propertyId;
    }
    return -1;
}


VertexProperty& PropertyInterface::getProperty(GlRenderTask* rTask, std::string name)
{
    std::map<int32_t, VertexProperty>& vertexProperty = rTask->getUniformVertexProperty();
    for (auto& property : vertexProperty) {
        if (property.second.propertyName == name) return property.second;
    }
    return mEmptyProperty;
}


VertexProperty& PropertyInterface::getProperty(GlRenderTask* rTask, int32_t propId)
{
    std::map<int32_t, VertexProperty>& vertexProperty = rTask->getUniformVertexProperty();
    if (vertexProperty.find(propId) != vertexProperty.end()) return vertexProperty[propId];
    return mEmptyProperty;
}


void PropertyInterface::clearData(GlRenderTask* rTask)
{
    std::map<int32_t, VertexProperty>& vertexProperty = rTask->getUniformVertexProperty();
    for (auto& prop : vertexProperty) {
        prop.second.propertyValues.clear();
    }
}
