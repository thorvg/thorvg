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

#include "aa_reference_poc.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <thorvg.h>
#include "aa_poc_cli.h"
#include "aa_poc_comparison_scene.h"
#include "aa_poc_curve_mask_scene.h"
#include "aa_poc_flat_mask_scene.h"
#include "aa_poc_gl.h"
#include "tvgMath.h"
#include "tvgRender.h"

using namespace tvg;

namespace aa_reference
{
namespace
{

constexpr uint32_t SSAA_SCALE = 8;

enum class SceneKind
{
    FlatDirect,
    CurveDirect,
    FlatMask,
    CurveMask,
    Comparison,
};

struct Color
{
    float r, g, b, a;
};

struct Viewport
{
    int32_t x, y;
    uint32_t width, height;
};

struct ShapeData
{
    RenderPath path;
    Matrix transform;
    Color color;
};

struct Scene
{
    uint32_t width = 0;
    uint32_t height = 0;
    Viewport viewport = {};
    std::vector<ShapeData> shapes;
};

struct Mesh
{
    std::vector<Point> points;
    std::vector<uint32_t> indices;
    Color color;
};

struct Options : aa_poc::RunOptions
{
    SceneKind scene = SceneKind::CurveDirect;
    float renderScale = 1.0f;
};

constexpr Matrix IDENTITY = {1.0f, 0.0f, 0.0f,
                             0.0f, 1.0f, 0.0f,
                             0.0f, 0.0f, 1.0f};

const char* sceneName(SceneKind scene)
{
    switch (scene) {
        case SceneKind::FlatDirect: return "flat-direct";
        case SceneKind::CurveDirect: return "curve-direct";
        case SceneKind::FlatMask: return "flat-mask";
        case SceneKind::CurveMask: return "curve-mask";
        case SceneKind::Comparison: return "comparison";
    }
    return "unknown";
}

Point rotatePoint(float x, float y, float cx, float cy, float radians)
{
    auto cosine = std::cos(radians);
    auto sine = std::sin(radians);
    return {cx + x * cosine - y * sine, cy + x * sine + y * cosine};
}

void appendRotatedRect(RenderPath& path, float cx, float cy, float width, float height, float degrees)
{
    constexpr float DEG_TO_RAD = 0.01745329251994329577f;
    auto radians = degrees * DEG_TO_RAD;
    auto halfWidth = width * 0.5f;
    auto halfHeight = height * 0.5f;
    path.moveTo(rotatePoint(-halfWidth, -halfHeight, cx, cy, radians));
    path.lineTo(rotatePoint(halfWidth, -halfHeight, cx, cy, radians));
    path.lineTo(rotatePoint(halfWidth, halfHeight, cx, cy, radians));
    path.lineTo(rotatePoint(-halfWidth, halfHeight, cx, cy, radians));
    path.close();
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

void appendInflectedCubic(RenderPath& path, float cx, float cy, float scale)
{
    path.moveTo({cx - 50.0f * scale, cy - 25.0f * scale});
    path.cubicTo({cx - 15.0f * scale, cy - 75.0f * scale},
                 {cx + 15.0f * scale, cy + 25.0f * scale},
                 {cx + 50.0f * scale, cy - 25.0f * scale});
    path.lineTo({cx + 50.0f * scale, cy + 25.0f * scale});
    path.cubicTo({cx + 15.0f * scale, cy + 75.0f * scale},
                 {cx - 15.0f * scale, cy - 25.0f * scale},
                 {cx - 50.0f * scale, cy + 25.0f * scale});
    path.close();
}

void addShape(Scene& scene, const RenderPath& path, const Color& color,
              const Matrix& transform = IDENTITY)
{
    scene.shapes.push_back({path, transform, color});
}

Scene makeFlatDirectScene(float offsetX, float offsetY, float scale)
{
    constexpr uint32_t WIDTH = 640;
    constexpr uint32_t HEIGHT = 360;
    Scene scene;
    scene.width = static_cast<uint32_t>(WIDTH * scale);
    scene.height = static_cast<uint32_t>(HEIGHT * scale);
    scene.viewport = {0, 0, scene.width, scene.height};
    auto x = [=](float value) { return (value + offsetX) * scale; };
    auto y = [=](float value) { return (value + offsetY) * scale; };

    {
        RenderPath path;
        appendRotatedRect(path, x(75.0f), y(80.0f), 100.0f * scale, 50.0f * scale, 15.0f);
        addShape(scene, path, {0.08f, 0.42f, 0.88f, 1.0f});
    }
    {
        RenderPath path;
        appendRotatedRect(path, x(220.0f), y(80.0f), 100.0f * scale, 50.0f * scale, 45.0f);
        addShape(scene, path, {0.95f, 0.48f, 0.06f, 1.0f});
    }
    {
        RenderPath path;
        appendCircle(path, x(365.0f), y(80.0f), 35.0f * scale);
        addShape(scene, path, {0.95f, 0.48f, 0.06f, 1.0f});
    }
    {
        RenderPath path;
        appendInflectedCubic(path, x(530.0f), y(80.0f), scale);
        addShape(scene, path, {0.70f, 0.20f, 0.70f, 1.0f});
    }
    {
        RenderPath path;
        appendRotatedRect(path, x(95.0f), y(260.0f), 140.0f * scale, 0.5f * scale, -15.0f);
        addShape(scene, path, {0.08f, 0.63f, 0.37f, 1.0f});
    }
    {
        RenderPath path;
        appendRotatedRect(path, x(245.0f), y(260.0f), 140.0f * scale, 1.0f * scale, -15.0f);
        addShape(scene, path, {0.08f, 0.50f, 0.70f, 1.0f});
    }
    {
        RenderPath path;
        path.moveTo({x(380.0f), y(195.0f)});
        path.lineTo({x(435.0f), y(320.0f)});
        path.lineTo({x(325.0f), y(320.0f)});
        path.close();
        addShape(scene, path, {0.88f, 0.12f, 0.16f, 1.0f});
    }
    {
        RenderPath path;
        appendRotatedRect(path, x(535.0f), y(260.0f), 150.0f * scale, 75.0f * scale, -12.0f);
        addShape(scene, path, {0.45f, 0.18f, 0.78f, 0.50f});
    }
    return scene;
}

Scene makeCurveDirectScene(float offsetX, float offsetY, float scale)
{
    constexpr uint32_t WIDTH = 720;
    constexpr uint32_t HEIGHT = 480;
    Scene scene;
    scene.width = static_cast<uint32_t>(WIDTH * scale);
    scene.height = static_cast<uint32_t>(HEIGHT * scale);
    scene.viewport = {0, 0, scene.width, scene.height};
    auto point = [=](float x, float y) {
        return Point{(x + offsetX) * scale, (y + offsetY) * scale};
    };
    auto x = [=](float value) { return (value + offsetX) * scale; };
    auto y = [=](float value) { return (value + offsetY) * scale; };

    {
        RenderPath path;
        appendCircle(path, x(120.0f), y(115.0f), 72.0f * scale);
        addShape(scene, path, {0.08f, 0.42f, 0.88f, 1.0f});
    }
    {
        RenderPath path;
        path.moveTo(point(285.0f, 175.0f));
        path.cubicTo(point(245.0f, 8.0f), point(475.0f, 8.0f), point(435.0f, 175.0f));
        path.cubicTo(point(410.0f, 225.0f), point(310.0f, 225.0f), point(285.0f, 175.0f));
        path.close();
        addShape(scene, path, {0.95f, 0.48f, 0.06f, 1.0f});
    }
    {
        RenderPath path;
        path.moveTo(point(535.0f, 72.0f));
        path.cubicTo(point(575.0f, 5.0f), point(625.0f, 205.0f), point(685.0f, 138.0f));
        path.lineTo(point(685.0f, 173.0f));
        path.cubicTo(point(625.0f, 240.0f), point(575.0f, 40.0f), point(535.0f, 107.0f));
        path.close();
        addShape(scene, path, {0.70f, 0.20f, 0.70f, 1.0f});
    }
    {
        RenderPath path;
        path.moveTo(point(255.0f, 245.0f));
        path.lineTo(point(465.0f, 230.0f));
        path.lineTo(point(466.5f, 251.0f));
        path.lineTo(point(256.5f, 266.0f));
        path.close();
        addShape(scene, path, {0.10f, 0.55f, 0.68f, 1.0f});
    }
    {
        RenderPath path;
        path.moveTo(point(45.0f, 365.0f));
        path.cubicTo(point(42.0f, 285.0f), point(115.0f, 270.0f), point(130.0f, 325.0f));
        path.cubicTo(point(150.0f, 275.0f), point(220.0f, 305.0f), point(205.0f, 375.0f));
        path.cubicTo(point(195.0f, 445.0f), point(125.0f, 455.0f), point(105.0f, 405.0f));
        path.cubicTo(point(75.0f, 445.0f), point(38.0f, 420.0f), point(45.0f, 365.0f));
        path.close();
        addShape(scene, path, {0.08f, 0.63f, 0.37f, 1.0f});
    }
    {
        RenderPath path;
        path.moveTo(point(360.0f, 455.0f));
        path.cubicTo(point(345.0f, 415.0f), point(270.0f, 390.0f), point(285.0f, 325.0f));
        path.cubicTo(point(300.0f, 270.0f), point(355.0f, 300.0f), point(360.0f, 340.0f));
        path.cubicTo(point(365.0f, 300.0f), point(420.0f, 270.0f), point(435.0f, 325.0f));
        path.cubicTo(point(450.0f, 390.0f), point(375.0f, 415.0f), point(360.0f, 455.0f));
        path.close();
        addShape(scene, path, {0.88f, 0.12f, 0.16f, 1.0f});
    }
    {
        RenderPath path;
        path.moveTo(point(525.0f, 385.0f));
        path.cubicTo(point(520.0f, 300.0f), point(585.0f, 275.0f), point(615.0f, 335.0f));
        path.cubicTo(point(650.0f, 275.0f), point(705.0f, 320.0f), point(690.0f, 390.0f));
        path.cubicTo(point(675.0f, 450.0f), point(610.0f, 455.0f), point(590.0f, 410.0f));
        path.cubicTo(point(560.0f, 450.0f), point(525.0f, 430.0f), point(525.0f, 385.0f));
        path.close();
        addShape(scene, path, {0.45f, 0.18f, 0.78f, 0.50f});
    }
    return scene;
}

Scene makeFlatMaskSceneLayout(float scale)
{
    constexpr uint32_t WIDTH = aa_poc::FLAT_MASK_WIDTH;
    constexpr uint32_t HEIGHT = aa_poc::FLAT_MASK_HEIGHT;
    Scene scene;
    scene.width = static_cast<uint32_t>(WIDTH * scale);
    scene.height = static_cast<uint32_t>(HEIGHT * scale);
    scene.viewport = {static_cast<int32_t>(8.0f * scale), static_cast<int32_t>(8.0f * scale),
                      static_cast<uint32_t>((WIDTH - 16) * scale),
                      static_cast<uint32_t>((HEIGHT - 16) * scale)};
    return scene;
}

Scene makeCurveMaskSceneLayout(float scale)
{
    constexpr uint32_t WIDTH = aa_poc::CURVE_MASK_WIDTH;
    constexpr uint32_t HEIGHT = aa_poc::CURVE_MASK_HEIGHT;
    Scene scene;
    scene.width = static_cast<uint32_t>(WIDTH * scale);
    scene.height = static_cast<uint32_t>(HEIGHT * scale);
    scene.viewport = {static_cast<int32_t>(8.0f * scale), static_cast<int32_t>(8.0f * scale),
                      static_cast<uint32_t>((WIDTH - 16) * scale),
                      static_cast<uint32_t>((HEIGHT - 16) * scale)};
    return scene;
}

Scene makeComparisonReferenceScene(float offsetX, float offsetY, float scale)
{
    Scene scene;
    scene.width = static_cast<uint32_t>(aa_poc::COMPARISON_WIDTH * scale);
    scene.height = static_cast<uint32_t>(aa_poc::COMPARISON_HEIGHT * scale);
    scene.viewport = {0, 0, scene.width, scene.height};

    for (const auto& shape : aa_poc::makeComparisonScene(offsetX, offsetY, scale)) {
        addShape(scene, shape.path, {shape.r, shape.g, shape.b, shape.a});
    }
    return scene;
}

Scene makeScene(SceneKind kind, float offsetX, float offsetY, float scale)
{
    switch (kind) {
        case SceneKind::FlatDirect: return makeFlatDirectScene(offsetX, offsetY, scale);
        case SceneKind::CurveDirect: return makeCurveDirectScene(offsetX, offsetY, scale);
        case SceneKind::FlatMask: return makeFlatMaskSceneLayout(scale);
        case SceneKind::CurveMask: return makeCurveMaskSceneLayout(scale);
        case SceneKind::Comparison: return makeComparisonReferenceScene(offsetX, offsetY, scale);
    }
    return {};
}

Point transformPoint(const Matrix& matrix, const Point& point)
{
    return {matrix.e11 * point.x + matrix.e12 * point.y + matrix.e13,
            matrix.e21 * point.x + matrix.e22 * point.y + matrix.e23};
}

Mesh flattenAndTessellate(const ShapeData& shape)
{
    // This is the normal ThorVG GL fill walk: cubic curves are subdivided with
    // Bezier::segments()/at() and each contour is emitted as a stencil fan.
    // Reference paths are rebuilt before this function at their device scale.
    Mesh mesh;
    mesh.color = shape.color;
    auto points = shape.path.pts.data;
    Point previous = {};
    uint32_t contourFirstIndex = 0;
    uint32_t contourPointCount = 0;

    auto beginContour = [&](const Point& point) {
        contourFirstIndex = static_cast<uint32_t>(mesh.points.size());
        contourPointCount = 1;
        previous = point;
        mesh.points.push_back(point);
    };

    auto appendPoint = [&](const Point& point) {
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

    for (auto command = shape.path.cmds.begin(); command != shape.path.cmds.end(); ++command) {
        switch (*command) {
            case PathCommand::MoveTo:
                beginContour(transformPoint(shape.transform, *points++));
                break;
            case PathCommand::LineTo:
                appendPoint(transformPoint(shape.transform, *points++));
                break;
            case PathCommand::CubicTo: {
                auto control1 = transformPoint(shape.transform, points[0]);
                auto control2 = transformPoint(shape.transform, points[1]);
                auto end = transformPoint(shape.transform, points[2]);
                Bezier curve{previous, control1, control2, end};
                auto count = std::max(curve.segments(), 2u);
                auto step = 1.0f / count;
                for (uint32_t i = 1; i <= count; ++i) appendPoint(curve.at(step * i));
                points += 3;
                break;
            }
            case PathCommand::Close:
                break;
        }
    }
    return mesh;
}

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

#undef AA_POC_GLSL_HEADER

GLuint createProgram()
{
    constexpr aa_poc::AttributeBinding ATTRIBUTES[] = {
        {0, "aPosition"},
    };
    return aa_poc::createProgram(
        SOLID_VERTEX_SHADER, SOLID_FRAGMENT_SHADER,
        ATTRIBUTES, sizeof(ATTRIBUTES) / sizeof(ATTRIBUTES[0]),
        "aa_reference_poc");
}

void setViewportUniform(GLuint program, uint32_t width, uint32_t height)
{
    glUseProgram(program);
    glUniform2f(glGetUniformLocation(program, "uViewport"),
                static_cast<float>(width), static_cast<float>(height));
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

void renderStencil(const Mesh& mesh, GLuint program, GLuint vbo, GLuint ebo,
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

    setViewportUniform(program, width, height);
    uploadPositions(vbo, mesh.points.data(), mesh.points.size());
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.indices.size() * sizeof(uint32_t),
                 mesh.indices.data(), GL_STREAM_DRAW);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(mesh.indices.size()), GL_UNSIGNED_INT, nullptr);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
}

void renderCover(const Mesh& mesh, GLuint program, GLuint vbo, uint32_t width, uint32_t height)
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

    glStencilFunc(GL_NOTEQUAL, 0, 0xff);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    setViewportUniform(program, width, height);
    setPremultipliedColor(program, mesh.color);
    uploadPositions(vbo, cover, 6);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDisable(GL_STENCIL_TEST);
}

bool renderThorvgMaskReference(aa_poc::GlContext& context, Mode mode,
                               SceneKind sceneKind, float offsetX, float offsetY,
                               const std::string& filename)
{
    auto baseWidth = sceneKind == SceneKind::CurveMask ?
        aa_poc::CURVE_MASK_WIDTH : aa_poc::FLAT_MASK_WIDTH;
    auto baseHeight = sceneKind == SceneKind::CurveMask ?
        aa_poc::CURVE_MASK_HEIGHT : aa_poc::FLAT_MASK_HEIGHT;
    auto scale = mode == Mode::Ssaa8 ? static_cast<float>(SSAA_SCALE) : 1.0f;
    auto width = static_cast<uint32_t>(baseWidth * scale);
    auto height = static_cast<uint32_t>(baseHeight * scale);
    auto samples = mode == Mode::Msaa4 ? 4u : 1u;
    auto downsample = mode == Mode::Ssaa8 ? SSAA_SCALE : 1u;

    aa_poc::RenderTarget renderTarget;
    aa_poc::RenderTarget resolveTarget;
    if (!renderTarget.init(width, height, samples, "aa_reference_poc")) return false;
    if (samples > 1 && !resolveTarget.init(width, height, 1, "aa_reference_poc")) return false;

    auto canvas = std::unique_ptr<GlCanvas>(GlCanvas::gen(EngineOption::Default));
    if (!canvas) {
        std::fprintf(stderr, "aa_reference_poc: GlCanvas::gen() failed\n");
        return false;
    }

    auto success = aa_poc::checkThorvg(
        context.target(*canvas, renderTarget.framebuffer(), width, height,
                       ColorSpace::ABGR8888S),
        "GlCanvas::target", "aa_reference_poc");
    auto viewportScale = static_cast<int32_t>(scale);
    if (success) {
        success = aa_poc::checkThorvg(
            canvas->viewport(8 * viewportScale, 8 * viewportScale,
                             static_cast<int32_t>(width) - 16 * viewportScale,
                             static_cast<int32_t>(height) - 16 * viewportScale),
            "Canvas::viewport", "aa_reference_poc");
    }
    if (success) {
        if (sceneKind == SceneKind::CurveMask) {
            success = aa_poc::populateCurveMaskScene(
                *canvas, offsetX, offsetY, scale, "aa_reference_poc");
        } else {
            success = aa_poc::populateFlatMaskScene(
                *canvas, offsetX, offsetY, scale, "aa_reference_poc");
        }
    }
    if (success) {
        success = aa_poc::checkThorvg(canvas->update(), "Canvas::update", "aa_reference_poc");
    }
    if (success) {
        glDisable(GL_DEPTH_TEST);
        aa_poc::clearFramebuffer(renderTarget.framebuffer(), width, height);
        success = aa_poc::checkThorvg(canvas->draw(false), "Canvas::draw", "aa_reference_poc");
    }
    if (success) {
        success = aa_poc::checkThorvg(canvas->sync(), "Canvas::sync", "aa_reference_poc");
    }
    canvas.reset();

    GLuint readTarget = renderTarget.framebuffer();
    if (success && samples > 1) {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, renderTarget.framebuffer());
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolveTarget.framebuffer());
        glBlitFramebuffer(0, 0, width, height, 0, 0, width, height,
                          GL_COLOR_BUFFER_BIT, GL_NEAREST);
        readTarget = resolveTarget.framebuffer();
    }

    if (success) {
        success = aa_poc::writeFramebufferPng(
            filename, readTarget, width, height, downsample, "aa_reference_poc");
    }
    if (success) std::printf("wrote %s\n", filename.c_str());
    return success;
}

bool renderReference(const Scene& scene, uint32_t samples, uint32_t downsample,
                     GLuint program, GLuint vao, GLuint vbo, GLuint ebo,
                     const std::string& filename)
{
    aa_poc::RenderTarget renderTarget;
    aa_poc::RenderTarget resolveTarget;
    if (!renderTarget.init(scene.width, scene.height, samples, "aa_reference_poc")) return false;
    if (samples > 1 &&
        !resolveTarget.init(scene.width, scene.height, 1, "aa_reference_poc")) return false;

    glBindFramebuffer(GL_FRAMEBUFFER, renderTarget.framebuffer());
    glBindVertexArray(vao);
    glDisable(GL_DEPTH_TEST);
    aa_poc::clearFramebuffer(renderTarget.framebuffer(), scene.width, scene.height);
    glEnable(GL_SCISSOR_TEST);
    glScissor(scene.viewport.x, scene.viewport.y,
              static_cast<GLsizei>(scene.viewport.width),
              static_cast<GLsizei>(scene.viewport.height));
    glDisable(GL_CULL_FACE);
    glFrontFace(GL_CCW);
#if !defined(AA_POC_GLES)
    glEnable(GL_MULTISAMPLE);
#endif
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    for (const auto& shape : scene.shapes) {
        auto mesh = flattenAndTessellate(shape);
        renderStencil(mesh, program, vbo, ebo, scene.width, scene.height);
        renderCover(mesh, program, vbo, scene.width, scene.height);
    }

    GLuint readTarget = renderTarget.framebuffer();
    if (samples > 1) {
        glDisable(GL_SCISSOR_TEST);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, renderTarget.framebuffer());
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolveTarget.framebuffer());
        glBlitFramebuffer(0, 0, scene.width, scene.height,
                          0, 0, scene.width, scene.height,
                          GL_COLOR_BUFFER_BIT, GL_NEAREST);
        readTarget = resolveTarget.framebuffer();
    }

    auto success = aa_poc::writeFramebufferPng(
        filename, readTarget, scene.width, scene.height, downsample, "aa_reference_poc");
    if (success) std::printf("wrote %s\n", filename.c_str());
    return success;
}

bool parseScene(const char* text, SceneKind& scene)
{
    if (std::strcmp(text, "flat-direct") == 0) scene = SceneKind::FlatDirect;
    else if (std::strcmp(text, "curve-direct") == 0) scene = SceneKind::CurveDirect;
    else if (std::strcmp(text, "flat-mask") == 0) scene = SceneKind::FlatMask;
    else if (std::strcmp(text, "curve-mask") == 0) scene = SceneKind::CurveMask;
    else if (std::strcmp(text, "comparison") == 0) scene = SceneKind::Comparison;
    else return false;
    return true;
}

bool parseOptions(int argc, char** argv, Options& options)
{
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--scene") == 0) {
            if (i + 1 >= argc) return false;
            if (!parseScene(argv[++i], options.scene)) return false;
        } else if (std::strcmp(argv[i], "--render-scale") == 0) {
            if (i + 1 >= argc ||
                !aa_poc::parseFloat(argv[++i], options.renderScale) ||
                options.renderScale <= 0.0f) {
                return false;
            }
        } else {
            auto result = aa_poc::parseCommonOption(i, argc, argv, options);
            if (result != aa_poc::ParseOptionResult::Matched) return false;
        }
    }
    return aa_poc::validOptions(options) &&
           (options.scene == SceneKind::Comparison || options.renderScale == 1.0f);
}

} // namespace

int run(Mode mode, int argc, char** argv)
{
    Options options;
    options.outputDir = mode == Mode::Msaa4 ? "aa_msaa4_poc-output" : "aa_ssaa8_poc-output";
    if (!parseOptions(argc, argv, options)) {
        aa_poc::printUsage(
            argv[0], "[--scene flat-direct|curve-direct|flat-mask|curve-mask|comparison] "
                     "[--render-scale S]");
        return EXIT_FAILURE;
    }
    if (!aa_poc::makeOutputDirectory(options.outputDir)) {
        std::fprintf(stderr, "aa_reference_poc: cannot create output directory: %s\n",
                     options.outputDir.c_str());
        return EXIT_FAILURE;
    }

    aa_poc::GlContext context;
    if (!context.init()) {
        std::fprintf(stderr, "aa_reference_poc: failed to create an offscreen GL context\n");
        return EXIT_FAILURE;
    }

    aa_poc::printGlInfo();

    if (mode == Mode::Msaa4) {
        GLint maxSamples = 0;
        glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
        if (maxSamples < 4) {
            std::fprintf(stderr,
                         "aa_msaa4_poc: GL implementation supports only %d samples; 4 are required\n",
                         maxSamples);
            return EXIT_FAILURE;
        }
    } else {
        auto scaledScene = makeScene(options.scene, 0.0f, 0.0f,
                                     options.renderScale * SSAA_SCALE);
        GLint maxRenderbufferSize = 0;
        glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &maxRenderbufferSize);
        if (maxRenderbufferSize < static_cast<GLint>(scaledScene.width) ||
            maxRenderbufferSize < static_cast<GLint>(scaledScene.height)) {
            std::fprintf(stderr,
                         "aa_ssaa8_poc: GL_MAX_RENDERBUFFER_SIZE=%d is too small for %ux%u\n",
                         maxRenderbufferSize, scaledScene.width, scaledScene.height);
            return EXIT_FAILURE;
        }
    }

    auto thorvgScene = options.scene == SceneKind::FlatMask ||
                       options.scene == SceneKind::CurveMask;
    if (thorvgScene && Initializer::init() != Result::Success) {
        std::fprintf(stderr, "aa_reference_poc: ThorVG initialization failed\n");
        return EXIT_FAILURE;
    }

    GLuint program = 0;
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    if (!thorvgScene) {
        program = createProgram();
        if (!program) return EXIT_FAILURE;
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ebo);
    }

    auto renderAt = [&](float offsetX, float offsetY, const std::string& outputDir) {
        auto filename = outputDir + (mode == Mode::Msaa4 ? "/msaa4.png" : "/ssaa8.png");
        if (thorvgScene) {
            return renderThorvgMaskReference(
                context, mode, options.scene, offsetX, offsetY, filename);
        }
        auto downsample = mode == Mode::Ssaa8 ? SSAA_SCALE : 1u;
        auto scale = options.renderScale * downsample;
        auto sceneOffsetX = options.scene == SceneKind::Comparison ?
                            offsetX / options.renderScale : offsetX;
        auto sceneOffsetY = options.scene == SceneKind::Comparison ?
                            offsetY / options.renderScale : offsetY;
        auto scene = makeScene(options.scene, sceneOffsetX, sceneOffsetY, scale);
        return renderReference(scene, mode == Mode::Msaa4 ? 4u : 1u,
                               downsample,
                               program, vao, vbo, ebo, filename);
    };

    auto success = aa_poc::renderOffsets(options, renderAt);

    if (thorvgScene) {
        if (Initializer::term() != Result::Success) {
            std::fprintf(stderr, "aa_reference_poc: ThorVG termination failed\n");
            success = false;
        }
    } else {
        glDeleteBuffers(1, &ebo);
        glDeleteBuffers(1, &vbo);
        glDeleteVertexArrays(1, &vao);
        glDeleteProgram(program);
    }

    if (success) {
        std::printf("scene: %s\n", sceneName(options.scene));
        std::printf("subpixel offset: %.3f, %.3f\n",
                    static_cast<double>(options.offsetX), static_cast<double>(options.offsetY));
        return EXIT_SUCCESS;
    }
    return EXIT_FAILURE;
}

} // namespace aa_reference
