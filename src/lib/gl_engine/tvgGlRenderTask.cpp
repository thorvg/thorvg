#include "tvgGlCommon.h"
#include "tvgGlShaderSrc.h"
#include "tvgGlRenderTask.h"
#include "tvgGlPropertyInterface.h"

#include <memory>
#include <string>
#include <GLES2/gl2.h>


GlRenderTask::GlRenderTask(RenderTypes renderType, shared_ptr<GlShader> shader)
{
    mRenderType = renderType;
    mProgram = GlProgram::gen(shader);
    load();

    VertexProperty* prop = nullptr;
    ADD_ATTRIBUTE_PROPERTY(prop, this, mProgram, "aLocation", FORMAT_SIZE_VEC_4, mLocVertexAttribute);
}


GlRenderTask::RenderTypes GlRenderTask::getRenderType()
{
    return mRenderType;
}


void GlRenderTask::load()
{
    mProgram->load();
}


void GlRenderTask::unload()
{
    GlProgram::unload();
}


std::shared_ptr<GlProgram> GlRenderTask::getProgram()
{
    return mProgram;
}


std::map<int32_t, VertexProperty>& GlRenderTask::getAttributeVertexProperty()
{
    return mAttributePropertyBuffer;
}


std::map<int32_t, VertexProperty>& GlRenderTask::getUniformVertexProperty()
{
    return mUniformPropertyBuffer;
}


int32_t GlRenderTask::getLocationPropertyId() const
{
    return mLocVertexAttribute;
}


void GlRenderTask::uploadValues()
{
    for (auto& property : mUniformPropertyBuffer)
    {
        PropertyValue& propertyVal = property.second.propertyValues;
        switch (property.second.dataType) {
            case VertexProperty::DataType::INT: {
                switch (propertyVal.getStride()) {
                    case 1:
                        mProgram->setUniform1Value(property.second.propertyId, propertyVal.getCount(), (const int*)propertyVal.getData());
                        break;
                    case 2:
                        mProgram->setUniform2Value(property.second.propertyId, propertyVal.getCount(), (const int*)propertyVal.getData());
                        break;
                    case 3:
                        mProgram->setUniform3Value(property.second.propertyId, propertyVal.getCount(), (const int*)propertyVal.getData());
                        break;
                    case 4:
                        mProgram->setUniform4Value(property.second.propertyId, propertyVal.getCount(), (const int*)propertyVal.getData());
                    default: break;
                }
                break;
            }
            case VertexProperty::DataType::FLOAT: {
                switch (propertyVal.getStride()) {
                    case 1:
                        mProgram->setUniform1Value(property.second.propertyId, propertyVal.getCount(), propertyVal.getData());
                        break;
                    case 2:
                        mProgram->setUniform2Value(property.second.propertyId, propertyVal.getCount(), propertyVal.getData());
                        break;
                    case 3:
                        mProgram->setUniform3Value(property.second.propertyId, propertyVal.getCount(), propertyVal.getData());
                        break;
                    case 4:
                        mProgram->setUniform4Value(property.second.propertyId, propertyVal.getCount(), propertyVal.getData());
                    default: break;
                }
                break;
            }
        }
    }
}


std::shared_ptr<GlColorRenderTask> GlColorRenderTask::gen()
{
    return std::make_shared<GlColorRenderTask>();
}


GlColorRenderTask::GlColorRenderTask()
    :GlRenderTask(GlRenderTask::RenderTypes::RT_Color, GlShader::gen(COLOR_VERT_SHADER, COLOR_FRAG_SHADER))
{
    VertexProperty* prop = nullptr;
    ADD_UNIFORM_PROPERTY(prop, this, getProgram(), "uColor", FORMAT_SIZE_VEC_4, mLocColor);
}


void GlColorRenderTask::setColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    if (mLocColor != -1)
    {
        PropertyInterface::setProperty(this, mLocColor, r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
    }
}


GlGradientRenderTask::GlGradientRenderTask(GlRenderTask::RenderTypes renderType, std::shared_ptr<GlShader> shader)
    :GlRenderTask(renderType, shader)
{
    VertexProperty* prop = nullptr;
    ADD_UNIFORM_PROPERTY(prop, this, getProgram(), "uSize", FORMAT_SIZE_VEC_2, mLocPrimitiveSize);
    ADD_UNIFORM_PROPERTY(prop, this, getProgram(), "uCanvasSize", FORMAT_SIZE_VEC_2, mLocCanvasSize);
    ADD_UNIFORM_PROPERTY(prop, this, getProgram(), "noise_level", FORMAT_SIZE_FLOAT, mLocNoise);
    ADD_UNIFORM_PROPERTY(prop, this, getProgram(), "nStops", FORMAT_SIZE_FLOAT, mLocStopCnt);
    ADD_UNIFORM_PROPERTY(prop, this, getProgram(), "stopPoints", FORMAT_SIZE_FLOAT, mLocStops);
    ADD_UNIFORM_PROPERTY(prop, this, getProgram(), "stopColors", FORMAT_SIZE_VEC_4, mLocStopColors);
}


void GlGradientRenderTask::setPrimitveSize(float width, float height)
{
    if (mLocPrimitiveSize != -1)
    {
        PropertyInterface::setProperty(this, mLocPrimitiveSize, width, height);
    }
}


void GlGradientRenderTask::setCanvasSize(float width, float height)
{
    if (mLocCanvasSize != -1)
    {
        PropertyInterface::setProperty(this, mLocCanvasSize, width, height);
    }
}


void GlGradientRenderTask::setNoise(float noise)
{
    if (mLocNoise != -1)
    {
        PropertyInterface::setProperty(this, mLocNoise, noise);
    }
}


void GlGradientRenderTask::setStopCount(int32_t count)
{
    if (mLocStopCnt != -1)
    {
        PropertyInterface::setProperty(this, mLocStopCnt, (float)count);
    }
}

void GlGradientRenderTask::setStopColor(int index, float stopVal, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    if (index < MAX_GRADIENT_STOPS && mLocStops != -1 && mLocStopColors != -1)
    {
        PropertyInterface::setProperty(this, mLocStops, stopVal);
        PropertyInterface::setProperty(this, mLocStopColors, r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
    }
}


std::shared_ptr<GlLinearGradientRenderTask> GlLinearGradientRenderTask::gen()
{
    return std::make_shared<GlLinearGradientRenderTask>();
}


GlLinearGradientRenderTask::GlLinearGradientRenderTask()
    :GlGradientRenderTask(GlRenderTask::RenderTypes::RT_LinGradient, GlShader::gen(GRADIENT_VERT_SHADER, LINEAR_GRADIENT_FRAG_SHADER))
{
    VertexProperty* prop = nullptr;
    ADD_UNIFORM_PROPERTY(prop, this, getProgram(), "gradStartPos", FORMAT_SIZE_VEC_2, mLocStartPos);
    ADD_UNIFORM_PROPERTY(prop, this, getProgram(), "gradEndPos", FORMAT_SIZE_VEC_2, mLocEndPos);
}


void GlLinearGradientRenderTask::setStartPosition(float posX, float posY)
{
    if (mLocStartPos != -1)
    {
        PropertyInterface::setProperty(this, mLocStartPos, posX, posY);
    }
}


void GlLinearGradientRenderTask::setEndPosition(float posX, float posY)
{
    if (mLocEndPos != -1)
    {
        PropertyInterface::setProperty(this, mLocEndPos, posX, posY);
    }
}


std::shared_ptr<GlRadialGradientRenderTask> GlRadialGradientRenderTask::gen()
{
    return std::make_shared<GlRadialGradientRenderTask>();
}


GlRadialGradientRenderTask::GlRadialGradientRenderTask()
    :GlGradientRenderTask(GlRenderTask::RenderTypes::RT_RadGradient, GlShader::gen(GRADIENT_VERT_SHADER, RADIAL_GRADIENT_FRAG_SHADER))
{
    VertexProperty* prop = nullptr;
    ADD_UNIFORM_PROPERTY(prop, this, getProgram(), "gradStartPos", FORMAT_SIZE_VEC_2, mLocStartPos);
    ADD_UNIFORM_PROPERTY(prop, this, getProgram(), "stRadius", FORMAT_SIZE_FLOAT, mLocStRadius);
}


GlRadialGradientRenderTask::~GlRadialGradientRenderTask()
{
}


void GlRadialGradientRenderTask::setStartPosition(float posX, float posY)
{
    if (mLocStartPos != -1)
    {
        PropertyInterface::setProperty(this, mLocStartPos, posX, posY);
    }
}


void GlRadialGradientRenderTask::setStartRadius(float radius)
{
    if (mLocStRadius != -1)
    {
        PropertyInterface::setProperty(this, mLocStRadius, radius);
    }
}


void GlRadialGradientRenderTask::setEndRadius(float radius)
{
    if (mLocEdRadius != -1)
    {
        PropertyInterface::setProperty(this, mLocEdRadius, radius);
    }
}
