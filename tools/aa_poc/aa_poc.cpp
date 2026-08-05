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
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION 1
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl3.h>
#else
#include <EGL/egl.h>
#include <EGL/eglext.h>
#if defined(AA_POC_GLES)
#include <GLES3/gl3.h>
#else
#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
#include <GL/glext.h>
#endif
#endif

#include "lodepng.h"
#include "tvgMath.h"
#include "tvgRender.h"

using namespace tvg;

namespace
{

constexpr uint32_t WIDTH = 640;
constexpr uint32_t HEIGHT = 360;
constexpr uint32_t SSAA_SCALE = 8;
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

struct Target
{
    GLuint fbo = 0;
    GLuint color = 0;
    GLuint depthStencil = 0;
};

struct Programs
{
    GLuint solid = 0;
    GLuint boundary = 0;
};

struct Options
{
    std::string outputDir = "aa_poc-output";
    float offsetX = 0.0f;
    float offsetY = 0.0f;
    bool offsetGrid = false;
    bool motion = false;
};

#ifdef __APPLE__

struct GlContext
{
    CGLContextObj context = nullptr;

    bool init()
    {
        CGLPixelFormatAttribute attrs[] = {
            kCGLPFAOpenGLProfile, static_cast<CGLPixelFormatAttribute>(kCGLOGLPVersion_GL3_Core),
            kCGLPFAColorSize, static_cast<CGLPixelFormatAttribute>(32),
            kCGLPFADepthSize, static_cast<CGLPixelFormatAttribute>(24),
            kCGLPFAStencilSize, static_cast<CGLPixelFormatAttribute>(8),
            static_cast<CGLPixelFormatAttribute>(0)
        };

        CGLPixelFormatObj pixelFormat = nullptr;
        GLint count = 0;
        if (CGLChoosePixelFormat(attrs, &pixelFormat, &count) != kCGLNoError || !pixelFormat) return false;
        auto result = CGLCreateContext(pixelFormat, nullptr, &context);
        CGLDestroyPixelFormat(pixelFormat);
        if (result != kCGLNoError || !context) return false;
        return CGLSetCurrentContext(context) == kCGLNoError;
    }

    ~GlContext()
    {
        if (!context) return;
        CGLSetCurrentContext(nullptr);
        CGLDestroyContext(context);
    }
};

#else

struct GlContext
{
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLContext context = EGL_NO_CONTEXT;

    bool initDisplay()
    {
        display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (display != EGL_NO_DISPLAY && eglInitialize(display, nullptr, nullptr) == EGL_TRUE) return true;

        display = EGL_NO_DISPLAY;
        auto queryDevices = reinterpret_cast<PFNEGLQUERYDEVICESEXTPROC>(eglGetProcAddress("eglQueryDevicesEXT"));
        auto getPlatformDisplay = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplayEXT"));
        if (!getPlatformDisplay) {
            getPlatformDisplay = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplay"));
        }
        if (!queryDevices || !getPlatformDisplay) return false;

        EGLDeviceEXT device = nullptr;
        EGLint count = 0;
        if (queryDevices(1, &device, &count) != EGL_TRUE || count <= 0) return false;
        display = getPlatformDisplay(EGL_PLATFORM_DEVICE_EXT, device, nullptr);
        return display != EGL_NO_DISPLAY && eglInitialize(display, nullptr, nullptr) == EGL_TRUE;
    }

    bool init()
    {
        if (!initDisplay()) return false;

#if defined(AA_POC_GLES)
        if (eglBindAPI(EGL_OPENGL_ES_API) != EGL_TRUE) return false;
        constexpr EGLint renderable = EGL_OPENGL_ES3_BIT;
#else
        if (eglBindAPI(EGL_OPENGL_API) != EGL_TRUE) return false;
        constexpr EGLint renderable = EGL_OPENGL_BIT;
#endif

        const EGLint configAttrs[] = {
            EGL_RENDERABLE_TYPE, renderable,
            EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
            EGL_RED_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE, 8,
            EGL_ALPHA_SIZE, 8,
            EGL_DEPTH_SIZE, 24,
            EGL_STENCIL_SIZE, 8,
            EGL_NONE
        };
        EGLConfig config = nullptr;
        EGLint count = 0;
        if (eglChooseConfig(display, configAttrs, &config, 1, &count) != EGL_TRUE || count <= 0) return false;

#if defined(AA_POC_GLES)
        const EGLint contextAttrs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
#else
        const EGLint contextAttrs[] = {
            EGL_CONTEXT_MAJOR_VERSION, 3,
            EGL_CONTEXT_MINOR_VERSION, 3,
            EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
            EGL_NONE
        };
#endif
        context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttrs);
        if (context == EGL_NO_CONTEXT) return false;

        const EGLint surfaceAttrs[] = {EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE};
        surface = eglCreatePbufferSurface(display, config, surfaceAttrs);
        if (surface == EGL_NO_SURFACE) return false;
        return eglMakeCurrent(display, surface, surface, context) == EGL_TRUE;
    }

    ~GlContext()
    {
        if (display == EGL_NO_DISPLAY) return;
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (surface != EGL_NO_SURFACE) eglDestroySurface(display, surface);
        if (context != EGL_NO_CONTEXT) eglDestroyContext(display, context);
        eglTerminate(display);
        eglReleaseThread();
    }
};

#endif

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

GLuint compileShader(GLenum type, const char* source)
{
    auto shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_TRUE) return shader;

    char log[4096] = {};
    glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
    std::fprintf(stderr, "aa_poc: shader compilation failed:\n%s\n", log);
    glDeleteShader(shader);
    return 0;
}

GLuint createProgram(const char* vertexSource, const char* fragmentSource, bool boundary)
{
    auto vertex = compileShader(GL_VERTEX_SHADER, vertexSource);
    auto fragment = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
    if (!vertex || !fragment) {
        if (vertex) glDeleteShader(vertex);
        if (fragment) glDeleteShader(fragment);
        return 0;
    }

    auto program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glBindAttribLocation(program, 0, "aPosition");
    if (boundary) {
        glBindAttribLocation(program, 1, "aStart");
        glBindAttribLocation(program, 2, "aEnd");
    }
    glLinkProgram(program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);

    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked == GL_TRUE) return program;

    char log[4096] = {};
    glGetProgramInfoLog(program, sizeof(log), nullptr, log);
    std::fprintf(stderr, "aa_poc: shader link failed:\n%s\n", log);
    glDeleteProgram(program);
    return 0;
}

bool createPrograms(Programs& programs)
{
    programs.solid = createProgram(SOLID_VERTEX_SHADER, SOLID_FRAGMENT_SHADER, false);
    programs.boundary = createProgram(BOUNDARY_VERTEX_SHADER, BOUNDARY_FRAGMENT_SHADER, true);
    return programs.solid && programs.boundary;
}

void destroyPrograms(Programs& programs)
{
    if (programs.solid) glDeleteProgram(programs.solid);
    if (programs.boundary) glDeleteProgram(programs.boundary);
    programs = {};
}

bool createTarget(Target& target, uint32_t width, uint32_t height, uint32_t samples)
{
    glGenFramebuffers(1, &target.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, target.fbo);

    glGenRenderbuffers(1, &target.color);
    glBindRenderbuffer(GL_RENDERBUFFER, target.color);
    if (samples > 1) glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_RGBA8, width, height);
    else glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, target.color);

    glGenRenderbuffers(1, &target.depthStencil);
    glBindRenderbuffer(GL_RENDERBUFFER, target.depthStencil);
    if (samples > 1) glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_DEPTH24_STENCIL8, width, height);
    else glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, target.depthStencil);

    auto status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status == GL_FRAMEBUFFER_COMPLETE) return true;
    std::fprintf(stderr, "aa_poc: framebuffer incomplete (0x%x)\n", status);
    return false;
}

void destroyTarget(Target& target)
{
    if (target.fbo) glDeleteFramebuffers(1, &target.fbo);
    if (target.color) glDeleteRenderbuffers(1, &target.color);
    if (target.depthStencil) glDeleteRenderbuffers(1, &target.depthStencil);
    target = {};
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

bool writePng(const std::string& filename, GLuint framebuffer, uint32_t width, uint32_t height, uint32_t downsample)
{
    const auto outputWidth = width / downsample;
    const auto outputHeight = height / downsample;
    std::vector<unsigned char> pixels(width * height * 4);
    std::vector<unsigned char> output(outputWidth * outputHeight * 4);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    for (uint32_t y = 0; y < outputHeight; ++y) {
        for (uint32_t x = 0; x < outputWidth; ++x) {
            uint32_t sum[4] = {};
            for (uint32_t sy = 0; sy < downsample; ++sy) {
                auto srcY = height - 1 - (y * downsample + sy);
                for (uint32_t sx = 0; sx < downsample; ++sx) {
                    auto src = pixels.data() + (srcY * width + x * downsample + sx) * 4;
                    for (uint32_t channel = 0; channel < 4; ++channel) sum[channel] += src[channel];
                }
            }
            auto dst = output.data() + (y * outputWidth + x) * 4;
            auto sampleCount = downsample * downsample;
            for (uint32_t channel = 0; channel < 4; ++channel) dst[channel] = static_cast<unsigned char>((sum[channel] + sampleCount / 2) / sampleCount);
        }
    }

    auto error = lodepng::encode(filename, output, outputWidth, outputHeight);
    if (!error) return true;
    std::fprintf(stderr, "aa_poc: PNG encode failed for %s: %s\n", filename.c_str(), lodepng_error_text(error));
    return false;
}

bool renderMode(const char* name, uint32_t samples, bool flatDirect, const std::vector<Mesh>& scene,
                const Programs& programs, GLuint vao, GLuint vbo, GLuint ebo, const std::string& outputDir,
                uint32_t width = WIDTH, uint32_t height = HEIGHT, uint32_t downsample = 1)
{
    Target renderTarget;
    Target resolveTarget;
    if (!createTarget(renderTarget, width, height, samples)) return false;
    if (samples > 1 && !createTarget(resolveTarget, width, height, 1)) {
        destroyTarget(renderTarget);
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, renderTarget.fbo);
    glBindVertexArray(vao);
    glViewport(0, 0, width, height);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_SCISSOR_TEST);
    glScissor(0, 0, width, height);
    glFrontFace(GL_CCW);
#if !defined(AA_POC_GLES)
    glEnable(GL_MULTISAMPLE);
#endif
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

    GLuint readTarget = renderTarget.fbo;
    if (samples > 1) {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, renderTarget.fbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolveTarget.fbo);
        glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        readTarget = resolveTarget.fbo;
    }

    auto filename = outputDir + "/" + name + ".png";
    auto result = writePng(filename, readTarget, width, height, downsample);
    if (result) std::printf("wrote %s\n", filename.c_str());
    destroyTarget(resolveTarget);
    destroyTarget(renderTarget);
    return result;
}

bool makeOutputDirectory(const std::string& path)
{
#ifdef _WIN32
    auto result = _mkdir(path.c_str());
#else
    auto result = mkdir(path.c_str(), 0755);
#endif
    return result == 0 || errno == EEXIST;
}

bool parseFloat(const char* text, float& value)
{
    char* end = nullptr;
    errno = 0;
    auto parsed = std::strtof(text, &end);
    if (errno != 0 || end == text || *end != '\0' || !std::isfinite(parsed)) return false;
    value = parsed;
    return true;
}

void printUsage(const char* executable)
{
    std::printf("usage: %s [--output-dir DIR] [--offset-x PX] [--offset-y PX] [--offset-grid | --motion]\n", executable);
}

bool parseOptions(int argc, char** argv, Options& options)
{
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--output-dir") == 0 && i + 1 < argc) {
            options.outputDir = argv[++i];
        } else if (std::strcmp(argv[i], "--offset-x") == 0 && i + 1 < argc) {
            if (!parseFloat(argv[++i], options.offsetX)) return false;
        } else if (std::strcmp(argv[i], "--offset-y") == 0 && i + 1 < argc) {
            if (!parseFloat(argv[++i], options.offsetY)) return false;
        } else if (std::strcmp(argv[i], "--offset-grid") == 0) {
            options.offsetGrid = true;
        } else if (std::strcmp(argv[i], "--motion") == 0) {
            options.motion = true;
        } else {
            return false;
        }
    }
    return !options.outputDir.empty() && !(options.offsetGrid && options.motion);
}

} // namespace

int main(int argc, char** argv)
{
    Options options;
    if (!parseOptions(argc, argv, options)) {
        printUsage(argv[0]);
        return EXIT_FAILURE;
    }
    if (!makeOutputDirectory(options.outputDir)) {
        std::fprintf(stderr, "aa_poc: cannot create output directory: %s\n", options.outputDir.c_str());
        return EXIT_FAILURE;
    }

    GlContext context;
    if (!context.init()) {
        std::fprintf(stderr, "aa_poc: failed to create an offscreen GL context\n");
        return EXIT_FAILURE;
    }

    auto version = glGetString(GL_VERSION);
    std::printf("GL: %s\n", version ? reinterpret_cast<const char*>(version) : "unknown");
#if defined(AA_POC_GLES)
    std::printf("ThorVG compatibility target: OpenGL ES 3.0 / GLSL ES 3.00\n");
#else
    std::printf("ThorVG compatibility target: OpenGL 3.3 / GLSL 3.30\n");
#endif

    GLint maxSamples = 0;
    glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
    if (maxSamples < 4) {
        std::fprintf(stderr, "aa_poc: GL implementation supports only %d samples; msaa4 requires 4\n", maxSamples);
        return EXIT_FAILURE;
    }

    Programs programs;
    if (!createPrograms(programs)) return EXIT_FAILURE;

    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);
    auto renderAt = [&](float offsetX, float offsetY, const std::string& outputDir) {
        if (!makeOutputDirectory(outputDir)) return false;
        auto scene = makeScene(offsetX, offsetY);
        auto result = renderMode("noaa", 1, false, scene, programs, vao, vbo, ebo, outputDir);
        result = renderMode("msaa4", 4, false, scene, programs, vao, vbo, ebo, outputDir) && result;
        result = renderMode("flat-direct", 1, true, scene, programs, vao, vbo, ebo, outputDir) && result;
        // Rebuild and re-flatten at the reference resolution. ThorVG's cubic
        // subdivision count is device-scale dependent, so scaling a finished
        // mesh would not be a valid SSAA reference.
        auto ssaaScene = makeScene(offsetX, offsetY, static_cast<float>(SSAA_SCALE));
        result = renderMode("ssaa8", 1, false, ssaaScene, programs, vao, vbo, ebo, outputDir,
                            WIDTH * SSAA_SCALE, HEIGHT * SSAA_SCALE, SSAA_SCALE) && result;
        return result;
    };

    auto success = true;
    if (options.offsetGrid) {
        for (uint32_t y = 0; y < 8; ++y) {
            for (uint32_t x = 0; x < 8; ++x) {
                char name[32];
                std::snprintf(name, sizeof(name), "offset-%u-%u", x, y);
                auto outputDir = options.outputDir + "/" + name;
                success = renderAt(options.offsetX + x / 8.0f, options.offsetY + y / 8.0f, outputDir) && success;
            }
        }
    } else if (options.motion) {
        for (uint32_t frame = 0; frame <= 64; ++frame) {
            char name[32];
            std::snprintf(name, sizeof(name), "frame-%03u", frame);
            auto outputDir = options.outputDir + "/" + name;
            success = renderAt(options.offsetX + frame / 64.0f, options.offsetY, outputDir) && success;
        }
    } else {
        success = renderAt(options.offsetX, options.offsetY, options.outputDir);
    }

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
