#ifndef _TVG_GL_RENDER_TASK_H_
#define _TVG_GL_RENDER_TASK_H_

#include "tvgGlShader.h"
#include "tvgGlProgram.h"
#include "tvgGlRendererProperties.h"

#include <string>
#include <memory>
#include <map>

#define MAX_GRADIENT_STOPS 4

class GlRenderTask
{
public:
    enum RenderTypes
    {
        RT_Color = 0,
        RT_LinGradient,
        RT_RadGradient,

        RT_None,
    };

    GlRenderTask(RenderTypes renderType, std::shared_ptr<GlShader> shader);
    RenderTypes getRenderType();

    void load();
    static void unload();
    std::shared_ptr<GlProgram> getProgram();
    std::map<int32_t, VertexProperty>& getAttributeVertexProperty();
    std::map<int32_t, VertexProperty>& getUniformVertexProperty();
    int32_t getLocationPropertyId() const;
    void uploadValues();

private:
    RenderTypes mRenderType;

    uint32_t    propertyFormatSize;
    std::shared_ptr<GlProgram> mProgram;
    std::map<int32_t, VertexProperty> mAttributePropertyBuffer;
    std::map<int32_t, VertexProperty> mUniformPropertyBuffer;

    int32_t mLocVertexAttribute;
};

class GlColorRenderTask : public GlRenderTask
{
public:
    static std::shared_ptr<GlColorRenderTask> gen();
    GlColorRenderTask();
    void setColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a);

private:
    int32_t mLocColor = -1;
};

class GlGradientRenderTask : public GlRenderTask
{
public:
    GlGradientRenderTask(GlRenderTask::RenderTypes renderType, std::shared_ptr<GlShader> shader);
    void setPrimitveSize(float width, float height);
    void setCanvasSize(float width, float height);
    void setNoise(float noise);
    void setStopCount(int32_t count);
    void setStopColor(int index, float stopVal, uint8_t r, uint8_t g, uint8_t b, uint8_t a);

private:
    int32_t mLocPrimitiveSize = -1;
    int32_t mLocCanvasSize = -1;
    int32_t mLocNoise = -1;
    int32_t mLocStopCnt = -1;
    int32_t mLocStops;
    int32_t mLocStopColors;
};

class GlLinearGradientRenderTask : public GlGradientRenderTask
{
public:
    static std::shared_ptr<GlLinearGradientRenderTask> gen();
    GlLinearGradientRenderTask();
    void setStartPosition(float posX, float posY);
    void setEndPosition(float posX, float posY);

private:
    int32_t mLocStartPos = -1;
    int32_t mLocEndPos = -1;
};

class GlRadialGradientRenderTask : public GlGradientRenderTask
{
public:
    static std::shared_ptr<GlRadialGradientRenderTask> gen();
    GlRadialGradientRenderTask();
    ~GlRadialGradientRenderTask();
    void setStartPosition(float posX, float posY);
    void setStartRadius(float radius);
    void setEndRadius(float radius);

private:
    int32_t mLocStartPos = -1;
    int32_t mLocStRadius = -1;
    int32_t mLocEdRadius = -1;
};


#endif /* _TVG_GL_RENDER_TASK_H_ */
