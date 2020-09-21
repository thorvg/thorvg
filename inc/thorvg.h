#ifndef _THORVG_H_
#define _THORVG_H_

#include <memory>

#ifdef TVG_BUILD
    #define TVG_EXPORT __attribute__ ((visibility ("default")))
#else
    #define TVG_EXPORT
#endif

#ifdef  LOG_TAG
#undef  LOG_TAG
#endif
#define LOG_TAG "TVG"


#ifdef __cplusplus
extern "C" {
#endif


#define _TVG_DECLARE_PRIVATE(A) \
protected: \
    struct Impl; \
    Impl* pImpl; \
    A(const A&) = delete; \
    const A& operator=(const A&) = delete; \
    A()

#define _TVG_DISABLE_CTOR(A) \
    A() = delete; \
    ~A() = delete

#define _TVG_DECLARE_ACCESSOR() \
    friend Canvas; \
    friend Scene; \
    friend Picture

#define _TVG_DECALRE_IDENTIFIER() \
    auto id() const { return _id; } \
protected: \
    unsigned _id

namespace tvg
{

class RenderMethod;
class Scene;
class Picture;
class Canvas;


enum class TVG_EXPORT Result { Success = 0, InvalidArguments, InsufficientCondition, FailedAllocation, MemoryCorruption, NonSupport, Unknown };
enum class TVG_EXPORT PathCommand { Close = 0, MoveTo, LineTo, CubicTo };
enum class TVG_EXPORT StrokeCap { Square = 0, Round, Butt };
enum class TVG_EXPORT StrokeJoin { Bevel = 0, Round, Miter };
enum class TVG_EXPORT FillSpread { Pad = 0, Reflect, Repeat };
enum class TVG_EXPORT CompMethod { None = 0, ClipPath };
enum class TVG_EXPORT CanvasEngine { Sw = (1 << 1), Gl = (1 << 2)};


struct Point
{
    float x, y;
};


struct Matrix
{
    float e11, e12, e13;
    float e21, e22, e23;
    float e31, e32, e33;
};


/**
 * @class Paint
 *
 * @ingroup ThorVG
 *
 * @brief description...
 *
 */
class TVG_EXPORT Paint
{
public:
    virtual ~Paint();

    Result rotate(float degree) noexcept;
    Result scale(float factor) noexcept;
    Result translate(float x, float y) noexcept;
    Result transform(const Matrix& m) noexcept;
    Matrix transform() noexcept;
    Result bounds(float* x, float* y, float* w, float* h) const noexcept;
    Paint* duplicate() const noexcept;

    Result composite(std::unique_ptr<Paint> comp, CompMethod methd) const noexcept;
    Paint* composite() const noexcept;
    CompMethod compositeMethod() const noexcept;

    _TVG_DECLARE_ACCESSOR();
    _TVG_DECLARE_PRIVATE(Paint);
};


/**
 * @class Fill
 *
 * @ingroup ThorVG
 *
 * @brief description...
 *
 */
class TVG_EXPORT Fill
{
public:
    struct ColorStop
    {
        float offset;
        uint8_t r, g, b, a;
    };

    virtual ~Fill();

    Result colorStops(const ColorStop* colorStops, uint32_t cnt) noexcept;
    Result spread(FillSpread s) noexcept;

    uint32_t colorStops(const ColorStop** colorStops) const noexcept;
    FillSpread spread() const noexcept;
    Fill* duplicate() const noexcept;

    _TVG_DECALRE_IDENTIFIER();
    _TVG_DECLARE_PRIVATE(Fill);
};


/**
 * @class Canvas
 *
 * @ingroup ThorVG
 *
 * @brief description...
 *
 */
class TVG_EXPORT Canvas
{
public:
    Canvas(RenderMethod*);
    virtual ~Canvas();

    Result reserve(uint32_t n) noexcept;
    virtual Result push(std::unique_ptr<Paint> paint) noexcept;
    virtual Result clear() noexcept;
    virtual Result update(Paint* paint) noexcept;
    virtual Result draw() noexcept;
    virtual Result sync() noexcept;

    _TVG_DECLARE_PRIVATE(Canvas);
};


/**
 * @class LinearGradient
 *
 * @ingroup ThorVG
 *
 * @brief description...
 *
 */
class TVG_EXPORT LinearGradient final : public Fill
{
public:
    ~LinearGradient();

    Result linear(float x1, float y1, float x2, float y2) noexcept;
    Result linear(float* x1, float* y1, float* x2, float* y2) const noexcept;

    static std::unique_ptr<LinearGradient> gen() noexcept;

    _TVG_DECLARE_PRIVATE(LinearGradient);
};


/**
 * @class RadialGradient
 *
 * @ingroup ThorVG
 *
 * @brief description...
 *
 */
class TVG_EXPORT RadialGradient final : public Fill
{
public:
    ~RadialGradient();

    Result radial(float cx, float cy, float radius) noexcept;
    Result radial(float* cx, float* cy, float* radius) const noexcept;

    static std::unique_ptr<RadialGradient> gen() noexcept;

    _TVG_DECLARE_PRIVATE(RadialGradient);
};



/**
 * @class Shape
 *
 * @ingroup ThorVG
 *
 * @brief description...
 *
 */
class TVG_EXPORT Shape final : public Paint
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
    Result appendRect(float x, float y, float w, float h, float rx, float ry) noexcept;
    Result appendCircle(float cx, float cy, float rx, float ry) noexcept;
    Result appendArc(float cx, float cy, float radius, float startAngle, float sweep, bool pie) noexcept;
    Result appendPath(const PathCommand* cmds, uint32_t cmdCnt, const Point* pts, uint32_t ptsCnt) noexcept;

    //Stroke
    Result stroke(float width) noexcept;
    Result stroke(uint8_t r, uint8_t g, uint8_t b, uint8_t a) noexcept;
    Result stroke(const float* dashPattern, uint32_t cnt) noexcept;
    Result stroke(StrokeCap cap) noexcept;
    Result stroke(StrokeJoin join) noexcept;

    //Fill
    Result fill(uint8_t r, uint8_t g, uint8_t b, uint8_t a) noexcept;
    Result fill(std::unique_ptr<Fill> f) noexcept;

    //Getters
    uint32_t pathCommands(const PathCommand** cmds) const noexcept;
    uint32_t pathCoords(const Point** pts) const noexcept;
    Result fill(uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* a) const noexcept;
    const Fill* fill() const noexcept;

    float strokeWidth() const noexcept;
    Result strokeColor(uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* a) const noexcept;
    uint32_t strokeDash(const float** dashPattern) const noexcept;
    StrokeCap strokeCap() const noexcept;
    StrokeJoin strokeJoin() const noexcept;

    static std::unique_ptr<Shape> gen() noexcept;

    _TVG_DECLARE_PRIVATE(Shape);
};


/**
 * @class Picture
 *
 * @ingroup ThorVG
 *
 * @brief description...
 *
 */
class TVG_EXPORT Picture final : public Paint
{
public:
    ~Picture();

    Result load(const std::string& path) noexcept;
    Result load(const char* data, uint32_t size) noexcept;
    Result viewbox(float* x, float* y, float* w, float* h) const noexcept;

    static std::unique_ptr<Picture> gen() noexcept;

    _TVG_DECLARE_PRIVATE(Picture);
};


/**
 * @class Scene
 *
 * @ingroup ThorVG
 *
 * @brief description...
 *
 */
class TVG_EXPORT Scene final : public Paint
{
public:
    ~Scene();

    Result push(std::unique_ptr<Paint> paint) noexcept;
    Result reserve(uint32_t size) noexcept;

    static std::unique_ptr<Scene> gen() noexcept;

    _TVG_DECLARE_PRIVATE(Scene);
};


/**
 * @class SwCanvas
 *
 * @ingroup ThorVG
 *
  @brief description...
 *
 */
class TVG_EXPORT SwCanvas final : public Canvas
{
public:
    ~SwCanvas();

    enum Colorspace { ABGR8888 = 0, ARGB8888 };

    Result target(uint32_t* buffer, uint32_t stride, uint32_t w, uint32_t h, Colorspace cs) noexcept;

    static std::unique_ptr<SwCanvas> gen() noexcept;

    _TVG_DECLARE_PRIVATE(SwCanvas);
};


/**
 * @class GlCanvas
 *
 * @ingroup ThorVG
 *
 * @brief description...
 *
 */
class TVG_EXPORT GlCanvas final : public Canvas
{
public:
    ~GlCanvas();

    //TODO: Gl Specific methods. Need gl backend configuration methods as well.
    Result target(uint32_t* buffer, uint32_t stride, uint32_t w, uint32_t h) noexcept;

    static std::unique_ptr<GlCanvas> gen() noexcept;

    _TVG_DECLARE_PRIVATE(GlCanvas);
};


/**
 * @class Engine
 *
 * @ingroup ThorVG
 *
 * @brief description...
 *
 */
class TVG_EXPORT Initializer final
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
    static Result init(CanvasEngine engine, uint32_t threads) noexcept;
    static Result term(CanvasEngine engine) noexcept;

    _TVG_DISABLE_CTOR(Initializer);
};

} //namespace

#ifdef __cplusplus
}
#endif

#endif //_THORVG_H_
