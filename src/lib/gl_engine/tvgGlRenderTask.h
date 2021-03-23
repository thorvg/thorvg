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

#ifndef _TVG_GL_RENDER_TASK_H_
#define _TVG_GL_RENDER_TASK_H_

#include "tvgGlRendererProperties.h"

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
    int32_t getTransformLocationPropertyId() const;
    void setTransform(int count, float* transform_matrix);
    void uploadValues();

private:
    RenderTypes mRenderType;

    uint32_t    propertyFormatSize;
    std::shared_ptr<GlProgram> mProgram;
    std::map<int32_t, VertexProperty> mAttributePropertyBuffer;
    std::map<int32_t, VertexProperty> mUniformPropertyBuffer;

    int32_t mLocVertexAttribute = -1;
    int32_t mLocTransform = -1;
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
    int32_t getTransformLocationPropertyId() const;
private:
    int32_t mLocPrimitiveSize = -1;
    int32_t mLocCanvasSize = -1;
    int32_t mLocNoise = -1;
    int32_t mLocStopCnt = -1;
    int32_t mLocStops = -1;
    int32_t mLocStopColors = -1;
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
