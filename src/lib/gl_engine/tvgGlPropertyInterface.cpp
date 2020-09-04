#include "tvgGlPropertyInterface.h"
#include "tvgGlRenderTask.h"

VertexProperty  PropertyInterface::mEmptyProperty;

VertexProperty& PropertyInterface::addProperty(GlRenderTask* rTask, std::shared_ptr<GlProgram> prog, std::string name, uint32_t propFormatSize, VertexProperty::PropertyType propType)
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
        VertexProperty property = { id, name, propType, VertexProperty::DataType::FLOAT };
        property.propertyValues.setStride(propFormatSize);
        if (vertexProperty)
        {
            (*vertexProperty)[id] = property;
            return (*vertexProperty)[id];
        }
    }
    return mEmptyProperty;
}


int32_t PropertyInterface::getPropertyId(GlRenderTask* rTask, std::string name)
{
    std::map<int32_t, VertexProperty>& vertexProperty = rTask->getUniformVertexProperty();
    for (auto& property : vertexProperty)
    {
        if (property.second.propertyName == name)
        {
            return property.second.propertyId;
        }
    }
    return -1;
}


VertexProperty& PropertyInterface::getProperty(GlRenderTask* rTask, std::string name)
{
    std::map<int32_t, VertexProperty>& vertexProperty = rTask->getUniformVertexProperty();
    for (auto& property : vertexProperty)
    {
        if (property.second.propertyName == name)
        {
            return property.second;
        }
    }
    return mEmptyProperty;
}


VertexProperty& PropertyInterface::getProperty(GlRenderTask* rTask, int32_t propId)
{
    std::map<int32_t, VertexProperty>& vertexProperty = rTask->getUniformVertexProperty();
    if (vertexProperty.find(propId) != vertexProperty.end())
    {
        return vertexProperty[propId];
    }
    return mEmptyProperty;
}


void PropertyInterface::clearData(GlRenderTask* rTask)
{
    std::map<int32_t, VertexProperty>& vertexProperty = rTask->getUniformVertexProperty();
    for (auto& prop : vertexProperty)
    {
        prop.second.propertyValues.clear();
    }
}
