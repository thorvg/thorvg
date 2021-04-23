/*!
* \file thorvg.h
*/


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
 * @brief Enumeration specifying the result from the APIs.
 */
enum class TVG_EXPORT Result
{
    Success = 0,           ///< The value returned in case of the correct request execution.
    InvalidArguments,      ///< The value returned in the event of a problem with the arguments given to the API - e.g. empty paths or null pointers.
    InsufficientCondition, ///< The value returned in case the request cannot be processed - e.g. asking for properties of an object, which does not exist.
    FailedAllocation,      ///< The value returned in case of unsuccessful memory allocation.
    MemoryCorruption,      ///< The value returned in the event of bad memory handling - e.g. failing in pointer releasing or casting
    NonSupport,            ///< The value returned in case of choosing unsupported options.
    Unknown                ///< The value returned in all other cases.
};

/**
 * @brief Enumeration specifying the values of the path commands accepted by ThorVG.
 *
 * Not to be confused with the path commands from the svg path element.
 */
enum class TVG_EXPORT PathCommand
{
    Close = 0, ///< Ends the current sub-path and connects it with its initial point - corresponds to Z command in the svg path commands.
    MoveTo,    ///< Sets a new initial point of the sub-path and a new current point - corresponds to M command in the svg path commands.
    LineTo,    ///< Draws a line from the current point to the given point and sets a new value of the current point - corresponds to L command in the svg path commands.
    CubicTo    ///< Draws a cubic Bezier curve from the current point to the given point using two given control points and sets a new value of the current point - corresponds to C command in the svg path commands.
};

/**
 * @brief Enumeration determining the ending type of a stroke in the open sub-paths.
 */
enum class TVG_EXPORT StrokeCap
{
    Square = 0, ///< The stroke is extended in both endpoints of a sub-path by a rectangle, with the width equal to the stroke width and the length equal to the half of the stroke width. For zero length sub-paths the square is rendered with the size of the stroke width.
    Round,      ///< The stroke is extended in both endpoints of a sub-path by a half circle, with a radius equal to the half of a stroke width. For zero length sub-paths a full circle is rendered.
    Butt        ///< The stroke ends exactly at each of the two endpoints of a sub-path. For zero length sub-paths no stroke is rendered.
};

/**
 * @brief Enumeration determining the style used at the corners of joined stroked path segments.
 */
enum class TVG_EXPORT StrokeJoin
{
    Bevel = 0, ///< The outer corner of the joined path segments is bevelled at the join point. The triangular region of the corner is enclosed by a straight line between the outer corners of each stroke.
    Round,     ///< The outer corner of the joined path segments is rounded. The circular region is centered at the join point.
    Miter      ///< The outer corner of the joined path segments is spiked. The spike is created by extension beyond the join point of the outer edges of the stroke until they intersect. In case the extension goes beyond the limit, the join style is converted to the Bevel style.
};

/**
 * @brief Enumeration specifying how to fill the area outside the gradient bounds.
 */
enum class TVG_EXPORT FillSpread
{
    Pad = 0, ///< The remaining area is filled with the closest stop color.
    Reflect, ///< The gradient pattern is reflected outside the gradient area until the expected region is filled.
    Repeat   ///< The gradient pattern is repeated continuously beyond the gradient area until the expected region is filled.
};

/**
 * @brief Enumeration specifying the algorithm used to establish which parts of the shape are treated as the inside of the shape.
 */
enum class TVG_EXPORT FillRule
{
    Winding = 0, ///< A line from the point to a location outside the shape is drawn. The intersections of the line with the path segment of the shape are counted. Starting from zero, if the path segment of the shape crosses the line clockwise, one is added, otherwise one is subtracted. If the resulting sum is non zero, the point is inside the shape.
    EvenOdd      ///< A line from the point to a location outside the shape is drawn and its intersections with the path segments of the shape are counted. If the number of intersections is an odd number, the point is inside the shape.
};

/**
 * @brief Enumeration indicating the method used in the composition of two objects - the target and the source.
 */
enum class TVG_EXPORT CompositeMethod
{
    None = 0,     ///< No composition is applied.
    ClipPath,     ///< The intersection of the source and the target is determined and only the resulting pixels from the source are rendered.
    AlphaMask,    ///< The pixels of the source and the target are alpha blended. As a result, only the part of the source, which intersects with the target is visible.
    InvAlphaMask  ///< The pixels of the source and the complement to the target's pixels are alpha blended. As a result, only the part of the source which is not covered by the target is visible.
};

/**
 * @brief Enumeration specifying the engine type used for the graphic rendering.
 */
enum class TVG_EXPORT CanvasEngine
{
    Sw = (1 << 1), ///< CPU rendering.
    Gl = (1 << 2)  ///< OpenGL rendering.
};


/**
 * @brief A data structure representing a point in two-dimensional space.
 */
struct Point
{
    float x, y;
};


/**
 * @brief A data structure representing a three-dimensional matrix.
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
 * @brief A class for drawing a vector primitive.
 *
 */
class TVG_EXPORT Paint
{
public:
    virtual ~Paint();

    /**
     * @brief Sets the angle by which the object is rotated.
     *
     * The angle in measured clockwise from the horizontal axis.
     * The rotational axis passes through the point on the object with zero coordinates.
     *
     * @param[in] degree The value of the angle in degrees.
     * @return Result::Success when succeeded, Result::FailedAllocation otherwise.
     */
    Result rotate(float degree) noexcept;

    /**
     * @brief Sets the scale value of the object.
     * @param[in] factor The value of the scaling factor.
     * @return Result::Success when succeeded, Result::FailedAllocation otherwise.
     */
    Result scale(float factor) noexcept;

    /**
     * @brief Sets the values by which the object is moved in a two-dimensional space.
     *
     * The origin of the coordinate system is in the upper left corner of the canvas.
     * The horizontal and vertical axes point to the right and down, respectively.
     *
     * @param[in] x The value of the horizontal shift.
     * @param[in] y The value of the vertical shift.
     * @return Result::Success when succeeded, Result::FailedAllocation otherwise.
     */
    Result translate(float x, float y) noexcept;

    /**
     * @brief Sets the matrix of the affine transformation for the object.
     *
     * The augmented matrix of the transformation is expected to be given.
     * The elements e11, e12, e21 and e22 represent the rotation matrix, including the scaling factor.
     * The elements e13 and e23 determine the translation of the object along the x and y-axis, respectively.
     * The element e31 and e32 are set to 0, e33 is set to 1.
     *
     * @param[in] m The 3x3 augmented matrix.
     * @return Result::Success when succeeded, Result::FailedAllocation otherwise.
     */
    Result transform(const Matrix& m) noexcept;

    /**
     * @brief Sets the opacity of the object.
     * @param[in] o The opacity value in the range [0 ~ 255], where 0 is completely transparent and 255 is opaque.
     * @return Result::Success.
     */
    Result opacity(uint8_t o) noexcept;

    /**
     * @brief Sets the composition target object and the composition method.
     * @param[in] target The pointer to the target object.
     * @param[in] method The method used to composite the source object with the target.
     * @return Result::Success when succeeded, Result::InsufficientCondition otherwise.
     */
    Result composite(std::unique_ptr<Paint> target, CompositeMethod method) const noexcept;

    /**
     * @brief Gets the bounding box of the object before any transformation.
     * @param[out] x The x coordinate of the upper left corner of the object.
     * @param[out] y The y coordinate of the upper left corner of the object.
     * @param[out] w The width of the object.
     * @param[out] h The height of the object.
     * @return Result::Success when succeeded, Result::InsufficientCondition otherwise.
     */
    Result bounds(float* x, float* y, float* w, float* h) const noexcept;

    /**
     * @brief Duplicates the object.
     *
     * Creates a new object and sets all its properties as in the original object.
     *
     * @return The pointer to the created object when succeeded, @c nullptr otherwise.
     */
    Paint* duplicate() const noexcept;

    /**
     * @brief Gets the opacity value of the object.
     * @return The opacity value in the range [0 ~ 255], where 0 is completely transparent and 255 is opaque.
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
 * @brief A class representing the gradient fill of the Shape object.
 *
 * It contains the information about the gradient colors and their arrangement
 * inside the gradient bounds. The gradients bounds are defined in the LinearGradient
 * or RadialGradient class, depending on the type of the gradient to be used.
 * It specifies the gradient behavior in case the area defined by the gradient bounds
 * is smaller than the area to be filled.
 */
class TVG_EXPORT Fill
{
public:
    /**
     * @brief A data structure storing the information about the color and its relative position inside the gradient bounds.
     */
    struct ColorStop
    {
        float offset; /**< The relative position of the color. */
        uint8_t r;    /**< The red color channel value in the range [0 ~ 255]. */
        uint8_t g;    /**< The green color channel value in the range [0 ~ 255]. */
        uint8_t b;    /**< The blue color channel value in the range [0 ~ 255]. */
        uint8_t a;    /**< The alpha channel value in the range [0 ~ 255], where 0 is completely transparent and 255 is opaque. */
    };

    virtual ~Fill();

    /**
     * @brief Sets the parameters of the colors of the gradient and their position.
     * @param[in] colorStops An array of the ColorStop objects.
     * @param[in] cnt The size of the @p colorStops array equal to the colors number used in the gradient.
     * @return Result::Success.
     */
    Result colorStops(const ColorStop* colorStops, uint32_t cnt) noexcept;

    /**
     * @brief Sets the FillSpread value, which specifies how to fill the area outside the gradient bounds.
     * @param[in] s The FillSpread value.
     * @return Result::Success.
     */
    Result spread(FillSpread s) noexcept;

    /**
     * @brief Gets the parameters of the colors of the gradient, their position and their number.
     * @param[in] colorStops A pointer to the memory location, where the array of the gradient's ColorStop objects is stored.
     * @return The number of colors used in the gradient. This value corresponds to the length of the @p colorStops array.
     */
    uint32_t colorStops(const ColorStop** colorStops) const noexcept;

    /**
     * @brief Gets the FillSpread value of the fill.
     * @return The set FillSpread enumeration values.
     */
    FillSpread spread() const noexcept;

    /**
     * @brief Creates a copy of the Fill object.
     * @return A pointer to a newly created Fill object with the properties copied from the original. Fill object when succeeded, @c nullptr otherwise.
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
     * return
     */
    virtual Result push(std::unique_ptr<Paint> paint) noexcept;

    /**
     * @brief
     * @param[in]
     * return Result::Success when succeeded, Result::InsufficientCondition otherwise.
     */
    virtual Result clear(bool free = true) noexcept;

    /**
     * @brief ...
     * @param[in]
     * return
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
 * @brief A class representing the linear gradient fill of the Shape object.
 *
 * Besides the APIs inherited from the Fill class, it enables setting and getting the linear gradient bounds.
 * The behavior outside the gradient bounds depends on the value specified in the spread API.
 */
class TVG_EXPORT LinearGradient final : public Fill
{
public:
    ~LinearGradient();

    /**
     * @brief Sets the linear gradient bounds.
     *
     * The bounds of the linear gradient are defined as a surface constrained by two parallel lines crossing
     * the given points (x1, y1) and (x2, y2), respectively. Both lines are perpendicular to the line linking
     * (x1, y1) and (x2, y2).
     *
     * @param[in] x1 The horizontal coordinate of the first point used to determine the gradient bounds.
     * @param[in] y1 The vertical coordinate of the first point used to determine the gradient bounds.
     * @param[in] x2 The horizontal coordinate of the second point used to determine the gradient bounds.
     * @param[in] y2 The vertical coordinate of the second point used to determine the gradient bounds.
     * @return Result::Success when succeeded, Result::InvalidArguments otherwise.
     */
    Result linear(float x1, float y1, float x2, float y2) noexcept;

    /**
     * @brief Gets the linear gradient bounds.
     *
     * The bounds of the linear gradient are defined as a surface constrained by two parallel lines crossing
     * the given points (x1, y1) and (x2, y2), respectively. Both lines are perpendicular to the line linking
     * (x1, y1) and (x2, y2).
     *
     * @param[out] x1 The horizontal coordinate of the first point used to determine the gradient bounds.
     * @param[out] y1 The vertical coordinate of the first point used to determine the gradient bounds.
     * @param[out] x2 The horizontal coordinate of the second point used to determine the gradient bounds.
     * @param[out] y2 The vertical coordinate of the second point used to determine the gradient bounds.
     * @return Result::Success.
     */
    Result linear(float* x1, float* y1, float* x2, float* y2) const noexcept;

    /**
     * @brief Creates a new LinearGradient object.
     * @return A unique pointer to the new LinearGradient object.
     */
    static std::unique_ptr<LinearGradient> gen() noexcept;

    _TVG_DECLARE_PRIVATE(LinearGradient);
};


/**
 * @class RadialGradient
 *
 * @ingroup ThorVG
 *
 * @brief A class representing the radial gradient fill of the Shape object.
 *
 */
class TVG_EXPORT RadialGradient final : public Fill
{
public:
    ~RadialGradient();

    /**
     * @brief Sets the radial gradient bounds.
     *
     * The radial gradient bounds are defined as a circle centered in a given
     * point (cx, cy) of a given radius.
     *
     * @param[in] cx The horizontal coordinate of the center of the bounding circle.
     * @param[in] cy The vertical coordinate of the center of the bounding circle.
     * @param[in] radius The radius of the bounding circle.
     * @return Result::Success when succeeded, Result::InvalidArguments otherwise.
     */
    Result radial(float cx, float cy, float radius) noexcept;

    /**
     * @brief Gets the radial gradient bounds.
     *
     * The radial gradient bounds are defined as a circle centered in a given
     * point (cx, cy) of a given radius.
     *
     * @param[out] cx The horizontal coordinate of the center of the bounding circle.
     * @param[out] cy The vertical coordinate of the center of the bounding circle.
     * @param[out] radius The radius of the bounding circle.
     * @return Result::Success.
     */
    Result radial(float* cx, float* cy, float* radius) const noexcept;

    /**
     * @brief Creates a new RadialGradient object.
     * @return A unique pointer to the new RadialGradient object.
     */
    static std::unique_ptr<RadialGradient> gen() noexcept;

    _TVG_DECLARE_PRIVATE(RadialGradient);
};



/**
 * @class Shape
 *
 * @ingroup ThorVG
 *
 * @brief A class representing two-dimensional figures and their properties.
 *
 * The shapes of the figures in the Shape object are stored as the sub-paths in the path.
 * The data to be saved in the path can be read directly from the svg file or through the provided APIs.
 */
class TVG_EXPORT Shape final : public Paint
{
public:
    ~Shape();

    /**
     * @brief Resets the information about the shape's path.
     *
     * The information about the color, the fill and the stroke is kept.
     * The memory, where the path data is stored, is not deallocated at this stage.
     *
     * @return Result::Success.
     */
    Result reset() noexcept;

    /**
     * @brief Sets the initial point of the sub-path.
     *
     * The value of the current point is set to the given point.
     * Corresponds to the M command from the svg path commands.
     *
     * @param[in] x The horizontal coordinate of the initial point of the sub-path.
     * @param[in] t The vertical coordinate of the initial point of the sub-path.
     * @return Result::Success.
     */
    Result moveTo(float x, float y) noexcept;

    /**
     * @brief Adds a new point to the sub-path, which results in drawing a line from the current
     * point to the given endpoint.
     *
     * The value of the current point is set to the given endpoint.
     * Corresponds to the L command from the svg path commands.
     * In case this is the first command in the path, it corresponds to the moveTo command.
     *
     * @param[in] x The horizontal coordinate of the endpoint of the line.
     * @param[in] t The vertical coordinate of the endpoint of the line.
     * @return Result::Success.
     */
    Result lineTo(float x, float y) noexcept;

    /**
     * @brief Adds new points to the sub-path, which results in drawing a cubic Bezier curve starting
     * at the current point and ending at the given endpoint (x, y) using the control points (cx1, cy1)
     * and (cx2, cy2).
     *
     * The value of the current point is set to the given endpoint.
     * Corresponds to the C command from the svg path commands.
     * In case this is the first command in the path, no data from the path are rendered.
     *
     * @param[in] cx1 The horizontal coordinate of the 1st control point.
     * @param[in] cy1 The vertical coordinate of the 1st control point.
     * @param[in] cx2 The horizontal coordinate of the 2nd control point.
     * @param[in] cy2 The vertical coordinate of the 2nd control point.
     * @param[in] x The horizontal coordinate of the endpoint of the curve.
     * @param[in] t The vertical coordinate of the endpoint of the curve.
     * @return Result::Success.
     */
    Result cubicTo(float cx1, float cy1, float cx2, float cy2, float x, float y) noexcept;

    /**
     * @brief Closes the current sub-path by drawing a line from the current point to the initial point
     * of the sub-path.
     *
     * The value of the current point is set to the initial point of the closed sub-path.
     * Corresponds to the Z command from the svg path commands.
     * In case the sub-path does not contain any points, this function has no effect.
     *
     * @return Result::Success.
     */
    Result close() noexcept;

    /**
     * @brief Appends a rectangle to the path.
     *
     * The rectangle with rounded corners can be achieved by setting non-zero values to the @p rx
     * and @p ry arguments. The @p rx and @p ry values specify the radii of the ellipse defining
     * the rounding of the corners.
     * For the @p rx and @p ry values greater than the half of the width and the half of the height,
     * respectively, an ellipse is drawn.
     *
     * The position of the rectangle is specified by the coordinates of its upper left
     * corner - the @p x and the @p y arguments.
     *
     * The rectangle is treated as a new sub-path - it is not connected with the previous sub-path.
     *
     * The value of the current point is set to (@p x + @p rx, @p y) - in case @p rx is greater
     * than @p w/2 the current point is set to (@p x + @p w/2, @p y)
     *
     * @param[in] x The horizontal coordinate of the upper left corner of the rectangle.
     * @param[in] y The vertical coordinate of the upper left corner of the rectangle.
     * @param[in] w The width of the rectangle.
     * @param[in] h The height of the rectangle.
     * @param[in] rx The x-axis radius of the ellipse defining the rounded corners of the rectangle.
     * @param[in] ry The y-axis radius of the ellipse defining the rounded corners of the rectangle.
     * @return Result::Success.
     */
    Result appendRect(float x, float y, float w, float h, float rx, float ry) noexcept;

    /**
     * @brief Appends an ellipse to the path.
     *
     * The position of the ellipse is specified by the coordinates of its center - the @p cx and
     * the @p cy arguments.
     *
     * The ellipse is treated as a new sub-path - it is not connected with the previous sub-path.
     *
     * The value of the current point is set to (@p cx, @p cy - @p ry).
     *
     * @param[in] cx The horizontal coordinate of the center of the ellipse.
     * @param[in] cy The vertical coordinate of the center of the ellipse.
     * @param[in] rx The x-axis radius of the ellipse.
     * @param[in] ry The y-axis radius of the ellipse.
     * @return Result::Success.
     */
    Result appendCircle(float cx, float cy, float rx, float ry) noexcept;

    /**
     * @brief Appends a circular arc to the path.
     *
     * The arc is treated as a new sub-path - it is not connected with the previous sub-path.
     *
     * The current point value is set to the endpoint of the arc in case @p pie is @c false,
     * and to the center of the arc otherwise.
     *
     * Setting @p sweep value greater than 360 degrees, is equivalent to calling appendCircle(cx, cy, radius, radius).
     *
     * @param[in] cx The horizontal coordinate of the center of the arc.
     * @param[in] cy The vertical coordinate of the center of the arc.
     * @param[in] radius The radius of the arc.
     * @param[in] startAngle The start angle of the arc given in degrees, measured
     *                       counterclockwise from the horizontal line.
     * @param[in] sweep The central angle of the arc given in degrees, measured
     *                  counterclockwise from the @p startAngle.
     * @param[in] pie Specifies whether to draw radii from the arc's center to both of its endpoints - drawn if @c true.
     * @return Result::Success.
     */
    Result appendArc(float cx, float cy, float radius, float startAngle, float sweep, bool pie) noexcept;

    /**
     * @brief Appends a given sub-path to the path.
     *
     * The current point value is set to the last point from the sub-path.
     *
     * For each command from the @p cmds array, an appropriate number of points in @p pts array should be specified,
     * as defined for the svg commands.
     *
     * @param[in] cmds The array of the commands in the sub-path.
     * @param[in] cmdCnt The number of the sub-path's commands.
     * @param[in] pts The array of the two-dimensional points.
     * @param[in] ptsCnt The number of the points in the @p pts array.
     * @return Result::Success when succeeded, Result::InvalidArguments otherwise.
     */
    Result appendPath(const PathCommand* cmds, uint32_t cmdCnt, const Point* pts, uint32_t ptsCnt) noexcept;

    /**
     * @brief Sets the stroke width for all of the figures from the path.
     * @param[in] width The width of the stroke.
     * @return Result::Success when succeeded, Result::FailedAllocation otherwise.
     */
    Result stroke(float width) noexcept;

    /**
     * @brief Sets the color of the stroke for all of the figures from the path.
     * @param[in] r The red color channel value in the range [0 ~ 255].
     * @param[in] g The green color channel value in the range [0 ~ 255].
     * @param[in] b The blue color channel value in the range [0 ~ 255].
     * @param[in] a The alpha channel value in the range [0 ~ 255], where 0 is completely transparent and 255 is opaque.
     * @return Result::Success when succeeded, Result::FailedAllocation otherwise.
     */
    Result stroke(uint8_t r, uint8_t g, uint8_t b, uint8_t a) noexcept;

    /**
     * @brief Sets the gradient fill of the stroke for all of the figures from the path..
     * @param[in] The unique pointer to the gradient fill.
     * @return Result::Success when succeeded, Result::FailedAllocation or Result::MemoryCorruption otherwise.
     */
    Result stroke(std::unique_ptr<Fill> f) noexcept;

    /**
     * @brief Sets the dash pattern of the stroke.
     *
     * If any of the dash pattern values is zero, this function has no effect.
     *
     * @param[in] dashPattern The array of consecutive pair values of the dash length and the gap length.
     * @param[in] cnt The length of the @p dashPattern array.
     * @return Result::Success when succeeded, Result::FailedAllocation or Result::InvalidArguments otherwise.
     */
    Result stroke(const float* dashPattern, uint32_t cnt) noexcept;

    /**
     * @brief Sets the cap style of the stroke in the open sub-paths.
     * @param[in] cap The cap style value.
     * @return Result::Success when succeeded, Result::FailedAllocation otherwise.
     */
    Result stroke(StrokeCap cap) noexcept;

    /**
     * @brief Sets the join style for stroked path segments.
     * The join style will be used for joining the two line segment while stroking the path.
     * @param[in] join The join style value.
     * @return Result::Success when succeeded, Result::FailedAllocation otherwise.
     */
    Result stroke(StrokeJoin join) noexcept;

    /**
     * @brief Sets the color of the fill for all of the figures from the path.
     *
     * The parts of the shape defined as inner are colored.
     *
     * @param[in] r The red color channel value in the range [0 ~ 255].
     * @param[in] g The green color channel value in the range [0 ~ 255].
     * @param[in] b The blue color channel value in the range [0 ~ 255].
     * @param[in] a The alpha channel value in the range [0 ~ 255], where 0 is completely transparent and 255 is opaque.
     * @return Result::Success.
     */
    Result fill(uint8_t r, uint8_t g, uint8_t b, uint8_t a) noexcept;

    /**
     * @brief Sets the gradient fill for all of the figures from the path.
     *
     * The parts of the shape defined as inner are filled.
     *
     * @param[in] The unique pointer to the gradient fill.
     * @return Result::Success when succeeded, Result::MemoryCorruption otherwise.
     */
    Result fill(std::unique_ptr<Fill> f) noexcept;

    /**
     * @brief Sets the fill rule for the Shape object.
     * @param[in] r The fill rule value.
     * @return Result::Success.
     */
    Result fill(FillRule r) noexcept;

    /**
     * @brief Gets the commands data of the path.
     * @param[out] cmds The pointer to the array of the commands from the path.
     * @return The length of the @p cmds array when succeeded, zero otherwise.
     */
    uint32_t pathCommands(const PathCommand** cmds) const noexcept;

    /**
     * @brief Gets the points values of the path.
     * @param[out] pts The pointer to the array of the two-dimensional points from the path.
     * @return The length of the @p pts array when succeeded, zero otherwise.
     */
    uint32_t pathCoords(const Point** pts) const noexcept;

    /**
     * @brief Gets the pointer to the gradient fill of the shape.
     * @return The pointer to the gradient fill of the stroke when succeeded, @c nullptr in case no fill was set.
     */
    const Fill* fill() const noexcept;

    /**
     * @brief Gets the color of the shape's fill.
     * @param[out] r The red color channel value in the range [0 ~ 255].
     * @param[out] g The green color channel value in the range [0 ~ 255].
     * @param[out] b The blue color channel value in the range [0 ~ 255].
     * @param[out] a The alpha channel value in the range [0 ~ 255], where 0 is completely transparent and 255 is opaque.
     * @return Result::Success.
     */
    Result fillColor(uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* a) const noexcept;

    /**
     * @brief Gets the fill rule value.
     * @return The fill rule value of the shape.
     */
    FillRule fillRule() const noexcept;

    /**
     * @brief Gets the stroke width.
     * @return The stroke width value when succeeded, zero if no stroke was set.
     */
    float strokeWidth() const noexcept;

    /**
     * @brief Gets the color of the shape's stroke.
     * @param[out] r The red color channel value in the range [0 ~ 255].
     * @param[out] g The green color channel value in the range [0 ~ 255].
     * @param[out] b The blue color channel value in the range [0 ~ 255].
     * @param[out] a The alpha channel value in the range [0 ~ 255], where 0 is completely transparent and 255 is opaque.
     * @return Result::Success when succeeded, Result::InsufficientCondition otherwise.
     */
    Result strokeColor(uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* a) const noexcept;

    /**
     * @brief Gets the pointer to the gradient fill of the stroke.
     * @return The pointer to the gradient fill of the stroke when succeeded, @c nullptr otherwise.
     */
    const Fill* strokeFill() const noexcept;

    /**
     * @brief Gets the dash pattern of the stroke.
     * @param[out] dashPattern The pointer to the memory, where the dash pattern array is stored.
     * @return The length of dash pattern array.
     */
    uint32_t strokeDash(const float** dashPattern) const noexcept;

    /**
     * @brief Gets the cap style used for stroking the path.
     * @return The cap style value of the stroke.
     */
    StrokeCap strokeCap() const noexcept;

    /**
     * @brief Gets the join style value used for stroking the path.
     * @return The join style value of the stroke.
     */
    StrokeJoin strokeJoin() const noexcept;

    /**
     * @brief Creates a new Shape object.
     * @return A unique pointer to the new Shape object.
     */
    static std::unique_ptr<Shape> gen() noexcept;

    _TVG_DECLARE_PRIVATE(Shape);
};


/**
 * @class Picture
 *
 * @ingroup ThorVG
 *
 * @brief A class representing an image read in one of the supported formats: svg, png and raw.
 * Besides the methods inherited from the Paint, it provides methods to load the image,
 * to change its size and to get the basic information.
 *
 */
class TVG_EXPORT Picture final : public Paint
{
public:
    ~Picture();

    /**
     * @brief Loads a picture data from a file.
     *
     * The direct loading from a file is available for the svg and png files.
     *
     * @param[in] path A path to the picture file.
     * @return Result::Success when succeeded, otherwise the returned value depends on the problem type:
     * Result::InvalidArguments - in case the @p path is empty,
     * Result::NonSupport - when trying to load a file with an unknown extension,
     * Result::Unknown - if an error occurs at a later stage.
     */
    Result load(const std::string& path) noexcept;

    /**
     * @brief Loads a picture data from a memory block of a given size.
     *
     * This method of data loading is supported only for the svg files.
     *
     * @param[in] data A pointer to a memory location where the content of the svg file is stored.
     * @param[in] size The size in bytes of the memory occupied by the @p data.
     * @return Result::Success when succeeded, otherwise the returned value depends on the problem type:
     * Result::InvalidArguments - in case no data are provided or the @p size is zero or less,
     * Result::NonSupport - when trying to load a file with an unknown extension,
     * Result::Unknown - if an error occurs at a later stage.
     */
    Result load(const char* data, uint32_t size) noexcept;

    /**
     * @brief Loads a picture data from a memory block of a given size.
     *
     * This method of data loading is supported only for the raw image format.
     *
     * @param[in] data The contiguous memory block of a size of w x h with the pixels information.
     * @param[in] w The width of the image in pixels.
     * @param[in] h The height of the image in pixels.
     * @param[in] copy Specifies the method of copying the @p data - shallow if @c false and deep id @c true.
     * @return Result::Success when succeeded, otherwise the returned value depends on the problem type:
     * Result::InvalidArguments - in case no data is provided or the width or the height of the picture is zero or less,
     * Result::NonSupport - when trying to load a file with an unknown extension.
     */
    Result load(uint32_t* data, uint32_t w, uint32_t h, bool copy) noexcept;

    /**
     * @brief ...
     * @param[out] x
     * @param[out] y
     * @param[out] w
     * @param[out] h
     * @return Result::Success when succeeded, ... otherwise.
     */
    //TODO: Replace with size(). Remove API
    Result viewbox(float* x, float* y, float* w, float* h) const noexcept;

    /**
     * @brief Sets a new size of the image.
     *
     * The functionality works only for the images loaded from svg files.
     * The preserveAspectRatio attribute specified in the svg file determines whether the aspect ratio is preserved during the resizing.
     * In case the preserving needs to be forced,
     * the scaling factor is established for each of the-dimensions and the smaller value is applied
     * to both of them.
     *
     * @param[in] w A new width of the image in pixels.
     * @param[in] h A new height of the image in pixels.
     * @return Result::Success when succeeded, Result::InsufficientCondition otherwise.
     */
    Result size(float w, float h) noexcept;

    /**
     * @brief Gets the size of the image.
     * @param[out] w The width of the image in pixels.
     * @param[out] h The height of the image in pixels.
     * @return Result::Success.
     */
    Result size(float* w, float* h) const noexcept;

    /**
     * @brief Gets the pixels information of the image.
     *
     * The option is available for png and raw file formats.
     *
     * @return A pointer to the memory location where the image data are stored when succeeded, @p nullptr otherwise.
     */
    const uint32_t* data() const noexcept;

    /**
     * @brief Creates a new Picture object.
     * @return A unique pointer to the new Picture object.
     */
    static std::unique_ptr<Picture> gen() noexcept;

    _TVG_DECLARE_PRIVATE(Picture);
};


/**
 * @class Scene
 *
 * @ingroup ThorVG
 *
 * @brief A class enabling to hold many Paint objects.
 *
 * As a whole they can be transformed, their transparency can be changed, or the composition
 * methods may be used to all of them at once.
 *
 */
class TVG_EXPORT Scene final : public Paint
{
public:
    ~Scene();

    /**
     * @brief Passes the Paint object to the Scene.
     *
     * The function may be called many times and all passed objects will be rendered.
     *
     * @param[in] paint A unique pointer to the Paint object.
     * @return Result::Success when succeeded, Result::MemoryCorruption otherwise.
     */
    Result push(std::unique_ptr<Paint> paint) noexcept;

    /**
     * @brief Sets the size of the memory block, where all the paints pushed into the scene are stored.
     *
     * If the number of objects pushed into the scene is known in advance, calling the function
     * prevents multiple memory reallocation, thus improving the performance.
     *
     * @param[in] size The number of objects for which the memory is to be reserved.
     * @return Result::Success.
     */
    Result reserve(uint32_t size) noexcept;

    /**
     * @brief Sets the total number of the paints pushed into the scene to be zero.
     *
     * The memory is not deallocated at this stage.
     *
     * @return Result::Success.
     */
    Result clear() noexcept;

    /**
     * @brief Creates a new Scene object.
     * @return A unique pointer to the new Scene object.
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
     * @brief Enumeration specifying the methods of combining the 8-bit color channels into 32-bit color.
     */
    enum Colorspace
    {
        ABGR8888 = 0, ///<
        ARGB8888      ///<
    };

    /**
     * @brief ...
     * @param[in]
     * @param[in]
     * @param[in]
     * @param[in]
     * @param[in]
     * @return Result::Success when succeeded, ... otherwise.
     */
    Result target(uint32_t* buffer, uint32_t stride, uint32_t w, uint32_t h, Colorspace cs) noexcept;

    /**
     * @brief ...
     * @return Result::Success when succeeded, ... otherwise.
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
     * @param[in]
     * @param[in]
     * @param[in]
     * @param[in]
     * @return Result::Success when succeeded, ... otherwise.
     */
    //TODO: Gl Specific methods. Need gl backend configuration methods as well.
    Result target(uint32_t* buffer, uint32_t stride, uint32_t w, uint32_t h) noexcept;

    /**
     * @brief ...
     * @return
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
     * @param[in] arg ...
     * @return ...
     */
    static Result term(CanvasEngine engine) noexcept;

    _TVG_DISABLE_CTOR(Initializer);
};

} //namespace

#ifdef __cplusplus
}
#endif

#endif //_THORVG_H_
