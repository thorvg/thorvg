#ifndef _TVG_GL_RENDER_PROPERTIES_H_
#define _TVG_GL_RENDER_PROPERTIES_H_

#include "tvgGlShader.h"
#include "tvgGlProgram.h"

#include <string>
#include <memory>
#include <vector>
#include <map>

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
        FLOAT
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
