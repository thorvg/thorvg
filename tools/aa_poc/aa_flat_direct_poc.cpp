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

constexpr uint32_t NATIVE_WIDTH = 640;
constexpr uint32_t NATIVE_HEIGHT = 360;
constexpr float STRIP_HALF_WIDTH = 0.5f;

struct Color
{
    float r, g, b, a;
};

struct BoundaryVertex
{
    float x, y;
    float ax, ay;
    float bx, by;
};

struct Mesh
{
    std::vector<Point> points;
    std::vector<uint32_t> indices;
    std::vector<BoundaryVertex> boundary;
    Color color;
    float insideSign = 1.0f;
    bool convex = false;
};

struct Programs
{
    GLuint solid = 0;
    GLuint boundary = 0;
};

#if defined(AA_POC_GLES)
#define AA_POC_GLSL_HEADER "#version 300 es\nprecision highp float;\n"
#else
// Match ThorVG's desktop GL path: OpenGL 3.3 and GLSL 3.30.
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
in vec2 aStart;
in vec2 aEnd;
uniform vec2 uViewport;
flat out vec2 vStart;
flat out vec2 vEnd;
void main()
{
    vec2 ndc = vec2(aPosition.x * 2.0 / uViewport.x - 1.0,
                    1.0 - aPosition.y * 2.0 / uViewport.y);
    gl_Position = vec4(ndc, 1.0, 1.0);
    vStart = aStart;
    vEnd = aEnd;
}
)";

constexpr const char* BOUNDARY_FRAGMENT_SHADER = AA_POC_GLSL_HEADER R"(
flat in vec2 vStart;
flat in vec2 vEnd;
uniform vec2 uViewport;
uniform vec4 uColor;
uniform float uInsideSign;
out vec4 fragColor;
void main()
{
    vec2 pixel = vec2(gl_FragCoord.x, uViewport.y - gl_FragCoord.y);
    vec2 edge = vEnd - vStart;
    float edgeLength2 = dot(edge, edge);
    float t = clamp(dot(pixel - vStart, edge) / edgeLength2, 0.0, 1.0);
    float distancePx = length(pixel - (vStart + t * edge));
    float crossValue = (edge.x * (pixel.y - vStart.y) - edge.y * (pixel.x - vStart.x)) * uInsideSign;
    float side = crossValue < 0.0 ? -1.0 : 1.0;
    float coverage = clamp(0.5 + side * distancePx, 0.0, 1.0);
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
        {1, "aStart"},
        {2, "aEnd"},
    };
    programs.solid = aa_poc::createProgram(
        SOLID_VERTEX_SHADER, SOLID_FRAGMENT_SHADER,
        SOLID_ATTRIBUTES, sizeof(SOLID_ATTRIBUTES) / sizeof(SOLID_ATTRIBUTES[0]),
        "aa_flat_direct_poc");
    programs.boundary = aa_poc::createProgram(
        BOUNDARY_VERTEX_SHADER, BOUNDARY_FRAGMENT_SHADER,
        BOUNDARY_ATTRIBUTES, sizeof(BOUNDARY_ATTRIBUTES) / sizeof(BOUNDARY_ATTRIBUTES[0]),
        "aa_flat_direct_poc");
    return programs.solid && programs.boundary;
}

void destroyPrograms(Programs& programs)
{
    if (programs.solid) glDeleteProgram(programs.solid);
    if (programs.boundary) glDeleteProgram(programs.boundary);
    programs = {};
}

void appendBoundarySegment(std::vector<BoundaryVertex>& vertices, const Point& a, const Point& b)
{
    auto dx = b.x - a.x;
    auto dy = b.y - a.y;
    auto length = std::sqrt(dx * dx + dy * dy);
    if (length <= 0.0001f) return;

    auto tx = dx / length;
    auto ty = dy / length;
    auto nx = -ty;
    auto ny = tx;
    auto startX = a.x - tx * STRIP_HALF_WIDTH;
    auto startY = a.y - ty * STRIP_HALF_WIDTH;
    auto endX = b.x + tx * STRIP_HALF_WIDTH;
    auto endY = b.y + ty * STRIP_HALF_WIDTH;

    BoundaryVertex v0{startX + nx * STRIP_HALF_WIDTH, startY + ny * STRIP_HALF_WIDTH, a.x, a.y, b.x, b.y};
    BoundaryVertex v1{startX - nx * STRIP_HALF_WIDTH, startY - ny * STRIP_HALF_WIDTH, a.x, a.y, b.x, b.y};
    BoundaryVertex v2{endX + nx * STRIP_HALF_WIDTH, endY + ny * STRIP_HALF_WIDTH, a.x, a.y, b.x, b.y};
    BoundaryVertex v3{endX - nx * STRIP_HALF_WIDTH, endY - ny * STRIP_HALF_WIDTH, a.x, a.y, b.x, b.y};
    vertices.push_back(v0);
    vertices.push_back(v1);
    vertices.push_back(v2);
    vertices.push_back(v1);
    vertices.push_back(v3);
    vertices.push_back(v2);
}

Mesh flattenAndTessellate(const RenderPath& path, const Color& color, bool convex = true)
{
    // Mirror BWTessellator's fill walk so the stencil fan and boundary share the
    // same ThorVG Bezier::segments()/at() flattened points. Emit the strip in
    // this walk as well: a production integration should not scan the contour
    // again on the render thread.
    Mesh mesh;
    mesh.color = color;
    auto pts = path.pts.data;
    Point previous = {};
    Point contourFirst = {};
    uint32_t contourFirstIndex = 0;
    uint32_t contourPointCount = 0;
    float twiceArea = 0.0f;
    bool contourOpen = false;

    auto finishContour = [&]() {
        if (!contourOpen) return;
        appendBoundarySegment(mesh.boundary, previous, contourFirst);
        twiceArea += previous.x * contourFirst.y - contourFirst.x * previous.y;
        contourOpen = false;
    };

    auto beginContour = [&](const Point& point) {
        finishContour();
        contourFirst = previous = point;
        contourFirstIndex = static_cast<uint32_t>(mesh.points.size());
        contourPointCount = 1;
        contourOpen = true;
        mesh.points.push_back(point);
    };

    auto appendPoint = [&](const Point& point) {
        appendBoundarySegment(mesh.boundary, previous, point);
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
            case PathCommand::LineTo:
                appendPoint(*pts++);
                break;
            case PathCommand::CubicTo: {
                Bezier curve{previous, pts[0], pts[1], pts[2]};
                auto count = std::max(curve.segments(), 2u);
                auto step = 1.0f / count;
                for (uint32_t i = 1; i <= count; ++i) appendPoint(curve.at(step * i));
                pts += 3;
                break;
            }
            case PathCommand::Close:
                finishContour();
                break;
        }
    }
    finishContour();
    mesh.insideSign = twiceArea >= 0.0f ? 1.0f : -1.0f;
    mesh.convex = convex;
    return mesh;
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
    auto hw = width * 0.5f;
    auto hh = height * 0.5f;
    path.moveTo(rotatePoint(-hw, -hh, cx, cy, radians));
    path.lineTo(rotatePoint(hw, -hh, cx, cy, radians));
    path.lineTo(rotatePoint(hw, hh, cx, cy, radians));
    path.lineTo(rotatePoint(-hw, hh, cx, cy, radians));
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

std::vector<Mesh> makeScene(float offsetX, float offsetY, float scale = 1.0f)
{
    auto x = [=](float value) { return (value + offsetX) * scale; };
    auto y = [=](float value) { return (value + offsetY) * scale; };
    std::vector<Mesh> scene;
    {
        RenderPath path;
        appendRotatedRect(path, x(75.0f), y(80.0f), 100.0f * scale, 50.0f * scale, 15.0f);
        scene.push_back(flattenAndTessellate(path, {0.08f, 0.42f, 0.88f, 1.0f}));
    }
    {
        RenderPath path;
        appendRotatedRect(path, x(220.0f), y(80.0f), 100.0f * scale, 50.0f * scale, 45.0f);
        scene.push_back(flattenAndTessellate(path, {0.95f, 0.48f, 0.06f, 1.0f}));
    }
    {
        RenderPath path;
        appendCircle(path, x(365.0f), y(80.0f), 35.0f * scale);
        scene.push_back(flattenAndTessellate(path, {0.95f, 0.48f, 0.06f, 1.0f}));
    }
    {
        RenderPath path;
        appendInflectedCubic(path, x(530.0f), y(80.0f), scale);
        scene.push_back(flattenAndTessellate(path, {0.70f, 0.20f, 0.70f, 1.0f}, false));
    }
    {
        RenderPath path;
        appendRotatedRect(path, x(95.0f), y(260.0f), 140.0f * scale, 0.5f * scale, -15.0f);
        scene.push_back(flattenAndTessellate(path, {0.08f, 0.63f, 0.37f, 1.0f}));
    }
    {
        RenderPath path;
        appendRotatedRect(path, x(245.0f), y(260.0f), 140.0f * scale, 1.0f * scale, -15.0f);
        scene.push_back(flattenAndTessellate(path, {0.08f, 0.50f, 0.70f, 1.0f}));
    }
    {
        RenderPath path;
        path.moveTo({x(380.0f), y(195.0f)});
        path.lineTo({x(435.0f), y(320.0f)});
        path.lineTo({x(325.0f), y(320.0f)});
        path.close();
        scene.push_back(flattenAndTessellate(path, {0.88f, 0.12f, 0.16f, 1.0f}));
    }
    {
        RenderPath path;
        appendRotatedRect(path, x(535.0f), y(260.0f), 150.0f * scale, 75.0f * scale, -12.0f);
        scene.push_back(flattenAndTessellate(path, {0.45f, 0.18f, 0.78f, 0.50f}));
    }
    return scene;
}

std::vector<Mesh> makeComparisonMeshes(float offsetX, float offsetY)
{
    std::vector<Mesh> meshes;
    auto scene = aa_poc::makeComparisonScene(offsetX, offsetY);
    meshes.reserve(scene.size());
    for (const auto& item : scene) {
        // Use the stencil path for every shared shape. This avoids granting
        // flat-direct a hand-authored convexity hint unavailable to the other
        // candidates while preserving the exact same source contours.
        meshes.push_back(flattenAndTessellate(
            item.path, {item.r, item.g, item.b, item.a}, false));
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
    glUniform4f(glGetUniformLocation(program, "uColor"), color.r * color.a, color.g * color.a, color.b * color.a, color.a);
}

void uploadPositions(GLuint vbo, const Point* points, size_t count)
{
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, count * sizeof(Point), points, GL_STREAM_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Point), nullptr);
}

void renderStencil(const Mesh& mesh, const Programs& programs, GLuint vbo, GLuint ebo, uint32_t width, uint32_t height)
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
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.indices.size() * sizeof(uint32_t), mesh.indices.data(), GL_STREAM_DRAW);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(mesh.indices.size()), GL_UNSIGNED_INT, nullptr);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDisable(GL_STENCIL_TEST);
}

void renderDirect(const Mesh& mesh, const Programs& programs, GLuint vbo, GLuint ebo, uint32_t width, uint32_t height)
{
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_GREATER);
    glDepthMask(GL_FALSE);
    setCommonUniforms(programs.solid, width, height);
    setPremultipliedColor(programs.solid, mesh.color);
    uploadPositions(vbo, mesh.points.data(), mesh.points.size());
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.indices.size() * sizeof(uint32_t), mesh.indices.data(), GL_STREAM_DRAW);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(mesh.indices.size()), GL_UNSIGNED_INT, nullptr);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
}

void renderCover(const Mesh& mesh, const Programs& programs, GLuint vbo, bool excludeBoundary, uint32_t width, uint32_t height)
{
    float minX = mesh.points[0].x;
    float minY = mesh.points[0].y;
    float maxX = mesh.points[0].x;
    float maxY = mesh.points[0].y;
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

void renderBoundary(const Mesh& mesh, const Programs& programs, GLuint vbo, uint32_t width, uint32_t height)
{
    setCommonUniforms(programs.boundary, width, height);
    setPremultipliedColor(programs.boundary, mesh.color);
    glUniform1f(glGetUniformLocation(programs.boundary, "uInsideSign"), mesh.insideSign);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, mesh.boundary.size() * sizeof(BoundaryVertex), mesh.boundary.data(), GL_STREAM_DRAW);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(BoundaryVertex), reinterpret_cast<const void*>(0));
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(BoundaryVertex), reinterpret_cast<const void*>(2 * sizeof(float)));
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(BoundaryVertex), reinterpret_cast<const void*>(4 * sizeof(float)));
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_ALWAYS);
    glDepthMask(GL_TRUE);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(mesh.boundary.size()));
    glDisable(GL_DEPTH_TEST);
}

bool renderMode(const char* name, bool flatDirect, const std::vector<Mesh>& scene,
                const Programs& programs, GLuint vao, GLuint vbo, GLuint ebo,
                uint32_t width, uint32_t height, const std::string& outputDir)
{
    aa_poc::RenderTarget renderTarget;
    if (!renderTarget.init(width, height, 1, "aa_flat_direct_poc")) return false;

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
        if (flatDirect) {
            // Binary depth occupancy only keeps the solid cover out of the strip;
            // fractional coverage is painted directly and never accumulated.
            glDepthMask(GL_TRUE);
            glClear(GL_DEPTH_BUFFER_BIT);
        }
        if (mesh.convex) {
            if (flatDirect) renderBoundary(mesh, programs, vbo, width, height);
            renderDirect(mesh, programs, vbo, ebo, width, height);
        } else {
            renderStencil(mesh, programs, vbo, ebo, width, height);
            if (flatDirect) renderBoundary(mesh, programs, vbo, width, height);
            renderCover(mesh, programs, vbo, flatDirect, width, height);
        }
    }

    auto filename = outputDir + "/" + name + ".png";
    auto result = aa_poc::writeFramebufferPng(
        filename, renderTarget.framebuffer(), width, height, 1, "aa_flat_direct_poc");
    if (result) std::printf("wrote %s\n", filename.c_str());
    return result;
}

} // namespace

int main(int argc, char** argv)
{
    auto comparison = aa_poc::takeComparisonOption(argc, argv);
    aa_poc::RunOptions options;
    options.outputDir = "aa_flat_direct_poc-output";
    if (!aa_poc::parseOptions(argc, argv, options)) {
        aa_poc::printUsage(argv[0], "[--comparison]");
        return EXIT_FAILURE;
    }
    if (!aa_poc::makeOutputDirectory(options.outputDir)) {
        std::fprintf(stderr, "aa_flat_direct_poc: cannot create output directory: %s\n", options.outputDir.c_str());
        return EXIT_FAILURE;
    }

    aa_poc::GlContext context;
    if (!context.init()) {
        std::fprintf(stderr, "aa_flat_direct_poc: failed to create an offscreen GL context\n");
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
        result = renderMode("flat-direct", true, scene, programs, vao, vbo, ebo,
                            width, height, outputDir) && result;
        return result;
    };

    auto success = aa_poc::renderOffsets(options, renderAt);

    glDeleteBuffers(1, &ebo);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    destroyPrograms(programs);

    if (success) {
        std::printf("subpixel offset: %.3f, %.3f\n", static_cast<double>(options.offsetX), static_cast<double>(options.offsetY));
        return EXIT_SUCCESS;
    }
    return EXIT_FAILURE;
}
