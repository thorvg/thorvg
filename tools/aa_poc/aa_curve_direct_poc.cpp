/*
 * Copyright (c) 2026 ThorVG project. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "aa_poc_cli.h"
#include "aa_poc_comparison_scene.h"
#include "aa_poc_gl.h"
#include "tvgMath.h"
#include "tvgRender.h"

using namespace tvg;

namespace
{

constexpr uint32_t NATIVE_WIDTH = 720;
constexpr uint32_t NATIVE_HEIGHT = 480;
constexpr float AA_RADIUS = 0.5f;

// POC boundary representation: regular cubics are implicitized once on the
// CPU into a normalized screen-space cubic polynomial. The fragment shader
// evaluates F/|gradient F| for coverage. Original controls are also retained
// solely to clip the infinite implicit curve to the source segment's AA band.

struct Color
{
    float r, g, b, a;
};

enum class PatchKind : uint8_t
{
    Line,
    Cubic,
};

struct BoundaryVertex
{
    float x, y;
    float p0x, p0y;
    float p1x, p1y;
    float p2x, p2y;
    float p3x, p3y;
    float c0, c1, c2, c3;
    float c4, c5, c6, c7;
    float c8, c9;
    float centerX, centerY, inverseScale;
    float kind;
    float insideSign;
};

static_assert(sizeof(BoundaryVertex) == 25 * sizeof(float), "BoundaryVertex must stay tightly packed");

struct Mesh
{
    std::vector<Point> points;
    std::vector<uint32_t> indices;
    std::vector<BoundaryVertex> boundary;
    Color color;
};

struct Programs
{
    GLuint solid = 0;
    GLuint boundary = 0;
};

#if defined(AA_POC_GLES)
#define AA_POC_GLSL_HEADER "#version 300 es\nprecision highp float;\n"
#else
#define AA_POC_GLSL_HEADER "#version 330 core\n"
#endif

constexpr const char* SOLID_VERTEX_SHADER = AA_POC_GLSL_HEADER R"(
in vec2 aPosition;
uniform vec2 uViewport;
void main()
{
    vec2 ndc = vec2(aPosition.x * 2.0 / uViewport.x - 1.0,
                    1.0 - aPosition.y * 2.0 / uViewport.y);
    gl_Position = vec4(ndc, 0.0, 1.0);
}
)";

constexpr const char* SOLID_FRAGMENT_SHADER = AA_POC_GLSL_HEADER R"(
uniform vec4 uColor;
out vec4 fragColor;
void main()
{
    fragColor = uColor;
}
)";

constexpr const char* BOUNDARY_VERTEX_SHADER = AA_POC_GLSL_HEADER R"(
in vec2 aPosition;
in vec2 aP0;
in vec2 aP1;
in vec2 aP2;
in vec2 aP3;
in vec4 aImplicit0;
in vec4 aImplicit1;
in vec2 aImplicit2;
in vec3 aNormalization;
in float aKind;
in float aInsideSign;
uniform vec2 uViewport;
flat out vec2 vP0;
flat out vec2 vP1;
flat out vec2 vP2;
flat out vec2 vP3;
flat out vec4 vImplicit0;
flat out vec4 vImplicit1;
flat out vec2 vImplicit2;
flat out vec3 vNormalization;
flat out float vKind;
flat out float vInsideSign;
void main()
{
    vec2 ndc = vec2(aPosition.x * 2.0 / uViewport.x - 1.0,
                    1.0 - aPosition.y * 2.0 / uViewport.y);
    gl_Position = vec4(ndc, 1.0, 1.0);
    vP0 = aP0;
    vP1 = aP1;
    vP2 = aP2;
    vP3 = aP3;
    vImplicit0 = aImplicit0;
    vImplicit1 = aImplicit1;
    vImplicit2 = aImplicit2;
    vNormalization = aNormalization;
    vKind = aKind;
    vInsideSign = aInsideSign;
}
)";

constexpr const char* BOUNDARY_FRAGMENT_SHADER = AA_POC_GLSL_HEADER R"(
flat in vec2 vP0;
flat in vec2 vP1;
flat in vec2 vP2;
flat in vec2 vP3;
flat in vec4 vImplicit0;
flat in vec4 vImplicit1;
flat in vec2 vImplicit2;
flat in vec3 vNormalization;
flat in float vKind;
flat in float vInsideSign;
uniform vec2 uViewport;
uniform vec4 uColor;
out vec4 fragColor;

vec2 cubicPoint(float t)
{
    float s = 1.0 - t;
    return s * s * s * vP0 + 3.0 * s * s * t * vP1 +
           3.0 * s * t * t * vP2 + t * t * t * vP3;
}

vec2 cubicDerivative(float t)
{
    float s = 1.0 - t;
    return 3.0 * (s * s * (vP1 - vP0) +
                  2.0 * s * t * (vP2 - vP1) +
                  t * t * (vP3 - vP2));
}

vec2 cubicSecondDerivative(float t)
{
    return 6.0 * ((1.0 - t) * (vP2 - 2.0 * vP1 + vP0) +
                  t * (vP3 - 2.0 * vP2 + vP1));
}

float cubicImplicit(vec2 pixel)
{
    vec2 p = (pixel - vNormalization.xy) * vNormalization.z;
    float x = p.x;
    float y = p.y;
    return dot(vImplicit0, vec4(x * x * x, x * x * y, x * y * y, y * y * y)) +
           dot(vImplicit1, vec4(x * x, x * y, y * y, x)) +
           dot(vImplicit2, vec2(y, 1.0));
}

vec2 cubicImplicitGradient(vec2 pixel)
{
    vec2 p = (pixel - vNormalization.xy) * vNormalization.z;
    float x = p.x;
    float y = p.y;
    float dx = 3.0 * vImplicit0.x * x * x + 2.0 * vImplicit0.y * x * y +
               vImplicit0.z * y * y + 2.0 * vImplicit1.x * x +
               vImplicit1.y * y + vImplicit1.w;
    float dy = vImplicit0.y * x * x + 2.0 * vImplicit0.z * x * y +
               3.0 * vImplicit0.w * y * y + vImplicit1.y * x +
               2.0 * vImplicit1.z * y + vImplicit2.x;
    return vec2(dx, dy) * vNormalization.z;
}

float cubicClosestParameter(vec2 pixel)
{
    float bestT = 0.0;
    float bestDistance2 = dot(pixel - vP0, pixel - vP0);
    for (int seed = 0; seed <= 8; ++seed) {
        float t = float(seed) * 0.125;
        for (int iteration = 0; iteration < 5; ++iteration) {
            vec2 delta = cubicPoint(t) - pixel;
            vec2 first = cubicDerivative(t);
            float denominator = dot(first, first) + dot(delta, cubicSecondDerivative(t));
            if (abs(denominator) > 1e-6) {
                t = clamp(t - dot(delta, first) / denominator, 0.0, 1.0);
            }
        }
        vec2 delta = cubicPoint(t) - pixel;
        float distance2 = dot(delta, delta);
        if (distance2 < bestDistance2) {
            bestT = t;
            bestDistance2 = distance2;
        }
    }
    vec2 endDelta = pixel - vP3;
    if (dot(endDelta, endDelta) < bestDistance2) bestT = 1.0;
    return bestT;
}

void main()
{
    vec2 pixel = vec2(gl_FragCoord.x, uViewport.y - gl_FragCoord.y);
    vec2 closest;
    vec2 tangent;

    if (vKind < 0.5) {
        tangent = vP3 - vP0;
        float length2 = dot(tangent, tangent);
        float t = clamp(dot(pixel - vP0, tangent) / length2, 0.0, 1.0);
        closest = mix(vP0, vP3, t);
    } else {
        float t = cubicClosestParameter(pixel);
        closest = cubicPoint(t);
        tangent = cubicDerivative(t);
        if (dot(tangent, tangent) < 1e-8) tangent = vP3 - vP0;
    }

    vec2 delta = pixel - closest;
    float distancePx = length(delta);
    if (distancePx > 0.5) discard;
    float signedDistance;
    if (vKind < 0.5) {
        float inside = cross(vec3(tangent, 0.0), vec3(delta, 0.0)).z * vInsideSign;
        signedDistance = inside >= 0.0 ? -distancePx : distancePx;
    } else {
        float implicitValue = cubicImplicit(pixel);
        vec2 implicitGradient = cubicImplicitGradient(pixel);
        float gradientLength = length(implicitGradient);
        float gradientPointsInside = cross(vec3(tangent, 0.0),
                                           vec3(implicitGradient, 0.0)).z * vInsideSign;
        signedDistance = implicitValue / max(gradientLength, 1e-6);
        if (gradientPointsInside >= 0.0) signedDistance = -signedDistance;
    }
    float coverage = clamp(0.5 - signedDistance, 0.0, 1.0);
    if (coverage <= 0.0) discard;
    fragColor = uColor * coverage;
}
)";

#undef AA_POC_GLSL_HEADER

bool createPrograms(Programs& programs)
{
    constexpr aa_poc::AttributeBinding SOLID_ATTRIBUTES[] = {
        {0, "aPosition"},
    };
    constexpr aa_poc::AttributeBinding BOUNDARY_ATTRIBUTES[] = {
        {0, "aPosition"},
        {1, "aP0"},
        {2, "aP1"},
        {3, "aP2"},
        {4, "aP3"},
        {5, "aImplicit0"},
        {6, "aImplicit1"},
        {7, "aImplicit2"},
        {8, "aNormalization"},
        {9, "aKind"},
        {10, "aInsideSign"},
    };
    programs.solid = aa_poc::createProgram(
        SOLID_VERTEX_SHADER, SOLID_FRAGMENT_SHADER,
        SOLID_ATTRIBUTES, sizeof(SOLID_ATTRIBUTES) / sizeof(SOLID_ATTRIBUTES[0]),
        "aa_curve_direct_poc");
    programs.boundary = aa_poc::createProgram(
        BOUNDARY_VERTEX_SHADER, BOUNDARY_FRAGMENT_SHADER,
        BOUNDARY_ATTRIBUTES, sizeof(BOUNDARY_ATTRIBUTES) / sizeof(BOUNDARY_ATTRIBUTES[0]),
        "aa_curve_direct_poc");
    return programs.solid && programs.boundary;
}

void destroyPrograms(Programs& programs)
{
    if (programs.solid) glDeleteProgram(programs.solid);
    if (programs.boundary) glDeleteProgram(programs.boundary);
    programs = {};
}

struct CubicImplicit
{
    std::array<float, 10> coefficients = {};
    Point center = {};
    float inverseScale = 1.0f;
};

enum class ImplicitResult : uint8_t
{
    Success,
    Empty,
    Failure,
};

static Point cubicAt(const Point& p0, const Point& p1, const Point& p2, const Point& p3, double t)
{
    auto s = 1.0 - t;
    auto b0 = static_cast<float>(s * s * s);
    auto b1 = static_cast<float>(3.0 * s * s * t);
    auto b2 = static_cast<float>(3.0 * s * t * t);
    auto b3 = static_cast<float>(t * t * t);
    return p0 * b0 + p1 * b1 + p2 * b2 + p3 * b3;
}

static std::array<double, 10> cubicMonomials(const Point& point)
{
    auto x = static_cast<double>(point.x);
    auto y = static_cast<double>(point.y);
    return {x * x * x, x * x * y, x * y * y, y * y * y,
            x * x, x * y, y * y, x, y, 1.0};
}

static double evaluateImplicit(const std::array<double, 10>& coefficients, const Point& point)
{
    auto monomials = cubicMonomials(point);
    double value = 0.0;
    for (size_t i = 0; i < coefficients.size(); ++i) value += coefficients[i] * monomials[i];
    return value;
}

template<size_t Rows, size_t Columns>
bool nullVector(std::array<std::array<double, Columns>, Rows> matrix,
                std::array<double, Columns>& coefficients)
{
    static_assert(Columns == Rows + 1, "Null-space solve expects one free column");
    std::array<int, Rows> pivotColumns = {};
    size_t rank = 0;
    for (size_t column = 0; column < Columns && rank < Rows; ++column) {
        auto pivot = rank;
        for (size_t row = rank + 1; row < Rows; ++row) {
            if (std::abs(matrix[row][column]) > std::abs(matrix[pivot][column])) pivot = row;
        }
        if (std::abs(matrix[pivot][column]) < 1e-12) continue;
        if (pivot != rank) std::swap(matrix[pivot], matrix[rank]);

        auto divisor = matrix[rank][column];
        for (size_t i = column; i < Columns; ++i) matrix[rank][i] /= divisor;
        for (size_t row = 0; row < Rows; ++row) {
            if (row == rank) continue;
            auto factor = matrix[row][column];
            for (size_t i = column; i < Columns; ++i) matrix[row][i] -= factor * matrix[rank][i];
        }
        pivotColumns[rank] = static_cast<int>(column);
        ++rank;
    }
    if (rank != Rows) return false;

    std::array<bool, Columns> pivoted = {};
    for (size_t row = 0; row < rank; ++row) pivoted[pivotColumns[row]] = true;
    size_t freeColumn = 0;
    while (freeColumn < Columns && pivoted[freeColumn]) ++freeColumn;
    if (freeColumn == Columns) return false;

    coefficients = {};
    coefficients[freeColumn] = 1.0;
    for (size_t row = 0; row < rank; ++row) {
        coefficients[pivotColumns[row]] = -matrix[row][freeColumn];
    }
    auto scale = 0.0;
    for (auto coefficient : coefficients) scale = std::max(scale, std::abs(coefficient));
    if (scale == 0.0) return false;
    for (auto& coefficient : coefficients) coefficient /= scale;
    return true;
}

ImplicitResult implicitizeCubic(const Point& p0, const Point& p1, const Point& p2, const Point& p3,
                                CubicImplicit& implicit)
{
    auto minX = std::min(std::min(p0.x, p1.x), std::min(p2.x, p3.x));
    auto minY = std::min(std::min(p0.y, p1.y), std::min(p2.y, p3.y));
    auto maxX = std::max(std::max(p0.x, p1.x), std::max(p2.x, p3.x));
    auto maxY = std::max(std::max(p0.y, p1.y), std::max(p2.y, p3.y));
    implicit.center = {(minX + maxX) * 0.5f, (minY + maxY) * 0.5f};
    auto scale = std::max(maxX - minX, maxY - minY);
    if (scale == 0.0f) return ImplicitResult::Empty;
    implicit.inverseScale = 1.0f / scale;

    auto normalize = [&](const Point& point) {
        return (point - implicit.center) * implicit.inverseScale;
    };
    auto n0 = normalize(p0);
    auto n1 = normalize(p1);
    auto n2 = normalize(p2);
    auto n3 = normalize(p3);

    const Point controls[] = {n0, n1, n2, n3};
    size_t lineStart = 0;
    size_t lineEnd = 0;
    float maximumDistance2 = 0.0f;
    for (size_t i = 0; i < 4; ++i) {
        for (size_t j = i + 1; j < 4; ++j) {
            auto distance2 = dot(controls[j] - controls[i], controls[j] - controls[i]);
            if (distance2 > maximumDistance2) {
                maximumDistance2 = distance2;
                lineStart = i;
                lineEnd = j;
            }
        }
    }
    if (maximumDistance2 <= FLOAT_EPSILON * FLOAT_EPSILON) return ImplicitResult::Empty;

    auto line = controls[lineEnd] - controls[lineStart];
    auto lineLength = std::sqrt(maximumDistance2);
    auto collinear = true;
    for (const auto& control : controls) {
        if (std::abs(cross(line, control - controls[lineStart])) > FLOAT_EPSILON * lineLength) {
            collinear = false;
            break;
        }
    }

    std::array<double, 10> coefficients = {};
    if (collinear) {
        // F(x, y) = cross(line, point - lineStart).
        auto lineX = static_cast<double>(line.x);
        auto lineY = static_cast<double>(line.y);
        auto startX = static_cast<double>(controls[lineStart].x);
        auto startY = static_cast<double>(controls[lineStart].y);
        auto divisor = static_cast<double>(lineLength);
        coefficients[7] = -lineY / divisor;
        coefficients[8] = lineX / divisor;
        coefficients[9] = (lineY * startX - lineX * startY) / divisor;
    } else {
        auto cubicPower = n3 - n2 * 3.0f + n1 * 3.0f - n0;
        if (dot(cubicPower, cubicPower) <= FLOAT_EPSILON * FLOAT_EPSILON) {
            std::array<std::array<double, 6>, 5> matrix = {};
            for (size_t row = 0; row < matrix.size(); ++row) {
                auto t = static_cast<double>(row) / static_cast<double>(matrix.size() - 1);
                auto monomials = cubicMonomials(cubicAt(n0, n1, n2, n3, t));
                std::copy(monomials.begin() + 4, monomials.end(), matrix[row].begin());
            }
            std::array<double, 6> quadratic = {};
            if (!nullVector(matrix, quadratic)) return ImplicitResult::Failure;
            std::copy(quadratic.begin(), quadratic.end(), coefficients.begin() + 4);
        } else {
            std::array<std::array<double, 10>, 9> matrix = {};
            for (size_t row = 0; row < matrix.size(); ++row) {
                auto t = static_cast<double>(row) / static_cast<double>(matrix.size() - 1);
                matrix[row] = cubicMonomials(cubicAt(n0, n1, n2, n3, t));
            }
            if (!nullVector(matrix, coefficients)) return ImplicitResult::Failure;
        }
    }

    // Verify between solve samples so an unstable implicitization fails visibly.
    for (uint32_t i = 0; i <= 32; ++i) {
        auto t = static_cast<double>(i) / 32.0;
        if (std::abs(evaluateImplicit(coefficients, cubicAt(n0, n1, n2, n3, t))) > 1e-5) {
            return ImplicitResult::Failure;
        }
    }
    for (size_t i = 0; i < coefficients.size(); ++i) {
        implicit.coefficients[i] = static_cast<float>(coefficients[i]);
    }
    return ImplicitResult::Success;
}

bool validateImplicitizer()
{
    CubicImplicit implicit;
    Point q0{0.0f, 0.0f};
    Point q1{0.5f, 1.0f};
    Point q2{1.0f, 0.0f};
    auto elevated1 = (q0 + q1 * 2.0f) / 3.0f;
    auto elevated2 = (q1 * 2.0f + q2) / 3.0f;
    if (implicitizeCubic(q0, elevated1, elevated2, q2, implicit) != ImplicitResult::Success) return false;
    if (implicitizeCubic({0.0f, 0.0f}, {0.25f, 0.25f}, {0.75f, 0.75f}, {1.0f, 1.0f}, implicit) !=
        ImplicitResult::Success) return false;
    return implicitizeCubic(q0, q0, q0, q0, implicit) == ImplicitResult::Empty;
}

void appendBoundaryPatch(std::vector<BoundaryVertex>& vertices, PatchKind kind,
                         const Point& p0, const Point& p1, const Point& p2, const Point& p3)
{
    if (kind == PatchKind::Line && p0 == p3) return;

    auto patchKind = static_cast<float>(kind == PatchKind::Cubic);
    CubicImplicit implicit;
    if (kind == PatchKind::Cubic) {
        auto result = implicitizeCubic(p0, p1, p2, p3, implicit);
        if (result == ImplicitResult::Empty) return;
        if (result == ImplicitResult::Failure) {
            std::fprintf(stderr, "aa_curve_direct_poc: failed to implicitize a cubic boundary patch\n");
            std::exit(EXIT_FAILURE);
        }
    }

    auto vertex = [&](float x, float y) {
        return BoundaryVertex{
            x, y, p0.x, p0.y, p1.x, p1.y, p2.x, p2.y, p3.x, p3.y,
            implicit.coefficients[0], implicit.coefficients[1],
            implicit.coefficients[2], implicit.coefficients[3],
            implicit.coefficients[4], implicit.coefficients[5],
            implicit.coefficients[6], implicit.coefficients[7],
            implicit.coefficients[8], implicit.coefficients[9],
            implicit.center.x, implicit.center.y, implicit.inverseScale,
            patchKind, 1.0f
        };
    };
    BoundaryVertex v0;
    BoundaryVertex v1;
    BoundaryVertex v2;
    BoundaryVertex v3;
    if (kind == PatchKind::Line) {
        auto delta = p3 - p0;
        auto length = std::sqrt(dot(delta, delta));
        auto tangent = delta / length;
        Point normal{-tangent.y, tangent.x};
        auto start = p0 - tangent * AA_RADIUS;
        auto end = p3 + tangent * AA_RADIUS;
        v0 = vertex(start.x + normal.x * AA_RADIUS, start.y + normal.y * AA_RADIUS);
        v1 = vertex(start.x - normal.x * AA_RADIUS, start.y - normal.y * AA_RADIUS);
        v2 = vertex(end.x + normal.x * AA_RADIUS, end.y + normal.y * AA_RADIUS);
        v3 = vertex(end.x - normal.x * AA_RADIUS, end.y - normal.y * AA_RADIUS);
    } else {
        // A Bezier lies inside its control hull. Expanding the control-point
        // AABB by the filter radius is conservative; the fragment shader
        // discards the broad box outside the actual half-pixel curve band.
        auto minX = std::min(std::min(p0.x, p1.x), std::min(p2.x, p3.x)) - AA_RADIUS;
        auto minY = std::min(std::min(p0.y, p1.y), std::min(p2.y, p3.y)) - AA_RADIUS;
        auto maxX = std::max(std::max(p0.x, p1.x), std::max(p2.x, p3.x)) + AA_RADIUS;
        auto maxY = std::max(std::max(p0.y, p1.y), std::max(p2.y, p3.y)) + AA_RADIUS;
        v0 = vertex(minX, minY);
        v1 = vertex(maxX, minY);
        v2 = vertex(minX, maxY);
        v3 = vertex(maxX, maxY);
    }
    vertices.push_back(v0);
    vertices.push_back(v1);
    vertices.push_back(v2);
    vertices.push_back(v2);
    vertices.push_back(v1);
    vertices.push_back(v3);
}

Mesh buildMesh(const RenderPath& path, const Color& color)
{
    // The fill walk intentionally matches BWTessellator. Cubics are flattened
    // only for the normal stencil fan; each AA patch above retains the four
    // untouched Bezier control points from RenderPath.
    Mesh mesh;
    mesh.color = color;
    auto pts = path.pts.data;
    Point previous = {};
    Point contourFirst = {};
    uint32_t contourFirstIndex = 0;
    uint32_t contourPointCount = 0;
    size_t contourBoundaryStart = 0;
    float twiceArea = 0.0f;
    bool contourOpen = false;

    auto appendLinePatch = [&](const Point& from, const Point& to) {
        appendBoundaryPatch(mesh.boundary, PatchKind::Line, from, from, to, to);
    };

    auto finishContour = [&]() {
        if (!contourOpen) return;
        appendLinePatch(previous, contourFirst);
        twiceArea += previous.x * contourFirst.y - contourFirst.x * previous.y;
        auto insideSign = twiceArea >= 0.0f ? 1.0f : -1.0f;
        for (auto i = contourBoundaryStart; i < mesh.boundary.size(); ++i) {
            mesh.boundary[i].insideSign = insideSign;
        }
        contourOpen = false;
    };

    auto beginContour = [&](const Point& point) {
        finishContour();
        contourFirst = previous = point;
        contourFirstIndex = static_cast<uint32_t>(mesh.points.size());
        contourPointCount = 1;
        contourBoundaryStart = mesh.boundary.size();
        twiceArea = 0.0f;
        contourOpen = true;
        mesh.points.push_back(point);
    };

    auto appendFillPoint = [&](const Point& point) {
        twiceArea += previous.x * point.y - point.x * previous.y;
        mesh.points.push_back(point);
        if (contourPointCount >= 2) {
            auto currentIndex = static_cast<uint32_t>(mesh.points.size() - 1);
            mesh.indices.push_back(contourFirstIndex);
            mesh.indices.push_back(currentIndex - 1);
            mesh.indices.push_back(currentIndex);
        }
        ++contourPointCount;
        previous = point;
    };

    ARRAY_FOREACH(cmd, path.cmds) {
        switch (*cmd) {
            case PathCommand::MoveTo:
                beginContour(*pts++);
                break;
            case PathCommand::LineTo: {
                auto end = *pts++;
                appendLinePatch(previous, end);
                appendFillPoint(end);
                break;
            }
            case PathCommand::CubicTo: {
                auto start = previous;
                appendBoundaryPatch(mesh.boundary, PatchKind::Cubic, start, pts[0], pts[1], pts[2]);
                Bezier curve{start, pts[0], pts[1], pts[2]};
                auto count = std::max(curve.segments(), 2u);
                auto step = 1.0f / count;
                for (uint32_t i = 1; i <= count; ++i) appendFillPoint(curve.at(step * i));
                pts += 3;
                break;
            }
            case PathCommand::Close:
                finishContour();
                break;
        }
    }
    finishContour();
    return mesh;
}

void appendCircle(RenderPath& path, float cx, float cy, float radius)
{
    constexpr float KAPPA = 0.5522847498307936f;
    auto k = radius * KAPPA;
    path.moveTo({cx + radius, cy});
    path.cubicTo({cx + radius, cy + k}, {cx + k, cy + radius}, {cx, cy + radius});
    path.cubicTo({cx - k, cy + radius}, {cx - radius, cy + k}, {cx - radius, cy});
    path.cubicTo({cx - radius, cy - k}, {cx - k, cy - radius}, {cx, cy - radius});
    path.cubicTo({cx + k, cy - radius}, {cx + radius, cy - k}, {cx + radius, cy});
    path.close();
}

std::vector<Mesh> makeScene(float offsetX, float offsetY, float scale = 1.0f)
{
    auto point = [=](float x, float y) {
        return Point{(x + offsetX) * scale, (y + offsetY) * scale};
    };
    auto x = [=](float value) { return (value + offsetX) * scale; };
    auto y = [=](float value) { return (value + offsetY) * scale; };
    std::vector<Mesh> scene;

    // Four-cubic circle.
    {
        RenderPath path;
        appendCircle(path, x(120.0f), y(115.0f), 72.0f * scale);
        scene.push_back(buildMesh(path, {0.08f, 0.42f, 0.88f, 1.0f}));
    }

    // A tight arch paired with a shallow return curve.
    {
        RenderPath path;
        path.moveTo(point(285.0f, 175.0f));
        path.cubicTo(point(245.0f, 8.0f), point(475.0f, 8.0f), point(435.0f, 175.0f));
        path.cubicTo(point(410.0f, 225.0f), point(310.0f, 225.0f), point(285.0f, 175.0f));
        path.close();
        scene.push_back(buildMesh(path, {0.95f, 0.48f, 0.06f, 1.0f}));
    }

    // Closed contour with two inflected S-shaped cubic sides.
    {
        RenderPath path;
        path.moveTo(point(535.0f, 72.0f));
        path.cubicTo(point(575.0f, 5.0f), point(625.0f, 205.0f), point(685.0f, 138.0f));
        path.lineTo(point(685.0f, 173.0f));
        path.cubicTo(point(625.0f, 240.0f), point(575.0f, 40.0f), point(535.0f, 107.0f));
        path.close();
        scene.push_back(buildMesh(path, {0.70f, 0.20f, 0.70f, 1.0f}));
    }

    // A lone un-stroked line has no fill area. This narrow butt-cap bar is
    // the filled contour produced by expanding a straight stroke. Its long,
    // shallow-slope edges isolate the analytical LineTo path.
    {
        RenderPath path;
        path.moveTo(point(255.0f, 245.0f));
        path.lineTo(point(465.0f, 230.0f));
        path.lineTo(point(466.5f, 251.0f));
        path.lineTo(point(256.5f, 266.0f));
        path.close();
        scene.push_back(buildMesh(path, {0.10f, 0.55f, 0.68f, 1.0f}));
    }

    // Several consecutive curves in one contour.
    {
        RenderPath path;
        path.moveTo(point(45.0f, 365.0f));
        path.cubicTo(point(42.0f, 285.0f), point(115.0f, 270.0f), point(130.0f, 325.0f));
        path.cubicTo(point(150.0f, 275.0f), point(220.0f, 305.0f), point(205.0f, 375.0f));
        path.cubicTo(point(195.0f, 445.0f), point(125.0f, 455.0f), point(105.0f, 405.0f));
        path.cubicTo(point(75.0f, 445.0f), point(38.0f, 420.0f), point(45.0f, 365.0f));
        path.close();
        scene.push_back(buildMesh(path, {0.08f, 0.63f, 0.37f, 1.0f}));
    }

    // Curves meeting at an acute bottom join.
    {
        RenderPath path;
        path.moveTo(point(360.0f, 455.0f));
        path.cubicTo(point(345.0f, 415.0f), point(270.0f, 390.0f), point(285.0f, 325.0f));
        path.cubicTo(point(300.0f, 270.0f), point(355.0f, 300.0f), point(360.0f, 340.0f));
        path.cubicTo(point(365.0f, 300.0f), point(420.0f, 270.0f), point(435.0f, 325.0f));
        path.cubicTo(point(450.0f, 390.0f), point(375.0f, 415.0f), point(360.0f, 455.0f));
        path.close();
        scene.push_back(buildMesh(path, {0.88f, 0.12f, 0.16f, 1.0f}));
    }

    // A 50%-opaque curved shape makes direct patch overdraw visible.
    {
        RenderPath path;
        path.moveTo(point(525.0f, 385.0f));
        path.cubicTo(point(520.0f, 300.0f), point(585.0f, 275.0f), point(615.0f, 335.0f));
        path.cubicTo(point(650.0f, 275.0f), point(705.0f, 320.0f), point(690.0f, 390.0f));
        path.cubicTo(point(675.0f, 450.0f), point(610.0f, 455.0f), point(590.0f, 410.0f));
        path.cubicTo(point(560.0f, 450.0f), point(525.0f, 430.0f), point(525.0f, 385.0f));
        path.close();
        scene.push_back(buildMesh(path, {0.45f, 0.18f, 0.78f, 0.50f}));
    }

    return scene;
}

std::vector<Mesh> makeComparisonMeshes(float offsetX, float offsetY)
{
    std::vector<Mesh> meshes;
    auto scene = aa_poc::makeComparisonScene(offsetX, offsetY);
    meshes.reserve(scene.size());
    for (const auto& item : scene) {
        meshes.push_back(buildMesh(item.path, {item.r, item.g, item.b, item.a}));
    }
    return meshes;
}

void setCommonUniforms(GLuint program, uint32_t width, uint32_t height)
{
    glUseProgram(program);
    glUniform2f(glGetUniformLocation(program, "uViewport"), static_cast<float>(width), static_cast<float>(height));
}

void setPremultipliedColor(GLuint program, const Color& color)
{
    glUniform4f(glGetUniformLocation(program, "uColor"),
                color.r * color.a, color.g * color.a, color.b * color.a, color.a);
}

void uploadPositions(GLuint vbo, const Point* points, size_t count)
{
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, count * sizeof(Point), points, GL_STREAM_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Point), nullptr);
}

void renderStencil(const Mesh& mesh, const Programs& programs, GLuint vbo, GLuint ebo,
                   uint32_t width, uint32_t height)
{
    glStencilMask(0xff);
    glClearStencil(0);
    glClear(GL_STENCIL_BUFFER_BIT);
    glEnable(GL_STENCIL_TEST);
    glStencilFuncSeparate(GL_FRONT, GL_ALWAYS, 0, 0xff);
    glStencilFuncSeparate(GL_BACK, GL_ALWAYS, 0, 0xff);
    glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_KEEP, GL_INCR_WRAP);
    glStencilOpSeparate(GL_BACK, GL_KEEP, GL_KEEP, GL_DECR_WRAP);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

    setCommonUniforms(programs.solid, width, height);
    uploadPositions(vbo, mesh.points.data(), mesh.points.size());
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.indices.size() * sizeof(uint32_t),
                 mesh.indices.data(), GL_STREAM_DRAW);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(mesh.indices.size()), GL_UNSIGNED_INT, nullptr);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDisable(GL_STENCIL_TEST);
}

void renderBoundary(const Mesh& mesh, const Programs& programs, GLuint vbo,
                    uint32_t width, uint32_t height)
{
    setCommonUniforms(programs.boundary, width, height);
    setPremultipliedColor(programs.boundary, mesh.color);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, mesh.boundary.size() * sizeof(BoundaryVertex),
                 mesh.boundary.data(), GL_STREAM_DRAW);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glEnableVertexAttribArray(3);
    glEnableVertexAttribArray(4);
    glEnableVertexAttribArray(5);
    glEnableVertexAttribArray(6);
    glEnableVertexAttribArray(7);
    glEnableVertexAttribArray(8);
    glEnableVertexAttribArray(9);
    glEnableVertexAttribArray(10);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(BoundaryVertex),
                          reinterpret_cast<const void*>(0));
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(BoundaryVertex),
                          reinterpret_cast<const void*>(2 * sizeof(float)));
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(BoundaryVertex),
                          reinterpret_cast<const void*>(4 * sizeof(float)));
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(BoundaryVertex),
                          reinterpret_cast<const void*>(6 * sizeof(float)));
    glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, sizeof(BoundaryVertex),
                          reinterpret_cast<const void*>(8 * sizeof(float)));
    glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(BoundaryVertex),
                          reinterpret_cast<const void*>(10 * sizeof(float)));
    glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(BoundaryVertex),
                          reinterpret_cast<const void*>(14 * sizeof(float)));
    glVertexAttribPointer(7, 2, GL_FLOAT, GL_FALSE, sizeof(BoundaryVertex),
                          reinterpret_cast<const void*>(18 * sizeof(float)));
    glVertexAttribPointer(8, 3, GL_FLOAT, GL_FALSE, sizeof(BoundaryVertex),
                          reinterpret_cast<const void*>(20 * sizeof(float)));
    glVertexAttribPointer(9, 1, GL_FLOAT, GL_FALSE, sizeof(BoundaryVertex),
                          reinterpret_cast<const void*>(23 * sizeof(float)));
    glVertexAttribPointer(10, 1, GL_FLOAT, GL_FALSE, sizeof(BoundaryVertex),
                          reinterpret_cast<const void*>(24 * sizeof(float)));
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_ALWAYS);
    glDepthMask(GL_TRUE);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(mesh.boundary.size()));
    glDisable(GL_DEPTH_TEST);
}

void renderCover(const Mesh& mesh, const Programs& programs, GLuint vbo, bool excludeBoundary,
                 uint32_t width, uint32_t height)
{
    auto minX = mesh.points[0].x;
    auto minY = mesh.points[0].y;
    auto maxX = mesh.points[0].x;
    auto maxY = mesh.points[0].y;
    for (const auto& point : mesh.points) {
        minX = std::min(minX, point.x);
        minY = std::min(minY, point.y);
        maxX = std::max(maxX, point.x);
        maxY = std::max(maxY, point.y);
    }
    minX = std::floor(minX);
    minY = std::floor(minY);
    maxX = std::ceil(maxX);
    maxY = std::ceil(maxY);

    const Point cover[] = {
        {minX, minY}, {maxX, minY}, {minX, maxY},
        {minX, maxY}, {maxX, minY}, {maxX, maxY}
    };
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_NOTEQUAL, 0, 0xff);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    if (excludeBoundary) {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_GREATER);
        glDepthMask(GL_FALSE);
    }
    setCommonUniforms(programs.solid, width, height);
    setPremultipliedColor(programs.solid, mesh.color);
    uploadPositions(vbo, cover, 6);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
}

bool renderMode(const char* name, bool curveDirect, const std::vector<Mesh>& scene,
                const Programs& programs, GLuint vao, GLuint vbo, GLuint ebo,
                uint32_t width, uint32_t height, const std::string& outputDir)
{
    aa_poc::RenderTarget renderTarget;
    if (!renderTarget.init(width, height, 1, "aa_curve_direct_poc")) return false;

    glBindFramebuffer(GL_FRAMEBUFFER, renderTarget.framebuffer());
    glBindVertexArray(vao);
    glViewport(0, 0, width, height);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_SCISSOR_TEST);
    glScissor(0, 0, width, height);
    glFrontFace(GL_CCW);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glStencilMask(0xff);
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClearStencil(0);
#if defined(AA_POC_GLES)
    glClearDepthf(0.0f);
#else
    glClearDepth(0.0);
#endif
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    for (const auto& mesh : scene) {
        renderStencil(mesh, programs, vbo, ebo, width, height);
        if (curveDirect) {
            // Boundary fragments write only binary depth occupancy. The solid
            // stencil cover then fills everything except this analytical band.
            // GL_ALWAYS deliberately permits adjacent patches to blend twice.
            glDepthMask(GL_TRUE);
            glClear(GL_DEPTH_BUFFER_BIT);
            renderBoundary(mesh, programs, vbo, width, height);
        }
        renderCover(mesh, programs, vbo, curveDirect, width, height);
    }

    auto filename = outputDir + "/" + name + ".png";
    auto result = aa_poc::writeFramebufferPng(
        filename, renderTarget.framebuffer(), width, height, 1, "aa_curve_direct_poc");
    if (result) std::printf("wrote %s\n", filename.c_str());
    return result;
}

} // namespace

int main(int argc, char** argv)
{
    auto comparison = aa_poc::takeComparisonOption(argc, argv);
    aa_poc::RunOptions options;
    options.outputDir = "aa_curve_direct_poc-output";
    if (!aa_poc::parseOptions(argc, argv, options)) {
        aa_poc::printUsage(argv[0], "[--comparison]");
        return EXIT_FAILURE;
    }
    if (!validateImplicitizer()) {
        std::fprintf(stderr, "aa_curve_direct_poc: implicitizer self-check failed\n");
        return EXIT_FAILURE;
    }
    if (!aa_poc::makeOutputDirectory(options.outputDir)) {
        std::fprintf(stderr, "aa_curve_direct_poc: cannot create output directory: %s\n",
                     options.outputDir.c_str());
        return EXIT_FAILURE;
    }

    aa_poc::GlContext context;
    if (!context.init()) {
        std::fprintf(stderr, "aa_curve_direct_poc: failed to create an offscreen GL context\n");
        return EXIT_FAILURE;
    }

    aa_poc::printGlInfo();

    Programs programs;
    if (!createPrograms(programs)) return EXIT_FAILURE;

    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);
    auto renderAt = [&](float offsetX, float offsetY, const std::string& outputDir) {
        auto scene = comparison ? makeComparisonMeshes(offsetX, offsetY) :
                                  makeScene(offsetX, offsetY);
        auto width = comparison ? aa_poc::COMPARISON_WIDTH : NATIVE_WIDTH;
        auto height = comparison ? aa_poc::COMPARISON_HEIGHT : NATIVE_HEIGHT;
        auto result = renderMode("noaa", false, scene, programs, vao, vbo, ebo,
                                 width, height, outputDir);
        result = renderMode("curve-direct", true, scene, programs, vao, vbo, ebo,
                            width, height, outputDir) && result;
        return result;
    };

    auto success = aa_poc::renderOffsets(options, renderAt);

    glDeleteBuffers(1, &ebo);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    destroyPrograms(programs);

    if (success) {
        if (comparison) {
            std::printf("scene: shared comparison\n");
        } else {
            std::printf("scene: circle | high curvature | inflected S | connected curves | sharp join | 50%% opacity | straight line\n");
        }
        std::printf("subpixel offset: %.3f, %.3f\n",
                    static_cast<double>(options.offsetX), static_cast<double>(options.offsetY));
        return EXIT_SUCCESS;
    }
    return EXIT_FAILURE;
}
