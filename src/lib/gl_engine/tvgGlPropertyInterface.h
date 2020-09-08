#ifndef _TVG_GL_PROPERTY_INTERFACE_H_
#define _TVG_GL_PROPERTY_INTERFACE_H_

#include "tvgGlRendererProperties.h"
#include "tvgGlRenderTask.h"

#include <string>
#include <memory>
#include <map>

#define ADD_ATTRIBUTE_PROPERTY(var, rtask, prog, varName, formatSize, location) \
    var = &PropertyInterface::addProperty(rtask, prog, varName, formatSize, VertexProperty::PropertyType::ATTRIBUTE); \
    if (var->propertyId != -1) \
        location = var->propertyId

#define ADD_UNIFORM_PROPERTY(var, rtask, prog, varName, formatSize, location) \
    var = &PropertyInterface::addProperty(rtask, prog, varName, formatSize, VertexProperty::PropertyType::UNIFORM); \
    if (var->propertyId != -1) \
        location = var->propertyId

#define FORMAT_SIZE_FLOAT   1
#define FORMAT_SIZE_VEC_2   2
#define FORMAT_SIZE_VEC_3   3
#define FORMAT_SIZE_VEC_4   4

class PropertyInterface
{
public:
    static VertexProperty& addProperty(GlRenderTask* rTask, std::shared_ptr<GlProgram> prog, std::string name, uint32_t propFormatSize, VertexProperty::PropertyType propType);
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

        prop.dataType = VertexProperty::DataType::FLOAT;
        prop.propertyValues.set(first, args...);
    }
    static int32_t getPropertyId(GlRenderTask* rTask, std::string name);
    static VertexProperty& getProperty(GlRenderTask* rTask, std::string name);
    static VertexProperty& getProperty(GlRenderTask* rTask, int32_t propId);
    static void clearData(GlRenderTask* rTask);

private:
    PropertyInterface() {}
    static VertexProperty  mEmptyProperty;
};


#endif /* _TVG_GL_PROPERTY_INTERFACE_H_ */
