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
private: \
    struct Impl; \
    std::unique_ptr<Impl> pImpl; \
    A(const A&) = delete; \
    const A& operator=(const A&) = delete; \
    A()

#define _TIZENVG_DISABLE_CTOR(A) \
    A() = delete; \
    ~A() = delete

namespace tvg
{

enum class TIZENVG_EXPORT PathCommand { Close, MoveTo, LineTo, CubicTo };

class RenderMethod;

struct Point
{
    float x, y;
};


/**
 * @class PaintNode
 *
 * @ingroup TizenVG
 *
 * @brief description...
 *
 */
class TIZENVG_EXPORT PaintNode
{
public:
    virtual ~PaintNode() {}
    virtual int update(RenderMethod*) = 0;
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
    virtual int push(std::unique_ptr<PaintNode> paint) noexcept;
    virtual int clear() noexcept;
    virtual int update() noexcept;
    virtual int draw(bool async = true) noexcept;
    virtual int sync() = 0;

    RenderMethod* engine() noexcept;

    _TIZENVG_DECLARE_PRIVATE(Canvas);
};


/**
 * @class ShapeNode
 *
 * @ingroup TizenVG
 *
 * @brief description...
 *
 */
class TIZENVG_EXPORT ShapeNode final : public PaintNode
{
public:
    ~ShapeNode();

    int update(RenderMethod* engine) noexcept override;
    int clear() noexcept;

    int appendRect(float x, float y, float w, float h, float cornerRadius) noexcept;
    int appendCircle(float cx, float cy, float radiusW, float radiusH) noexcept;

    int fill(size_t r, size_t g, size_t b, size_t a) noexcept;

    int pathCommands(const PathCommand** cmds) const noexcept;
    int pathCoords(const Point** pts) const noexcept;
    int fill(size_t* r, size_t* g, size_t* b, size_t* a) const noexcept;

    static std::unique_ptr<ShapeNode> gen() noexcept;

    //FIXME: Ugly... Better design?
    void *engine() noexcept;

    _TIZENVG_DECLARE_PRIVATE(ShapeNode);
};


/**
 * @class SceneNode
 *
 * @ingroup TizenVG
 *
 * @brief description...
 *
 */
class TIZENVG_EXPORT SceneNode final : public PaintNode
{
public:
    ~SceneNode();

    int update(RenderMethod* engine) noexcept override;

    int push(std::unique_ptr<ShapeNode> shape) noexcept;

    static std::unique_ptr<SceneNode> gen() noexcept;

    _TIZENVG_DECLARE_PRIVATE(SceneNode);
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

    int target(uint32_t* buffer, size_t stride, size_t height) noexcept;
    int sync() noexcept override;
    static std::unique_ptr<SwCanvas> gen(uint32_t* buffer = nullptr, size_t stride = 0, size_t height = 0) noexcept;

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
