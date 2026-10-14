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
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include "tvgGlRenderer.h"

namespace
{

constexpr uint32_t Width = 256, Height = 192;
const std::array<const char*, 8> Names = {{"notch", "ellipse", "concave", "ring", "cubic", "star", "clipped", "tiny"}};

template<class T> T procedure(const char* name)
{
    auto address = wglGetProcAddress(name);
    if (!address || address == reinterpret_cast<PROC>(1) || address == reinterpret_cast<PROC>(2) ||
        address == reinterpret_cast<PROC>(3) || address == reinterpret_cast<PROC>(-1))
        address = GetProcAddress(GetModuleHandleW(L"opengl32.dll"), name);
    return reinterpret_cast<T>(address);
}

struct WglTarget
{
    HWND window = nullptr;
    HDC dc = nullptr;
    HGLRC context = nullptr;
    ATOM windowClass = 0;
    wchar_t className[64] = {};
    void (APIENTRY *getTexImage)(GLenum, GLint, GLenum, GLenum, void*) = nullptr;
    void (APIENTRY *getTexLevelParameteriv)(GLenum, GLint, GLenum, GLint*) = nullptr;
    void (APIENTRY *objectLabel)(GLenum, GLuint, GLsizei, const char*) = nullptr;

    bool init()
    {
        auto instance = GetModuleHandleW(nullptr);
        _snwprintf_s(className, _countof(className), _TRUNCATE, L"ThorVG.CoverageStorage.%p", static_cast<void*>(this));
        WNDCLASSW wc = {};
        wc.style = CS_OWNDC;
        wc.lpfnWndProc = DefWindowProcW;
        wc.hInstance = instance;
        wc.lpszClassName = className;
        windowClass = RegisterClassW(&wc);
        if (!windowClass) return false;
        window = CreateWindowExW(0, className, L"", WS_POPUP, 0, 0, 1, 1, nullptr, nullptr, instance, nullptr);
        if (!window || !(dc = GetDC(window))) return false;
        PIXELFORMATDESCRIPTOR pfd = {};
        pfd.nSize = sizeof(pfd);
        pfd.nVersion = 1;
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.cColorBits = 32;
        pfd.cAlphaBits = 8;
        pfd.cDepthBits = 24;
        pfd.cStencilBits = 8;
        auto format = ChoosePixelFormat(dc, &pfd);
        if (!format || !SetPixelFormat(dc, format, &pfd)) return false;
        context = wglCreateContext(dc);
        if (!context || !wglMakeCurrent(dc, context)) return false;
        using CreateContext = HGLRC (WINAPI *)(HDC, HGLRC, const int*);
        auto createContext = procedure<CreateContext>("wglCreateContextAttribsARB");
        if (!createContext) return false;
        const int attributes[] = {0x2091, 3, 0x2092, 3, 0x9126, 1, 0};
        auto core = createContext(dc, nullptr, attributes);
        if (!core) return false;
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(context);
        context = core;
        if (!wglMakeCurrent(dc, context)) return false;
        getTexImage = procedure<decltype(getTexImage)>("glGetTexImage");
        getTexLevelParameteriv = procedure<decltype(getTexLevelParameteriv)>("glGetTexLevelParameteriv");
        objectLabel = procedure<decltype(objectLabel)>("glObjectLabel");
        return getTexImage && getTexLevelParameteriv;
    }

    ~WglTarget()
    {
        if (context) {
            wglMakeCurrent(nullptr, nullptr);
            wglDeleteContext(context);
        }
        if (dc) ReleaseDC(window, dc);
        if (window) DestroyWindow(window);
        if (windowClass) UnregisterClassW(className, GetModuleHandleW(nullptr));
    }
};

void shapes(std::array<RenderShape, 8>& out)
{
    auto& notch = out[0].path;
    notch.moveTo({18.25f, 16.5f}); notch.lineTo({218.75f, 16.5f}); notch.lineTo({218.75f, 173.25f});
    notch.lineTo({18.25f, 173.25f}); notch.lineTo({18.25f, 126.5f});
    notch.cubicTo({135.5f, 124.5f}, {142.25f, 65.75f}, {18.25f, 59.5f}); notch.close();
    out[1].path.addCircle(125.5f, 94.25f, 97.75f, 71.5f, true);
    auto& concave = out[2].path;
    concave.moveTo({31.25f, 28.5f}); concave.lineTo({208.75f, 28.5f}); concave.lineTo({208.75f, 58.25f});
    concave.lineTo({91.5f, 58.25f}); concave.lineTo({91.5f, 157.75f}); concave.lineTo({31.25f, 157.75f}); concave.close();
    out[3].path.addRect(24.25f, 18.5f, 203.5f, 153.25f, 19.5f, 19.5f, true);
    out[3].path.addRect(77.75f, 65.25f, 94.5f, 63.5f, 11.25f, 11.25f, true);
    out[3].rule = FillRule::EvenOdd;
    out[4].path.moveTo({20.5f, 94.25f});
    out[4].path.cubicTo({74.75f, -48.5f}, {178.25f, 244.25f}, {233.5f, 82.75f});
    auto& star = out[5].path;
    star.moveTo({126.25f, 13.5f}); star.lineTo({174.75f, 171.25f}); star.lineTo({16.5f, 70.75f});
    star.lineTo({235.5f, 70.75f}); star.lineTo({76.75f, 171.25f}); star.close();
    out[6].path.addRect(-23.75f, -12.5f, 144.5f, 123.25f, 13.75f, 13.75f, true);
    out[7].path.moveTo({121.2f, 91.35f}); out[7].path.lineTo({122.4f, 91.55f});
    out[7].path.lineTo({121.6f, 92.45f}); out[7].path.close();
    for (uint32_t i = 0; i < out.size(); ++i) {
        out[i].color = {255, 255, 255, 128};
        out[i].stroke = new RenderStroke;
        out[i].stroke->width = i == 7 ? 0.55f : 7.25f + i;
        out[i].stroke->color = {32, 64, 192, 128};
        out[i].stroke->cap = i % 2 ? StrokeCap::Round : StrokeCap::Square;
        out[i].stroke->join = i % 3 == 0 ? StrokeJoin::Miter : (i % 3 == 1 ? StrokeJoin::Round : StrokeJoin::Bevel);
    }
}

struct Validation
{
    uint32_t checks = 0, failures = 0, maps = 0;
    size_t denseBytes = 0, gpuBytes = 0;
    std::string directory;
    std::ofstream csv, svg;
    std::set<GLuint> textures;
    std::set<const void*> buffers;

    bool check(bool value, const std::string& message)
    {
        ++checks;
        if (!value) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
        return value;
    }

    void edgePatch(const CoverageMap& map, const std::string& label)
    {
        if (!svg) return;
        uint32_t first = 0;
        for (; first < map.stride * map.bounds.h(); ++first) if (map.data[first] > 0 && map.data[first] < 255) break;
        auto x = int32_t(first % map.stride), y = int32_t(first / map.stride);
        x = std::max(0, std::min(x - 3, int32_t(map.stride) - 8));
        y = std::max(0, std::min(y - 3, int32_t(map.bounds.h()) - 8));
        svg << "<g transform=\"translate(" << (maps % 4) * 170 << ',' << (maps / 4) * 160 << ")\">\n"
            << "<text x=\"4\" y=\"16\">" << label << "</text>\n<text x=\"4\" y=\"32\">pixel "
            << map.bounds.min.x + x << ',' << map.bounds.min.y + y << "</text>\n";
        for (uint32_t dy = 0; dy < std::min(8u, map.bounds.h()); ++dy) {
            for (uint32_t dx = 0; dx < std::min(8u, map.stride); ++dx) {
                auto coverage = unsigned(map.data[size_t(y + dy) * map.stride + x + dx]);
                auto gray = 255 - coverage;
                svg << "<rect x=\"" << 4 + dx * 19 << "\" y=\"" << 40 + dy * 14 << "\" width=\"19\" height=\"14\" fill=\"rgb("
                    << gray << ',' << gray << ',' << gray << ")\"/><text x=\"" << 5 + dx * 19 << "\" y=\"" << 50 + dy * 14
                    << "\" font-size=\"8\" fill=\"" << (coverage > 128 ? "white" : "black") << "\">" << coverage << "</text>\n";
            }
        }
        svg << "</g>\n";
    }

    bool read(GlRenderer& renderer, const WglTarget& target, const GlCoverageMap& coverage, const std::string& label, bool record = false)
    {
        const auto& map = coverage.map;
        if (!check(coverage.status == CoverageStatus::Ready && map.data && coverage.texture, label + " owns ready CPU/GPU map")) return false;
        renderer.mStateCache.bindTexture2D(TVG_GL_TEXTURE_SETUP_UNIT, coverage.texture);
        GLint width = 0, height = 0, format = 0, redBits = 0;
        target.getTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
        target.getTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);
        target.getTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &format);
        target.getTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_RED_SIZE, &redBits);
        auto runs = coverage.format == GlCoverageFormat::Runs;
        if (!check(width == int32_t(coverage.width) && height == int32_t(coverage.height) &&
                   format == (runs ? GL_RGBA16UI : GL_R8) && redBits == (runs ? 16 : 8), label + " allocated dimensions and format")) return false;
        auto dense = size_t(map.stride) * map.bounds.h();
        if (!check(map.stride == map.bounds.w() && coverage.byteSize == size_t(width) * height * (runs ? 8 : 1) &&
                   (runs ? coverage.byteSize < dense : coverage.byteSize == dense), label + " allocation includes padding and never expands dense storage")) return false;
        std::vector<uint8_t> restored(dense, 0);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        if (runs) {
            std::vector<uint16_t> records(coverage.byteSize / sizeof(uint16_t));
            target.getTexImage(GL_TEXTURE_2D, 0, GL_RGBA_INTEGER, GL_UNSIGNED_SHORT, records.data());
            if (!check(coverage.runCount <= records.size() / 4, label + " run count fits texture")) return false;
            for (uint32_t i = 0; i < coverage.runCount; ++i) {
                auto span = records.data() + i * 4;
                if (!check(span[2] > 0 && uint32_t(span[0]) + span[2] <= map.stride && span[1] < map.bounds.h() &&
                           span[3] > 0 && span[3] <= 255, label + " valid exact run")) return false;
                memset(restored.data() + size_t(span[1]) * map.stride + span[0], span[3], span[2]);
            }
            check(std::all_of(records.begin() + coverage.runCount * 4, records.end(), [](uint16_t value) { return value == 0; }), label + " trailing records are zero");
        } else {
            check(width == int32_t(map.stride) && height == int32_t(map.bounds.h()), label + " dense texture matches cropped bounds");
            target.getTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_UNSIGNED_BYTE, restored.data());
        }
        auto exact = check(glGetError() == GL_NO_ERROR && memcmp(restored.data(), map.data, dense) == 0, label + " GPU bytes exactly reconstruct CPU coverage");
        if (!record) return exact;
        check(textures.insert(coverage.texture).second && buffers.insert(map.data).second, label + " has independent storage");
        uint32_t nonzero = 0, partial = 0;
        for (auto value : restored) { nonzero += value != 0; partial += value > 0 && value < 255; }
        bool top = false, bottom = false, left = false, right = false;
        for (uint32_t x = 0; x < map.stride; ++x) { top |= map.data[x] != 0; bottom |= map.data[dense - map.stride + x] != 0; }
        for (uint32_t y = 0; y < map.bounds.h(); ++y) { left |= map.data[size_t(y) * map.stride] != 0; right |= map.data[size_t(y + 1) * map.stride - 1] != 0; }
        check(top && bottom && left && right, label + " tightly cropped coverage");
        check(partial > 0, label + " retains antialiased boundary pixels");
        std::ostringstream row;
        row << label << ',' << coverage.texture << ',' << map.bounds.min.x << ',' << map.bounds.min.y << ',' << map.bounds.max.x << ',' << map.bounds.max.y
            << ',' << nonzero << ',' << partial << ',' << dense << ',' << (runs ? "RGBA16UI-runs" : "R8") << ',' << width << ',' << height
            << ',' << coverage.runCount << ',' << coverage.byteSize << ',' << std::fixed << std::setprecision(3)
            << (100.0 * (dense - coverage.byteSize) / dense) << ',' << (exact ? 1 : 0) << '\n';
        std::cout << row.str();
        if (csv) csv << row.str();
        if (!directory.empty()) {
            std::ofstream pgm(directory + '/' + label + ".pgm", std::ios::binary);
            pgm << "P5\n" << map.stride << ' ' << map.bounds.h() << "\n255\n";
            pgm.write(reinterpret_cast<const char*>(restored.data()), restored.size());
            check(bool(pgm), label + " PGM saved");
        }
        edgePatch(map, label);
        ++maps; denseBytes += dense; gpuBytes += coverage.byteSize;
        return exact;
    }
};

} // namespace

int main(int argc, char** argv)
{
    Validation validation;
    bool capture = false, updatesOnly = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--capture") capture = true;
        else if (std::string(argv[i]) == "--updates-only") updatesOnly = true;
        else if (validation.directory.empty()) validation.directory = argv[i];
        else { std::cerr << "Usage: tvgCoverageStorageTests [output-directory] [--capture|--updates-only]\n"; return 2; }
    }
    if (!validation.directory.empty()) {
        if (!CreateDirectoryA(validation.directory.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) {
            std::cerr << "Cannot create output directory\n"; return 2;
        }
        validation.csv.open(validation.directory + "/storage.csv");
        validation.svg.open(validation.directory + "/edge-pixels.svg");
        if (!validation.csv || !validation.svg) { std::cerr << "Cannot write output files\n"; return 2; }
        validation.svg << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"680\" height=\"650\" viewBox=\"0 0 680 650\">\n"
                       << "<rect width=\"680\" height=\"650\" fill=\"white\"/><g font-family=\"monospace\" font-size=\"11\">\n";
    }
    WglTarget target;
    if (!validation.check(target.init(), "create WGL 3.3 core context")) return 1;
    if (!validation.check(Initializer::init(0) == Result::Success, "initialize")) return 1;
    std::unique_ptr<GlRenderer> renderer(GlRenderer::gen(0, EngineOption::None));
    if (!validation.check(bool(renderer), "production GL renderer")) { Initializer::term(); return 1; }
    if (!validation.check(renderer->target(nullptr, target.dc, target.context, 0, Width, Height, ColorSpace::ABGR8888S) == Result::Success, "renderer target")) {
        renderer.reset(); Initializer::term(); return 1;
    }
    renderer->viewport({{0, 0}, {Width, Height}});
    std::cerr << "GL renderer: " << glGetString(GL_RENDERER) << "; version: " << glGetString(GL_VERSION) << "; texture labels: " << bool(target.objectLabel) << '\n';
    std::array<RenderShape, 8> rshapes;
    std::array<GlShape*, 8> prepared = {};
    shapes(rshapes);
    Array<RenderData> clips;
    auto transform = identity();
    auto prepareAll = [&]() {
        validation.check(renderer->preUpdate(), "preUpdate");
        for (uint32_t i = 0; i < prepared.size(); ++i) {
            prepared[i] = static_cast<GlShape*>(renderer->prepare(rshapes[i], prepared[i], transform, clips, 255, RenderUpdateFlag::All, false));
            if (!validation.check(prepared[i] != nullptr, "shape prepare")) continue;
            if (target.objectLabel) {
                auto fill = "shape" + std::to_string(i) + "#fill", stroke = "shape" + std::to_string(i) + "#stroke";
                if (prepared[i]->fillCoverage.texture) target.objectLabel(GL_TEXTURE, prepared[i]->fillCoverage.texture, -1, fill.c_str());
                if (prepared[i]->strokeCoverage.texture) target.objectLabel(GL_TEXTURE, prepared[i]->strokeCoverage.texture, -1, stroke.c_str());
            }
        }
        validation.check(renderer->postUpdate(), "postUpdate");
    };
    prepareAll();
    const char* header = "map,texture,min_x,min_y,max_x,max_y,nonzero_pixels,partial_pixels,cropped_dense_bytes,gpu_format,gpu_width,gpu_height,run_count,gpu_bytes,reduction_percent,exact\n";
    std::cout << header;
    if (validation.csv) validation.csv << header;
    for (uint32_t i = 0; !updatesOnly && i < prepared.size(); ++i) {
        if (!prepared[i]) continue;
        auto label = std::string("shape") + std::to_string(i) + '-' + Names[i];
        validation.read(*renderer, target, prepared[i]->fillCoverage, label + "-fill", true);
        validation.read(*renderer, target, prepared[i]->strokeCoverage, label + "-stroke", true);
    }
    if (validation.svg) validation.svg << "</g></svg>\n";
    validation.csv.flush();
    validation.svg.flush();
    std::cout.flush();
    renderer->sync();
    if (capture) {
        SetWindowPos(target.window, nullptr, 0, 0, Width, Height, SWP_NOZORDER | SWP_NOACTIVATE);
        ShowWindow(target.window, SW_SHOWNOACTIVATE);
        for (uint32_t frame = 0; frame < 400; ++frame) {
            prepareAll();
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            validation.check(SwapBuffers(target.dc) != 0, "capture frame delimiter");
            Sleep(30);
        }
    }
    if (prepared[0]) {
        GLuint unpackBuffer = 0;
        glGenBuffers(1, &unpackBuffer); glBindBuffer(GL_PIXEL_UNPACK_BUFFER, unpackBuffer);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, 256, nullptr, GL_STATIC_DRAW);
        const GLenum parameters[] = {GL_UNPACK_ALIGNMENT, GL_UNPACK_ROW_LENGTH, GL_UNPACK_SKIP_PIXELS, GL_UNPACK_SKIP_ROWS, GL_UNPACK_SWAP_BYTES};
        const GLint values[] = {8, 17, 2, 3, GL_TRUE};
        for (uint32_t i = 0; i < 5; ++i) glPixelStorei(parameters[i], values[i]);
        renderer->prepare(rshapes[0], prepared[0], transform, clips, 255, RenderUpdateFlag::All, false);
        for (uint32_t i = 0; i < 5; ++i) { GLint actual; glGetIntegerv(parameters[i], &actual); validation.check(actual == values[i], "host unpack state restored"); }
        GLint actualBuffer; glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &actualBuffer);
        validation.check(actualBuffer == int32_t(unpackBuffer), "host unpack buffer restored");
        validation.read(*renderer, target, prepared[0]->fillCoverage, "host-unpack-fill");
        validation.read(*renderer, target, prepared[0]->strokeCoverage, "host-unpack-stroke");
        for (uint32_t i = 0; i < 5; ++i) glPixelStorei(parameters[i], i == 0 ? 4 : 0);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0); glDeleteBuffers(1, &unpackBuffer);
        renderer->sync();
        auto previous = prepared[0]->fillCoverage.map.bounds;
        transform.e13 = 9; transform.e23 = -3;
        renderer->prepare(rshapes[0], prepared[0], transform, clips, 255, RenderUpdateFlag::Transform, false);
        validation.check(!(prepared[0]->fillCoverage.map.bounds == previous), "geometry update changes map bounds");
        validation.read(*renderer, target, prepared[0]->fillCoverage, "updated-fill");
        validation.read(*renderer, target, prepared[0]->strokeCoverage, "updated-stroke");
        delete rshapes[0].stroke; rshapes[0].stroke = nullptr;
        renderer->prepare(rshapes[0], prepared[0], transform, clips, 255, RenderUpdateFlag::Stroke, false);
        validation.check(prepared[0]->strokeCoverage.status == CoverageStatus::Empty && !prepared[0]->strokeCoverage.map.data &&
                         !prepared[0]->strokeCoverage.texture && !prepared[0]->strokeCoverage.byteSize, "removing stroke clears owned CPU/GPU storage");
        validation.read(*renderer, target, prepared[0]->fillCoverage, "no-stroke-fill");
        rshapes[0].path.clear();
        renderer->prepare(rshapes[0], prepared[0], transform, clips, 255, RenderUpdateFlag::Path, false);
        validation.check(prepared[0]->fillCoverage.status == CoverageStatus::Empty && !prepared[0]->fillCoverage.map.data &&
                         !prepared[0]->fillCoverage.texture && !prepared[0]->fillCoverage.byteSize, "empty path clears owned CPU/GPU storage");
    }
    if (!updatesOnly) validation.check(validation.maps == 16 && validation.gpuBytes < validation.denseBytes, "eight shapes own sixteen exact maps with reduced GPU allocation");
    for (auto shape : prepared) if (shape) renderer->dispose(shape);
    renderer.reset();
    validation.check(Initializer::term() == Result::Success, "terminate");
    std::cerr << "coverage storage: maps=" << validation.maps << " cropped_dense_bytes=" << validation.denseBytes << " gpu_bytes=" << validation.gpuBytes
              << " checks=" << validation.checks << " failures=" << validation.failures << '\n';
    return validation.failures ? 1 : 0;
}
