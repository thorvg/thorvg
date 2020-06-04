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
#ifndef _TIZENVG_H_
#define _TIZENVG_H_

#include <memory>

#ifdef TIZENVG_BUILD
    #define TIZENVG_EXPORT __attribute__ ((visibility ("default")))
#else
    #define TIZENVG_EXPORT
#endif

#ifdef  LOG_TAG
#undef  LOG_TAG
#endif
#define LOG_TAG "TIZENVG"


#ifdef __cplusplus
extern "C" {
#endif

#define _TIZENVG_DECLARE_PRIVATE(A) \
protected: \
    struct Impl; \
    std::unique_ptr<Impl> pImpl; \
    A(const A&) = delete; \
    const A& operator=(const A&) = delete; \
    A()

#define _TIZENVG_DECLARE_ACCESSOR(A) \
    friend A

#define _TIZENVG_DISABLE_CTOR(A) \
    A() = delete; \
    ~A() = delete

namespace tvg
{

enum class TIZENVG_EXPORT PathCommand { Close = 0, MoveTo, LineTo, CubicTo };
enum class TIZENVG_EXPORT StrokeCap { Square = 0, Round, Butt };
enum class TIZENVG_EXPORT StrokeJoin { Bevel = 0, Round, Miter };

class RenderMethod;
class Scene;

struct Point
{
    float x, y;
};


/**
 * @class Paint
 *
 * @ingroup TizenVG
 *
 * @brief description...
 *
 */
class TIZENVG_EXPORT Paint
{
public:
    virtual ~Paint() {}

    virtual int rotate(float degree) = 0;
    virtual int scale(float factor) = 0;
    virtual int translate(float x, float y) = 0;

    virtual int bounds(float* x, float* y, float* w, float* h) const = 0;
};


/**
 * @class Canvas
 *
 * @ingroup TizenVG
 *
 * @brief description...
 *
 */
class TIZENVG_EXPORT Canvas
{
public:
    Canvas(RenderMethod*);
    virtual ~Canvas();

    int reserve(size_t n) noexcept;
    virtual int push(std::unique_ptr<Paint> paint) noexcept;
    virtual int clear() noexcept;
    virtual int update() noexcept;
    virtual int update(Paint* paint) noexcept;
    virtual int draw(bool async = true) noexcept;
    virtual int sync() = 0;

    _TIZENVG_DECLARE_ACCESSOR(Scene);
    _TIZENVG_DECLARE_PRIVATE(Canvas);
};


/**
 * @class Shape
 *
 * @ingroup TizenVG
 *
 * @brief description...
 *
 */
class TIZENVG_EXPORT Shape final : public Paint
{
public:
    ~Shape();

    int reset() noexcept;

    //Path
    int moveTo(float x, float y) noexcept;
    int lineTo(float x, float y) noexcept;
    int cubicTo(float cx1, float cy1, float cx2, float cy2, float x, float y) noexcept;
    int close() noexcept;

    //Shape
    int appendRect(float x, float y, float w, float h, float cornerRadius) noexcept;
    int appendCircle(float cx, float cy, float radiusW, float radiusH) noexcept;
    int appendPath(const PathCommand* cmds, size_t cmdCnt, const Point* pts, size_t ptsCnt) noexcept;

    //Stroke
    int stroke(float width) noexcept;
    int stroke(uint8_t r, uint8_t g, uint8_t b, uint8_t a) noexcept;
    int stroke(const size_t* dashPattern, size_t cnt) noexcept;
    int stroke(StrokeCap cap) noexcept;
    int stroke(StrokeJoin join) noexcept;

    //Fill
    int fill(uint8_t r, uint8_t g, uint8_t b, uint8_t a) noexcept;

    //Transform
    int rotate(float degree) noexcept override;
    int scale(float factor) noexcept override;
    int translate(float x, float y) noexcept override;

    //Getters
    size_t pathCommands(const PathCommand** cmds) const noexcept;
    size_t pathCoords(const Point** pts) const noexcept;
    int fill(uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* a) const noexcept;
    int bounds(float* x, float* y, float* w, float* h) const noexcept override;

    float strokeWidth() const noexcept;
    int strokeColor(uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* a) const noexcept;
    size_t strokeDash(const size_t** dashPattern) const noexcept;
    StrokeCap strokeCap() const noexcept;
    StrokeJoin strokeJoin() const noexcept;

    static std::unique_ptr<Shape> gen() noexcept;

    _TIZENVG_DECLARE_ACCESSOR(Scene);
    _TIZENVG_DECLARE_ACCESSOR(Canvas);
    _TIZENVG_DECLARE_PRIVATE(Shape);
};


/**
 * @class Scene
 *
 * @ingroup TizenVG
 *
 * @brief description...
 *
 */
class TIZENVG_EXPORT Scene final : public Paint
{
public:
    ~Scene();

    int push(std::unique_ptr<Paint> shape) noexcept;
    int reserve(size_t size) noexcept;

    int rotate(float degree) noexcept override;
    int scale(float factor) noexcept override;
    int translate(float x, float y) noexcept override;

    int bounds(float* x, float* y, float* w, float* h) const noexcept override;

    static std::unique_ptr<Scene> gen() noexcept;

    _TIZENVG_DECLARE_PRIVATE(Scene);
    _TIZENVG_DECLARE_ACCESSOR(Canvas);
};


/**
 * @class SwCanvas
 *
 * @ingroup TizenVG
 *
  @brief description...
 *
 */
class TIZENVG_EXPORT SwCanvas final : public Canvas
{
public:
    ~SwCanvas();

    int target(uint32_t* buffer, size_t stride, size_t w, size_t h) noexcept;
    int sync() noexcept override;
    static std::unique_ptr<SwCanvas> gen() noexcept;

    _TIZENVG_DECLARE_PRIVATE(SwCanvas);
};


/**
 * @class GlCanvas
 *
 * @ingroup TizenVG
 *
 * @brief description...
 *
 */
class TIZENVG_EXPORT GlCanvas final : public Canvas
{
public:
    ~GlCanvas();

    //TODO: Gl Specific methods. Need gl backend configuration methods as well.

    int target(uint32_t* buffer, size_t stride, size_t w, size_t h) noexcept;
    int sync() noexcept override;
    static std::unique_ptr<GlCanvas> gen() noexcept;

    _TIZENVG_DECLARE_PRIVATE(GlCanvas);
};


/**
 * @class Engine
 *
 * @ingroup TizenVG
 *
 * @brief description...
 *
 */
class TIZENVG_EXPORT Engine final
{
public:
    /**
     * @brief ...
     *
     * @param[in] arg ...
     *
     * @note ...
     *
     * @return ...
     *
     * @see ...
     */
    static int init() noexcept;
    static int term() noexcept;

    _TIZENVG_DISABLE_CTOR(Engine);
};

} //namespace

#ifdef __cplusplus
}
#endif

#endif //_TIZENVG_H_
