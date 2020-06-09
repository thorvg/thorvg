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

enum class TIZENVG_EXPORT Result { Success = 0, InvalidArguments, InsufficientCondition, FailedAllocation, MemoryCorruption, Unknown };
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

    virtual Result rotate(float degree) = 0;
    virtual Result scale(float factor) = 0;
    virtual Result translate(float x, float y) = 0;

    virtual Result bounds(float* x, float* y, float* w, float* h) const = 0;
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

    Result reserve(size_t n) noexcept;
    virtual Result push(std::unique_ptr<Paint> paint) noexcept;
    virtual Result clear() noexcept;
    virtual Result update() noexcept;
    virtual Result update(Paint* paint) noexcept;
    virtual Result draw(bool async = true) noexcept;
    virtual Result sync() = 0;

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

    Result reset() noexcept;

    //Path
    Result moveTo(float x, float y) noexcept;
    Result lineTo(float x, float y) noexcept;
    Result cubicTo(float cx1, float cy1, float cx2, float cy2, float x, float y) noexcept;
    Result close() noexcept;

    //Shape
    Result appendRect(float x, float y, float w, float h, float cornerRadius) noexcept;
    Result appendCircle(float cx, float cy, float radiusW, float radiusH) noexcept;
    Result appendPath(const PathCommand* cmds, size_t cmdCnt, const Point* pts, size_t ptsCnt) noexcept;

    //Stroke
    Result stroke(float width) noexcept;
    Result stroke(uint8_t r, uint8_t g, uint8_t b, uint8_t a) noexcept;
    Result stroke(const float* dashPattern, size_t cnt) noexcept;
    Result stroke(StrokeCap cap) noexcept;
    Result stroke(StrokeJoin join) noexcept;

    //Fill
    Result fill(uint8_t r, uint8_t g, uint8_t b, uint8_t a) noexcept;

    //Transform
    Result rotate(float degree) noexcept override;
    Result scale(float factor) noexcept override;
    Result translate(float x, float y) noexcept override;

    //Getters
    size_t pathCommands(const PathCommand** cmds) const noexcept;
    size_t pathCoords(const Point** pts) const noexcept;
    Result fill(uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* a) const noexcept;
    Result bounds(float* x, float* y, float* w, float* h) const noexcept override;

    float strokeWidth() const noexcept;
    Result strokeColor(uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* a) const noexcept;
    size_t strokeDash(const float** dashPattern) const noexcept;
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

    Result push(std::unique_ptr<Paint> shape) noexcept;
    Result reserve(size_t size) noexcept;

    Result rotate(float degree) noexcept override;
    Result scale(float factor) noexcept override;
    Result translate(float x, float y) noexcept override;

    Result bounds(float* x, float* y, float* w, float* h) const noexcept override;

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

    Result target(uint32_t* buffer, size_t stride, size_t w, size_t h) noexcept;
    Result sync() noexcept override;
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

    Result target(uint32_t* buffer, size_t stride, size_t w, size_t h) noexcept;
    Result sync() noexcept override;
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
    static Result init() noexcept;
    static Result term() noexcept;

    _TIZENVG_DISABLE_CTOR(Engine);
};

} //namespace

#ifdef __cplusplus
}
#endif

#endif //_TIZENVG_H_
