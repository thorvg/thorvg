#ifndef _THORVG_H_
#define _THORVG_H_

#include <memory>

#ifdef TVG_BUILD
    #define TVG_EXPORT __attribute__ ((visibility ("default")))
#else
    #define TVG_EXPORT
#endif

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


/**
 * @brief ...
 */
enum class TVG_EXPORT Result { Success = 0, InvalidArguments, InsufficientCondition, FailedAllocation, MemoryCorruption, NonSupport, Unknown };

/**
 * @brief ...
 */
enum class TVG_EXPORT PathCommand { Close = 0, MoveTo, LineTo, CubicTo };

/**
 * @brief Enumeration for The cap style to be used for stroking the path.
 */
enum class TVG_EXPORT StrokeCap
{
    Square = 0, ///< The end of lines is rendered as a square around the last point.
    Round,      ///< The end of lines is rendered as a half-circle around the last point.
    Butt        ///< The end of lines is rendered as a full stop on the last point itself.
};

/**
 * @brief Enumeration for The join style to be used for stroking the path.
 */
enum class TVG_EXPORT StrokeJoin
{
    Bevel = 0, ///< Used to render beveled line joins. The outer corner of the joined lines is filled by enclosing the triangular region of the corner with a straight line between the outer corners of each stroke.
    Round,     ///< Used to render rounded line joins. Circular arcs are used to join two lines smoothly.
    Miter      ///< Used to render mitered line joins. The intersection of the strokes is clipped at a line perpendicular to the bisector of the angle between the strokes, at the distance from the intersection of the segments equal to the product of the miter limit value and the border radius.  This prevents long spikes being created.
};

/**
 * @brief ...
 */
enum class TVG_EXPORT FillSpread { Pad = 0, Reflect, Repeat };

/**
 * @brief Enumeration for The fill rule of shape.
 */
enum class TVG_EXPORT FillRule
{
    Winding = 0, ///< Draw a horizontal line from the point to a location outside the shape. Determine whether the direction of the line at each intersection point is up or down. The winding number is determined by summing the direction of each intersection. If the number is non zero, the point is inside the shape.
    EvenOdd     ///< Draw a horizontal line from the point to a location outside the shape, and count the number of intersections. If the number of intersections is an odd number, the point is inside the shape.
};

/**
 * @brief ...
 */
enum class TVG_EXPORT CompositeMethod { None = 0, ClipPath, AlphaMask, InvAlphaMask };

/**
 * @brief ...
 */
enum class TVG_EXPORT CanvasEngine { Sw = (1 << 1), Gl = (1 << 2)};


/**
 * @brief ...
 */
struct Point
{
    float x, y;
};


/**
 * @brief ...
 */
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
 * @brief Paint is a object class for drawing a vector primitive.
 *
 */
class TVG_EXPORT Paint
{
public:
    virtual ~Paint();

    /**
     * @brief Set the angle of rotation transformation.
     * @param[in] degree The degree value of angle.
     * @return Returns True when it's successful. False otherwise.
     */
    Result rotate(float degree) noexcept;

    /**
     * @brief Set the scale value of scale transformation.
     * @param[in] factor The scale factor value.
     * @return Returns True when it's successful. False otherwise.
     */
    Result scale(float factor) noexcept;

    /**
     * @brief Set the x, y movement value of translate transformation.
     * @param[in] x The x axis movement value.
     * @param[in] y The y-axis movement value.
     * @return Returns True when it's successful. False otherwise.
     */
    Result translate(float x, float y) noexcept;

    /**
     * @brief Set the matrix value for affine transform.
     * @param[in] m The 3x3 matrix value.
     * @return Returns True when it's successful. False otherwise.
     */
    Result transform(const Matrix& m) noexcept;

    /**
     * @brief ...
     * @param[in] x ...
     * @param[in] y ...
     * @param[in] w ...
     * @param[in] h ...
     * @return Returns True when it's successful. False otherwise.
     */
    Result bounds(float* x, float* y, float* w, float* h) const noexcept;

    /**
     * @brief Set the transparency value
     * @param[in] o The transparency level [0 ~ 255], 0 means totally transparent, while 255 means opaque.
     * @return Returns True when it's successful. False otherwise.
     */
    Result opacity(uint8_t o) noexcept;

    /**
     * @brief ...
     * @return ...
     */
    Paint* duplicate() const noexcept;

    /**
     * @brief ...
     * @param[in] target ...
     * @param[in] method ...
     * @return Returns True when it's successful. False otherwise.
     */
    Result composite(std::unique_ptr<Paint> target, CompositeMethod method) const noexcept;

    /**
     * @brief Get the transparency value.
     * @return Returns the transparency level.
     */
    uint8_t opacity() const noexcept;

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
    /**
     * @brief ...
     */
    struct ColorStop
    {
        float offset;
        uint8_t r, g, b, a;
    };

    virtual ~Fill();

    /**
     * @brief ...
     * @param[in] colorStops ...
     * @param[in] cnt ...
     * @return Returns True when it's successful. False otherwise.
     */
    Result colorStops(const ColorStop* colorStops, uint32_t cnt) noexcept;

    /**
     * @brief ...
     * @param[in] s ...
     * @return Returns True when it's successful. False otherwise.
     */
    Result spread(FillSpread s) noexcept;

    /**
     * @brief ...
     * @param[in] colorStops ...
     * @return ...
     */
    uint32_t colorStops(const ColorStop** colorStops) const noexcept;

    /**
     * @brief ...
     * @return ...
     */
    FillSpread spread() const noexcept;

    /**
     * @brief ...
     * @return ...
     */
    Fill* duplicate() const noexcept;

    _TVG_DECALRE_IDENTIFIER();
    _TVG_DECLARE_PRIVATE(Fill);
};


/**
 * @class Canvas
 *
 * @ingroup ThorVG
 *
 * @brief CanvasView is a class for displaying an vector primitives. 
 *
 */
class TVG_EXPORT Canvas
{
public:
    Canvas(RenderMethod*);
    virtual ~Canvas();

    Result reserve(uint32_t n) noexcept;
    /**
     * @brief Add paint object to the canvas
     * This method is similar to registration. The added paint is drawn on the inner canvas.
     */
    virtual Result push(std::unique_ptr<Paint> paint) noexcept;

    /**
     * @brief ...
     */
    virtual Result clear(bool free = true) noexcept;

    /**
     * @brief ...
     */
    virtual Result update(Paint* paint) noexcept;
    /**
     * @brief Draw target buffer added to the canvas.
     */
    virtual Result draw() noexcept;

    /**
     * @brief ...
     */
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

    /**
     * @brief ...
     */
    Result linear(float x1, float y1, float x2, float y2) noexcept;

    /**
     * @brief ...
     */
    Result linear(float* x1, float* y1, float* x2, float* y2) const noexcept;

    /**
     * @brief ...
     */
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

    /**
     * @brief ...
     */
    Result radial(float cx, float cy, float radius) noexcept;

    /**
     * @brief ...
     */
    Result radial(float* cx, float* cy, float* radius) const noexcept;

    /**
     * @brief ...
     */
    static std::unique_ptr<RadialGradient> gen() noexcept;

    _TVG_DECLARE_PRIVATE(RadialGradient);
};



/**
 * @class Shape
 *
 * @ingroup ThorVG
 *
 * @brief Shape is a command list for drawing one shape groups
 * It has own path data & properties for sync/asynchronous drawing
 *
 */
class TVG_EXPORT Shape final : public Paint
{
public:
    ~Shape();

    /**
     * @brief Reset the added path(rect, circle, path, etc...) information.
     * Color and Stroke information are keeped.
     * @return Returns True when it's successful. False otherwise.
     */
    Result reset() noexcept;

    /**
     * @brief Add a point that sets the given point as the current point,
     * implicitly starting a new subpath and closing the previous one.
     * @param[in] x X co-ordinate of the current point.
     * @param[in] t Y co-ordinate of the current point.
     * @return Returns True when it's successful. False otherwise.
     */
    Result moveTo(float x, float y) noexcept;

    /**
     * @brief Adds a straight line from the current position to the given end point.
     * After the line is drawn, the current position is updated to be at the
     * end point of the line.
     * If no current position present, it draws a line to itself, basically * a point.
     * @param[in] x X co-ordinate of end point of the line.
     * @param[in] y Y co-ordinate of end point of the line.
     * @return Returns True when it's successful. False otherwise.
     */
    Result lineTo(float x, float y) noexcept;

    /**
     * @brief Adds a cubic Bezier curve between the current position and the
     * given end point (x, y) using the control points specified by
     * (cx1, cy1), and (cx2, cy2). After the path is drawn,
     * the current position is updated to be at the end point of the path.
     * @param[in] cx1 X co-ordinate of 1st control point.
     * @param[in] cy1 Y co-ordinate of 1st control point.
     * @param[in] cx2 X co-ordinate of 2nd control point.
     * @param[in] cy2 Y co-ordinate of 2nd control point.
     * @param[in] x X co-ordinate of end point of the line.
     * @param[in] t Y co-ordinate of end point of the line.
     * @return Returns True when it's successful. False otherwise.
     */
    Result cubicTo(float cx1, float cy1, float cx2, float cy2, float x, float y) noexcept;

    /**
     * @brief Closes the current subpath by drawing a line to the beginning of the
     * subpath, automatically starting a new path. The current point of the
     * new path is (0, 0).
     * If the subpath does not contain any points, this function does nothing.
     * @return Returns True when it's successful. False otherwise.
     */
    Result close() noexcept;

    /**
     * @brief Append the given rectangle with rounded corner to the path.
     * The rx, ry arguments specify the radii of the ellipses defining the
     * corners of the rounded rectangle.
     *
     * rx, ry are specified in terms of width and height respectively.
     *
     * If rx, ry values are 0, then it will draw a rectangle without rounded corner.
     *
     * @param[in] x The x-axis of the rectangle.
     * @param[in] y The y-axis of the rectangle.
     * @param[in] w The width of the rectangle.
     * @param[in] h The height of the rectangle.
     * @param[in] rx The radius of the rounded corner and should be in range [ 0 to w/2 ]
     * @param[in] ry The radius of the rounded corner and should be in range [ 0 to h/2 ]
     * @return Returns True when it's successful. False otherwise.
     */
    Result appendRect(float x, float y, float w, float h, float rx, float ry) noexcept;

    /**
     * @brief Append a circle with given center and x,y-axis radius.
     * @param[in] cx X co-ordinate of the center of the circle.
     * @param[in] ct Y co-ordinate of the center of the circle.
     * @param[in] rx X co-ordinate of radius of the circle.
     * @param[in] ry Y co-ordinate of radius of the circle.
     * @return Returns True when it's successful. False otherwise.
     */
    Result appendCircle(float cx, float cy, float rx, float ry) noexcept;

    /**
     * @brief Append the arcs.
     * @param[in] cx X co-ordinate of the center of the arc.
     * @param[in] ct Y co-ordinate of the center of the arc.
     * @param[in] radius Radius of the arc.
     * @param[in] startAngle Start angle (in degrees) where the arc begins.
     * @param[in] sweep The Angle measures how long the arc will be drawn.
     * @param[in] pie If True, the area is created by connecting start angle point and sweep angle point of the drawn arc. If false, it doesn't.
     * @return Returns True when it's successful. False otherwise.
     */
    Result appendArc(float cx, float cy, float radius, float startAngle, float sweep, bool pie) noexcept;

    /**
     * @brief ...
     */
    Result appendPath(const PathCommand* cmds, uint32_t cmdCnt, const Point* pts, uint32_t ptsCnt) noexcept;

    /**
     * @brief Set the stroke width to use for stroking the path.
     * @param[in] width Stroke width to be used.
     * @return Returns True when it's successful. False otherwise.
     */
    Result stroke(float width) noexcept;

    /**
     * @brief Set the color to use for stroking the path.
     * @param[in] r The red stroking color.
     * @param[in] g The grenn stroking color.
     * @param[in] b The blue stroking color.
     * @param[in] a The alpha stroking color.
     * @return Returns True when it's successful. False otherwise.
     */
    Result stroke(uint8_t r, uint8_t g, uint8_t b, uint8_t a) noexcept;

    /**
     * @brief ...
     */
    Result stroke(std::unique_ptr<Fill> f) noexcept;

    /**
     * @brief Sets the stroke dash pattern. The dash pattern is specified dash pattern.
     * @param[in] dashPattern Lenght and a gap array.
     * @param[in] cnt The length of dash pattern array.
     * @return Returns True when it's successful. False otherwise.
     */
    Result stroke(const float* dashPattern, uint32_t cnt) noexcept;

    /**
     * @brief Set the cap style to use for stroking the path. The cap will be used for capping the end point of a open subpath.
     * @param[in] cap Cap style to use.
     * @return Returns True when it's successful. False otherwise.
     */
    Result stroke(StrokeCap cap) noexcept;

    /**
     * @brief Set the join style to use for stroking the path.
     * The join style will be used for joining the two line segment while stroking the path.
     * @param[in] join Join style to use.
     * @return Returns True when it's successful. False otherwise.
     */
    Result stroke(StrokeJoin join) noexcept;

    /**
     * @brief Set the color to use for filling the path.
     * @param[in] r The red color value.
     * @param[in] g The green color value.
     * @param[in] b The blue color value.
     * @param[in] a The alpha color value.
     * @return Returns True when it's successful. False otherwise.
     */
    Result fill(uint8_t r, uint8_t g, uint8_t b, uint8_t a) noexcept;

    /**
     * @brief ...
     */
    Result fill(std::unique_ptr<Fill> f) noexcept;

    /**
     * @brief Set the fill rule.
     * @param[in] r The current fill rule of the shape.
     * @return Returns True when it's successful. False otherwise.
     */
    Result fill(FillRule r) noexcept;

    /**
     * @brief ...
     */
    uint32_t pathCommands(const PathCommand** cmds) const noexcept;

    /**
     * @brief ...
     */
    uint32_t pathCoords(const Point** pts) const noexcept;

    /**
     * @brief ...
     */
    const Fill* fill() const noexcept;

    /**
     * @brief Get the color to use for filling the path.
     * @param[out] r The red color value.
     * @param[out] g The green color value.
     * @param[out] b The blue color value.
     * @param[out] a The alpha color value.
     * @return Returns The color value.
     */
    Result fillColor(uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* a) const noexcept;

    /**
     * @brief Get the fill rule.
     * @return Returns the current fill rule of the shape.
     */
    FillRule fillRule() const noexcept;

    /**
     * @brief Get the stroke width to use for stroking the path.
     * @return Returns stroke width to be used.
     */
    float strokeWidth() const noexcept;
    /**
     * @brief Get the color to use for stroking the path.
     * @param[out] r The red stroking color value.
     * @param[out] g The green stroking color value.
     * @param[out] b The blue stroking color value.
     * @param[out] a The alpha stroking color value.
     * @return Returns the stroking color.
     */
    Result strokeColor(uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* a) const noexcept;

    /**
     * @brief ...
     */
    const Fill* strokeFill() const noexcept;

    /**
     * @brief Gets the stroke dash pattern.
     * @param[out] dashPattern Returns the stroke dash pattern array.
     * @return The lenght of dash pattern array.
     */
    uint32_t strokeDash(const float** dashPattern) const noexcept;

    /**
     * @brief Get the cap style to use for stroking the path.
     * @return Returns the cap style.
     */
    StrokeCap strokeCap() const noexcept;

    /**
     * @brief Get the join style to use for stroking the path.
     * @return Returns join style to use.
     */
    StrokeJoin strokeJoin() const noexcept;

    /**
     * @brief ...
     * @return ...
     */
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

    /**
     * @brief ...
     */
    Result load(const std::string& path) noexcept;

    /**
     * @brief ...
     */
    Result load(const char* data, uint32_t size) noexcept;

    /**
     * @brief ...
     */
    Result load(uint32_t* data, uint32_t w, uint32_t h, bool copy) noexcept;

    /**
     * @brief ...
     */
    //TODO: Replace with size(). Remove API
    Result viewbox(float* x, float* y, float* w, float* h) const noexcept;

    /**
     * @brief ...
     */
    Result size(float w, float h) noexcept;

    /**
     * @brief ...
     */
    Result size(float* w, float* h) const noexcept;

    /**
     * @brief ...
     */
    const uint32_t* data() const noexcept;

    /**
     * @brief ...
     */
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

    /**
     * @brief ...
     */
    Result push(std::unique_ptr<Paint> paint) noexcept;

    /**
     * @brief ...
     */
    Result reserve(uint32_t size) noexcept;

    /**
     * @brief ...
     */
    Result clear() noexcept;

    /**
     * @brief ...
     */
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

    /**
     * @brief ...
     */
    enum Colorspace { ABGR8888 = 0, ARGB8888 };

    /**
     * @brief ...
     */
    Result target(uint32_t* buffer, uint32_t stride, uint32_t w, uint32_t h, Colorspace cs) noexcept;

    /**
     * @brief ...
     */
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

    /**
     * @brief ...
     */
    //TODO: Gl Specific methods. Need gl backend configuration methods as well.
    Result target(uint32_t* buffer, uint32_t stride, uint32_t w, uint32_t h) noexcept;

    /**
     * @brief ...
     */
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

    /**
     * @brief ...
     */
    static Result term(CanvasEngine engine) noexcept;

    _TVG_DISABLE_CTOR(Initializer);
};

} //namespace

#ifdef __cplusplus
}
#endif

#endif //_THORVG_H_
