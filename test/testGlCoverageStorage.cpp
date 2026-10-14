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
    GLboolean (APIENTRY *isTexture)(GLuint) = nullptr;

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
        isTexture = procedure<decltype(isTexture)>("glIsTexture");
        return getTexImage && getTexLevelParameteriv && isTexture;
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

struct GlCalls
{
    uint32_t created = 0, allocated = 0, uploaded = 0, bound = 0;
    PFNGLGENTEXTURESPROC generate = glGenTextures;
    PFNGLTEXIMAGE2DPROC image = glTexImage2D;
    PFNGLTEXSUBIMAGE2DPROC subImage = glTexSubImage2D;
    PFNGLBINDTEXTUREPROC bind = glBindTexture;
    static GlCalls* active;

    GlCalls()
    {
        active = this;
        glGenTextures = [](GLsizei count, GLuint* textures) { active->created += count; active->generate(count, textures); };
        glTexImage2D = [](GLenum target, GLint level, GLint format, GLsizei width, GLsizei height, GLint border, GLenum sourceFormat, GLenum type, const void* data) {
            ++active->allocated; active->image(target, level, format, width, height, border, sourceFormat, type, data);
        };
        glTexSubImage2D = [](GLenum target, GLint level, GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, const void* data) {
            ++active->uploaded; active->subImage(target, level, x, y, width, height, format, type, data);
        };
        glBindTexture = [](GLenum target, GLuint texture) { ++active->bound; active->bind(target, texture); };
    }

    ~GlCalls()
    {
        glGenTextures = generate; glTexImage2D = image; glTexSubImage2D = subImage; glBindTexture = bind;
        active = nullptr;
    }
};

GlCalls* GlCalls::active = nullptr;

bool sameRegion(const GlCoverageRegion& a, const GlCoverageRegion& b)
{
    return a.texture == b.texture && a.rect == b.rect;
}

bool overlaps(const GlCoverageRegion& a, const GlCoverageRegion& b)
{
    return a.texture == b.texture && a.rect.min.x < b.rect.max.x && b.rect.min.x < a.rect.max.x &&
           a.rect.min.y < b.rect.max.y && b.rect.min.y < a.rect.max.y;
}

struct Readback
{
    GLint width = 0, height = 0, format = 0, redBits = 0;
    std::vector<uint8_t> bytes;
};

Readback readRegion(GlStateCache& state, const WglTarget& target, const GlCoverageRegion& region, GlCoverageFormat format)
{
    Readback result;
    state.bindTexture2D(TVG_GL_TEXTURE_SETUP_UNIT, region.texture);
    target.getTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &result.width);
    target.getTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &result.height);
    target.getTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &result.format);
    target.getTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_RED_SIZE, &result.redBits);
    if (region.rect.min.x < 0 || region.rect.min.y < 0 || !region.rect.valid() ||
        region.rect.max.x > result.width || region.rect.max.y > result.height) return result;
    auto runs = format == GlCoverageFormat::Runs;
    auto bytes = runs ? 8u : 1u;
    std::vector<uint8_t> page(size_t(result.width) * result.height * bytes);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    target.getTexImage(GL_TEXTURE_2D, 0, runs ? GL_RGBA_INTEGER : GL_RED, runs ? GL_UNSIGNED_SHORT : GL_UNSIGNED_BYTE, page.data());
    result.bytes.resize(size_t(region.rect.w()) * region.rect.h() * bytes);
    for (uint32_t y = 0; y < region.rect.h(); ++y) {
        auto src = page.data() + (size_t(region.rect.min.y + y) * result.width + region.rect.min.x) * bytes;
        memcpy(result.bytes.data() + size_t(y) * region.rect.w() * bytes, src, size_t(region.rect.w()) * bytes);
    }
    return result;
}

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
    size_t denseBytes = 0, payloadBytes = 0, reservedBytes = 0;
    std::string directory;
    std::ofstream csv, svg;
    std::set<GLuint> textures;
    std::set<const void*> buffers;
    std::vector<GlCoverageRegion> regions;
    std::vector<GlCoverageFormat> formats;

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
        const auto& region = coverage.region;
        if (!check(coverage.status == CoverageStatus::Ready && map.data && region.texture, label + " owns ready CPU map and atlas region")) return false;
        auto readback = readRegion(renderer.mStateCache, target, region, coverage.format);
        auto runs = coverage.format == GlCoverageFormat::Runs;
        if (!check(readback.bytes.size() == coverage.byteSize && readback.format == (runs ? GL_RGBA16UI : GL_R8) &&
                   readback.redBits == (runs ? 16 : 8), label + " region fits page of the expected format")) return false;
        auto dense = size_t(map.stride) * map.bounds.h();
        if (!check(map.stride == map.bounds.w() && coverage.byteSize == size_t(region.rect.w()) * region.rect.h() * (runs ? 8 : 1) &&
                   (runs ? coverage.byteSize < dense : coverage.byteSize == dense), label + " payload includes record padding and never expands dense storage")) return false;
        std::vector<uint8_t> restored(dense, 0);
        if (runs) {
            std::vector<uint16_t> records(coverage.byteSize / sizeof(uint16_t));
            memcpy(records.data(), readback.bytes.data(), coverage.byteSize);
            if (!check(coverage.runCount <= records.size() / 4, label + " run count fits texture")) return false;
            for (uint32_t i = 0; i < coverage.runCount; ++i) {
                auto span = records.data() + i * 4;
                if (!check(span[2] > 0 && uint32_t(span[0]) + span[2] <= map.stride && span[1] < map.bounds.h() &&
                           span[3] > 0 && span[3] <= 255, label + " valid exact run")) return false;
                memset(restored.data() + size_t(span[1]) * map.stride + span[0], span[3], span[2]);
            }
            check(std::all_of(records.begin() + coverage.runCount * 4, records.end(), [](uint16_t value) { return value == 0; }), label + " trailing records are zero");
        } else {
            check(region.rect.w() == map.stride && region.rect.h() == map.bounds.h(), label + " dense region matches cropped bounds");
            restored = readback.bytes;
        }
        auto exact = check(glGetError() == GL_NO_ERROR && memcmp(restored.data(), map.data, dense) == 0, label + " GPU bytes exactly reconstruct CPU coverage");
        if (!record) return exact;
        check(buffers.insert(map.data).second, label + " has independent CPU storage");
        for (uint32_t i = 0; i < regions.size(); ++i) {
            check(!overlaps(region, regions[i]), label + " atlas region does not overlap another map");
            if (region.texture == regions[i].texture) check(coverage.format == formats[i], label + " shares only with the same format");
        }
        regions.push_back(region);
        formats.push_back(coverage.format);
        auto pageBytes = size_t(readback.width) * readback.height * (runs ? 8 : 1);
        if (textures.insert(region.texture).second) reservedBytes += pageBytes;
        uint32_t nonzero = 0, partial = 0;
        for (auto value : restored) { nonzero += value != 0; partial += value > 0 && value < 255; }
        bool top = false, bottom = false, left = false, right = false;
        for (uint32_t x = 0; x < map.stride; ++x) { top |= map.data[x] != 0; bottom |= map.data[dense - map.stride + x] != 0; }
        for (uint32_t y = 0; y < map.bounds.h(); ++y) { left |= map.data[size_t(y) * map.stride] != 0; right |= map.data[size_t(y + 1) * map.stride - 1] != 0; }
        check(top && bottom && left && right, label + " tightly cropped coverage");
        check(partial > 0, label + " retains antialiased boundary pixels");
        std::ostringstream row;
        row << label << ',' << region.texture << ',' << map.bounds.min.x << ',' << map.bounds.min.y << ',' << map.bounds.max.x << ',' << map.bounds.max.y
            << ',' << nonzero << ',' << partial << ',' << dense << ',' << (runs ? "RGBA16UI-runs" : "R8") << ',' << readback.width << ',' << readback.height << ',' << pageBytes
            << ',' << region.rect.min.x << ',' << region.rect.min.y << ',' << region.rect.w() << ',' << region.rect.h()
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
        ++maps; denseBytes += dense; payloadBytes += coverage.byteSize;
        return exact;
    }
};

void atlasChecks(Validation& validation, GlRenderer& renderer, const WglTarget& target)
{
    GlCoverageAtlas atlas;
    GlCoverageData packed;
    GlCoverageRegion first, neighbor, full, pending, reused, runNeighbor;
    std::array<uint8_t, 64> a, b;
    a.fill(31); b.fill(79);
    packed.width = packed.height = 8;
    packed.byteSize = a.size();
    auto update = [&](const void* data, GlCoverageRegion& region, const char* label) {
        packed.data = data;
        return validation.check(atlas.update(packed, region), label);
    };
    if (!update(a.data(), first, "allocate first atlas region") || !update(b.data(), neighbor, "allocate neighbor atlas region")) {
        atlas.clear(renderer.mStateCache);
        return;
    }
    validation.check(first.texture == neighbor.texture && !overlaps(first, neighbor), "small dense maps share one page without overlap");
    auto original = first;
    a.fill(0);
    atlas.flush(renderer.mStateCache);
    auto page = readRegion(renderer.mStateCache, target, first, packed.format);
    if (!validation.check(page.width > 0 && page.height > 0 && page.bytes.size() == a.size(), "atlas page readback dimensions")) {
        atlas.clear(renderer.mStateCache);
        return;
    }
    validation.check(std::all_of(page.bytes.begin(), page.bytes.end(), [](uint8_t value) { return value == 31; }), "atlas stages bytes independently of caller storage");
    a.fill(42);
    {
        GlCalls calls;
        update(a.data(), first, "update existing atlas region");
        atlas.flush(renderer.mStateCache);
        validation.check(calls.created == 0 && calls.allocated == 0 && calls.uploaded == 1 && calls.bound <= 1, "same size update uploads once without new texture storage");
    }
    validation.check(sameRegion(first, original), "same size and format preserve atlas region");
    validation.check(readRegion(renderer.mStateCache, target, first, packed.format).bytes == std::vector<uint8_t>(a.begin(), a.end()), "region update reaches GPU");
    validation.check(readRegion(renderer.mStateCache, target, neighbor, packed.format).bytes == std::vector<uint8_t>(b.begin(), b.end()), "region update preserves neighboring bytes");

    packed.width = page.width;
    packed.height = page.height;
    packed.byteSize = size_t(packed.width) * packed.height;
    std::vector<uint8_t> wholePage(packed.byteSize, 133);
    if (update(wholePage.data(), full, "allocate full page region")) {
        validation.check(full.texture != first.texture, "page rollover preserves existing regions");
        atlas.flush(renderer.mStateCache);
        validation.check(readRegion(renderer.mStateCache, target, full, packed.format).bytes == wholePage, "rollover page contains exact bytes");
        auto retired = full;
        atlas.release(full);
        validation.check(!full.texture && !full.rect.valid(), "release clears region handle");
        update(wholePage.data(), pending, "allocate before synchronized reclaim");
        atlas.flush(renderer.mStateCache);
        validation.check(!overlaps(retired, pending), "released region remains unavailable before sync");
        validation.check(readRegion(renderer.mStateCache, target, retired, packed.format).bytes == wholePage, "released region remains intact before sync");
        validation.check(renderer.sync(), "sync before atlas reclaim");
        atlas.reclaim();
        update(wholePage.data(), reused, "allocate after synchronized reclaim");
        validation.check(sameRegion(retired, reused), "released full page is reusable after sync");
    }

    const uint16_t record[] = {0, 0, 8, 94};
    packed.width = packed.height = packed.runCount = 1;
    packed.byteSize = sizeof(record);
    packed.format = GlCoverageFormat::Runs;
    update(record, first, "switch atlas region format");
    update(record, runNeighbor, "allocate same-format run neighbor");
    atlas.flush(renderer.mStateCache);
    auto runs = readRegion(renderer.mStateCache, target, first, packed.format);
    validation.check(first.texture != original.texture && runs.format == GL_RGBA16UI, "format switch uses a different page of the correct format");
    validation.check(runs.bytes.size() == sizeof(record) && memcmp(runs.bytes.data(), record, sizeof(record)) == 0, "format switch preserves exact run bytes");
    validation.check(first.texture == runNeighbor.texture && !overlaps(first, runNeighbor), "run maps share a page without overlap");
    validation.check(readRegion(renderer.mStateCache, target, neighbor, GlCoverageFormat::Dense).bytes == std::vector<uint8_t>(b.begin(), b.end()), "format switch preserves dense neighbor");
    std::set<GLuint> pages = {original.texture, pending.texture, reused.texture, first.texture};
    atlas.clear(renderer.mStateCache);
    for (auto texture : pages) if (texture) validation.check(target.isTexture(texture) == GL_FALSE, "atlas clear releases page texture");
    validation.check(glGetError() == GL_NO_ERROR, "atlas lifecycle has no GL error");
}

uint8_t coveragePixel(const CoverageMap& map, uint32_t x, uint32_t y)
{
    if (!map.data || int32_t(x) < map.bounds.min.x || int32_t(y) < map.bounds.min.y ||
        int32_t(x) >= map.bounds.max.x || int32_t(y) >= map.bounds.max.y) return 0;
    return map.data[size_t(y - map.bounds.min.y) * map.stride + x - map.bounds.min.x];
}

std::array<double, 4> paintColor(const RenderColor& color, const Fill* fill, const Matrix& transform,
                                 uint8_t opacity, uint32_t x, uint32_t y)
{
    if (!fill) return {{color.r / 255.0, color.g / 255.0, color.b / 255.0, MULTIPLY(color.a, opacity) / 255.0}};

    Matrix inverseShape, inverseFill;
    inverse(&transform, &inverseShape);
    inverse(&fill->transform(), &inverseFill);
    Point position = {x + 0.5f, y + 0.5f};
    position *= inverseFill * inverseShape;
    double t = 0.0;
    if (fill->type() == Type::LinearGradient) {
        float x1, y1, x2, y2;
        static_cast<const LinearGradient*>(fill)->linear(&x1, &y1, &x2, &y2);
        t = ((position.x - x1) * (x2 - x1) + (position.y - y1) * (y2 - y1)) /
            ((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
    } else if (fill->type() == Type::RadialGradient) {
        float cx, cy, radius;
        static_cast<const RadialGradient*>(fill)->radial(&cx, &cy, &radius);
        t = std::hypot(position.x - cx, position.y - cy) / radius;
    } else {
        float cx, cy, angle;
        static_cast<const ConicGradient*>(fill)->conic(&cx, &cy, &angle);
        t = (std::atan2(position.y - cy, position.x - cx) - angle * 3.141592653589793 / 180.0) /
            (2.0 * 3.141592653589793);
        t -= std::floor(t);
    }
    t = std::max(0.0, std::min(1.0, t));
    const Fill::ColorStop* stops;
    fill->colorStops(&stops);
    const auto& first = stops[0];
    const auto& last = stops[1];
    return {{(first.r * (1.0 - t) + last.r * t) / 255.0,
             (first.g * (1.0 - t) + last.g * t) / 255.0,
             (first.b * (1.0 - t) + last.b * t) / 255.0,
             (first.a * (1.0 - t) + last.a * t) * opacity / (255.0 * 255.0)}};
}

std::vector<uint8_t> drawCase(Validation& validation, GlRenderer& renderer, const WglTarget& target,
                            const std::string& label, RenderShape& rshape, GlCoverageFormat format,
                            uint8_t opacity = 255, const Matrix& transform = identity(),
                            const RenderRegion& viewport = {{0, 0}, {Width, Height}}, uint32_t tolerance = 0,
                            const RenderColor* background = nullptr)
{
    renderer.viewport(viewport);
    Array<RenderData> clips;
    validation.check(renderer.preUpdate(), label + " preUpdate");
    RenderShape backdrop;
    GlShape* backgroundShape = nullptr;
    if (background) {
        backdrop.path.addRect(0.0f, 0.0f, Width, Height, 0.0f, 0.0f, true);
        backdrop.color = *background;
        backgroundShape = static_cast<GlShape*>(renderer.prepare(backdrop, nullptr, identity(), clips, 255, RenderUpdateFlag::All, false));
        validation.check(backgroundShape != nullptr, label + " background prepare");
    }
    auto shape = static_cast<GlShape*>(renderer.prepare(rshape, nullptr, transform, clips, opacity, RenderUpdateFlag::All, false));
    if (!validation.check(shape != nullptr, label + " prepare")) return {};
    validation.check(renderer.postUpdate(), label + " postUpdate");
    const auto& active = rshape.color.a || rshape.fill ? shape->fillCoverage : shape->strokeCoverage;
    validation.check(active.status == CoverageStatus::Ready && active.map.data && active.region.texture && active.format == format,
                     label + " uses expected ready atlas format");
    validation.check(renderer.preRender(), label + " preRender");
    if (backgroundShape) {
        validation.check(renderer.renderShape(backgroundShape), label + " background renderShape");
        validation.check(renderer.blend(BlendMethod::Multiply), label + " Multiply blend");
    }
    validation.check(renderer.renderShape(shape), label + " renderShape");
    if (background) renderer.blend(BlendMethod::Normal);
    validation.check(renderer.postRender(), label + " postRender");
    validation.check(renderer.sync(), label + " sync");

    // The root target stores premultiplied RGBA with OpenGL's bottom-up row order.
    renderer.mStateCache.bindTexture2D(TVG_GL_TEXTURE_SETUP_UNIT, renderer.mRootTarget.colorTex);
    std::vector<uint8_t> frame(size_t(Width) * Height * 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    target.getTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, frame.data());
    uint32_t maxError = 0, partial = 0;
    size_t firstMismatch = frame.size();
    for (uint32_t y = 0; y < Height; ++y) {
        for (uint32_t x = 0; x < Width; ++x) {
            std::array<uint8_t, 4> expected = background ? std::array<uint8_t, 4>{{background->r, background->g, background->b, background->a}} :
                                                        std::array<uint8_t, 4>{{0, 0, 0, 0}};
            auto composite = [&](bool stroke) {
                auto coverage = coveragePixel(stroke ? shape->strokeCoverage.map : shape->fillCoverage.map, x, y);
                if (!coverage || (stroke && !rshape.stroke)) return;
                auto color = paintColor(stroke ? rshape.stroke->color : rshape.color,
                                        stroke ? rshape.stroke->fill : rshape.fill, transform, opacity, x, y);
                auto alpha = color[3] * coverage / 255.0;
                for (uint32_t channel = 0; channel < 4; ++channel) {
                    auto source = channel == 3 ? 255.0 : color[channel] * (background ? expected[channel] : 255.0);
                    auto value = source * alpha + expected[channel] * (1.0 - alpha);
                    expected[channel] = uint8_t(std::max(0.0, std::min(255.0, std::round(value))));
                }
            };
            composite(rshape.strokeFirst());
            composite(!rshape.strokeFirst());
            auto coverage = background ? coveragePixel(active.map, x, y) : expected[3];
            partial += coverage > 0 && coverage < 255;
            auto offset = (size_t(Height - 1 - y) * Width + x) * 4;
            for (uint32_t channel = 0; channel < 4; ++channel) {
                auto error = uint32_t(std::abs(int(frame[offset + channel]) - int(expected[channel])));
                maxError = std::max(maxError, error);
                if (error > tolerance && firstMismatch == frame.size()) firstMismatch = (size_t(y) * Width + x) * 4 + channel;
            }
        }
    }
    if (firstMismatch != frame.size()) {
        std::cerr << label << ": first RGBA difference at " << firstMismatch / 4 % Width << ',' << firstMismatch / 4 / Width
                  << " channel " << firstMismatch % 4 << "; max error=" << maxError << '\n';
    }
    validation.check(maxError <= tolerance, label + " rendered RGBA matches CPU coverage and paint over entire target");
    validation.check(opacity == 0 || partial > 0, label + " includes partial coverage or opacity");
    validation.check(glGetError() == GL_NO_ERROR, label + " no GL error");
    std::cout << "draw," << label << ",max_channel_error," << maxError << '\n';
    if (backgroundShape) renderer.dispose(backgroundShape);
    renderer.dispose(shape);
    return frame;
}

void drawChecks(Validation& validation, GlRenderer& renderer, const WglTarget& target)
{
    RenderShape dense, runs;
    dense.path.moveTo({121.2f, 91.35f}); dense.path.lineTo({122.4f, 91.55f});
    dense.path.lineTo({121.6f, 92.45f}); dense.path.close();
    dense.stroke = new RenderStroke;
    dense.stroke->width = 0.55f;
    dense.stroke->join = StrokeJoin::Round;
    runs.path.addRect(41.25f, 35.5f, 145.5f, 100.25f, 0.0f, 0.0f, true);
    runs.stroke = new RenderStroke;
    runs.stroke->width = 9.25f;
    for (auto shape : {&dense, &runs}) {
        auto format = shape == &dense ? GlCoverageFormat::Dense : GlCoverageFormat::Runs;
        auto name = shape == &dense ? "dense" : "runs";
        shape->color = {255, 255, 255, 255};
        shape->stroke->color = {};
        drawCase(validation, renderer, target, std::string(name) + "-fill", *shape, format);
        shape->color = {};
        shape->stroke->color = {255, 255, 255, 255};
        drawCase(validation, renderer, target, std::string(name) + "-stroke", *shape, format);
    }
    runs.color = {213, 47, 101, 176};
    runs.stroke->color = {23, 139, 221, 208};
    auto fillFirst = drawCase(validation, renderer, target, "color-opacity-fill-first", runs, GlCoverageFormat::Runs, 147, identity(),
                              {{0, 0}, {Width, Height}}, 2);
    runs.stroke->first = true;
    auto strokeFirst = drawCase(validation, renderer, target, "color-opacity-stroke-first", runs, GlCoverageFormat::Runs, 147, identity(),
                                {{0, 0}, {Width, Height}}, 2);
    validation.check(fillFirst != strokeFirst, "strokeFirst changes overlapping fill and stroke colors");
    auto transform = identity();
    transform.e13 = -25.125f; transform.e23 = 18.375f;
    drawCase(validation, renderer, target, "translated-offset-viewport", runs, GlCoverageFormat::Runs, 147, transform,
             {{61, 42}, {158, 109}}, 2);
    drawCase(validation, renderer, target, "zero-opacity", runs, GlCoverageFormat::Runs, 0);

    const Fill::ColorStop stops[] = {{0.0f, 217, 39, 91, 96}, {1.0f, 21, 183, 231, 208}};
    auto gradientTransform = identity();
    gradientTransform.e13 = 7.25f; gradientTransform.e23 = -4.5f;
    transform.e13 = 11.375f; transform.e23 = -8.125f;
    for (auto type : {Type::LinearGradient, Type::RadialGradient, Type::ConicGradient}) {
        Fill* gradient = nullptr;
        const char* name = nullptr;
        if (type == Type::LinearGradient) {
            auto linear = LinearGradient::gen();
            linear->linear(40.0f, 30.0f, 190.0f, 30.0f);
            gradient = linear; name = "linear";
        } else if (type == Type::RadialGradient) {
            auto radial = RadialGradient::gen();
            radial->radial(40.0f, 30.0f, 150.0f, 40.0f, 30.0f, 0.0f);
            gradient = radial; name = "radial";
        } else {
            auto conic = ConicGradient::gen();
            // Keep the seam outside the path so the reference uses ordinary interpolation.
            conic->conic(0.0f, 0.0f, 0.0f);
            gradient = conic; name = "conic";
        }
        gradient->colorStops(stops, 2);
        gradient->transform(gradientTransform);
        runs.color = {}; runs.stroke->color = {};
        runs.fill = gradient;
        drawCase(validation, renderer, target, std::string(name) + "-gradient-fill", runs, GlCoverageFormat::Runs, 173, transform,
                 {{0, 0}, {Width, Height}}, 2);
        runs.fill = nullptr;
        runs.stroke->fill = gradient;
        drawCase(validation, renderer, target, std::string(name) + "-gradient-stroke", runs, GlCoverageFormat::Runs, 173, transform,
                 {{0, 0}, {Width, Height}}, 2);
        runs.stroke->fill = nullptr;
        delete gradient;
    }
    runs.color = {128, 192, 64, 255};
    const RenderColor background = {96, 144, 208, 255};
    drawCase(validation, renderer, target, "multiply-coverage", runs, GlCoverageFormat::Runs, 255, identity(),
             {{0, 0}, {Width, Height}}, 2, &background);
    renderer.viewport({{0, 0}, {Width, Height}});
}

} // namespace

int main(int argc, char** argv)
{
    Validation validation;
    bool capture = false, updatesOnly = false, drawOnly = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--capture") capture = true;
        else if (std::string(argv[i]) == "--updates-only") updatesOnly = true;
        else if (std::string(argv[i]) == "--draw-only") drawOnly = true;
        else if (validation.directory.empty()) validation.directory = argv[i];
        else { std::cerr << "Usage: tvgCoverageStorageTests [output-directory] [--capture|--updates-only|--draw-only]\n"; return 2; }
    }
    if (!validation.directory.empty() && !drawOnly) {
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
    if (drawOnly) {
        drawChecks(validation, *renderer, target);
        renderer.reset();
        validation.check(Initializer::term() == Result::Success, "terminate");
        std::cout << "coverage drawing: checks=" << validation.checks << " failures=" << validation.failures << '\n';
        return validation.failures ? 1 : 0;
    }
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
        }
        validation.check(renderer->postUpdate(), "postUpdate");
        if (target.objectLabel) {
            for (auto shape : prepared) if (shape) {
                for (auto coverage : {&shape->fillCoverage, &shape->strokeCoverage}) {
                    auto label = "coverage-page" + std::to_string(coverage->region.texture) + (coverage->format == GlCoverageFormat::Runs ? "#RGBA16UI" : "#R8");
                    if (coverage->region.texture) target.objectLabel(GL_TEXTURE, coverage->region.texture, -1, label.c_str());
                }
            }
        }
    };
    {
        GlCalls calls;
        prepareAll();
        std::set<GLuint> pages;
        for (auto shape : prepared) if (shape) {
            if (shape->fillCoverage.region.texture) pages.insert(shape->fillCoverage.region.texture);
            if (shape->strokeCoverage.region.texture) pages.insert(shape->strokeCoverage.region.texture);
        }
        validation.check(calls.created == pages.size() && calls.allocated == pages.size() && calls.uploaded == pages.size() && calls.bound == pages.size(), "first batch creates, allocates, uploads and binds once per page");
        validation.check(pages.size() < 16, "sixteen maps use fewer page textures");
    }
    const char* header = "map,page_texture,min_x,min_y,max_x,max_y,nonzero_pixels,partial_pixels,cropped_dense_bytes,gpu_format,page_width,page_height,page_reserved_bytes,region_x,region_y,region_width,region_height,run_count,payload_bytes,payload_reduction_percent,exact\n";
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
        auto fillRegion = prepared[0]->fillCoverage.region, strokeRegion = prepared[0]->strokeCoverage.region;
        auto fillBuffer = prepared[0]->fillCoverage.map.data, strokeBuffer = prepared[0]->strokeCoverage.map.data;
        {
            GlCalls calls;
            renderer->prepare(rshapes[0], prepared[0], transform, clips, 255, RenderUpdateFlag::None, false);
            validation.check(renderer->postUpdate(), "unchanged postUpdate");
            validation.check(calls.created == 0 && calls.allocated == 0 && calls.uploaded == 0 && calls.bound == 0, "unchanged prepare has no texture calls");
        }
        validation.check(sameRegion(fillRegion, prepared[0]->fillCoverage.region) && sameRegion(strokeRegion, prepared[0]->strokeCoverage.region) &&
                         fillBuffer == prepared[0]->fillCoverage.map.data && strokeBuffer == prepared[0]->strokeCoverage.map.data, "unchanged prepare preserves CPU maps and atlas regions");
        GLuint unpackBuffer = 0;
        glGenBuffers(1, &unpackBuffer); glBindBuffer(GL_PIXEL_UNPACK_BUFFER, unpackBuffer);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, 256, nullptr, GL_STATIC_DRAW);
        const GLenum parameters[] = {GL_UNPACK_ALIGNMENT, GL_UNPACK_ROW_LENGTH, GL_UNPACK_SKIP_PIXELS, GL_UNPACK_SKIP_ROWS, GL_UNPACK_SWAP_BYTES};
        const GLint values[] = {8, 17, 2, 3, GL_TRUE};
        for (uint32_t i = 0; i < 5; ++i) glPixelStorei(parameters[i], values[i]);
        {
            GlCalls calls;
            renderer->prepare(rshapes[0], prepared[0], transform, clips, 255, RenderUpdateFlag::All, false);
            validation.check(renderer->postUpdate(), "host unpack postUpdate");
            auto pages = fillRegion.texture == strokeRegion.texture ? 1u : 2u;
            validation.check(calls.created == 0 && calls.allocated == 0 && calls.uploaded == pages && calls.bound <= pages, "same size renderer update uploads once per dirty page without new storage");
        }
        validation.check(sameRegion(fillRegion, prepared[0]->fillCoverage.region) && sameRegion(strokeRegion, prepared[0]->strokeCoverage.region), "same size prepare preserves atlas regions");
        for (uint32_t i = 0; i < 5; ++i) { GLint actual; glGetIntegerv(parameters[i], &actual); validation.check(actual == values[i], "host unpack state restored"); }
        GLint actualBuffer; glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &actualBuffer);
        validation.check(actualBuffer == int32_t(unpackBuffer), "host unpack buffer restored");
        validation.read(*renderer, target, prepared[0]->fillCoverage, "host-unpack-fill");
        validation.read(*renderer, target, prepared[0]->strokeCoverage, "host-unpack-stroke");
        if (prepared[1]) validation.read(*renderer, target, prepared[1]->fillCoverage, "unchanged-neighbor-fill");
        for (uint32_t i = 0; i < 5; ++i) glPixelStorei(parameters[i], i == 0 ? 4 : 0);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0); glDeleteBuffers(1, &unpackBuffer);
        renderer->sync();
        auto previous = prepared[0]->fillCoverage.map.bounds;
        transform.e13 = 9; transform.e23 = -3;
        renderer->prepare(rshapes[0], prepared[0], transform, clips, 255, RenderUpdateFlag::Transform, false);
        validation.check(renderer->postUpdate(), "geometry update postUpdate");
        validation.check(!(prepared[0]->fillCoverage.map.bounds == previous), "geometry update changes map bounds");
        validation.read(*renderer, target, prepared[0]->fillCoverage, "updated-fill");
        validation.read(*renderer, target, prepared[0]->strokeCoverage, "updated-stroke");
        delete rshapes[0].stroke; rshapes[0].stroke = nullptr;
        renderer->prepare(rshapes[0], prepared[0], transform, clips, 255, RenderUpdateFlag::Stroke, false);
        validation.check(renderer->postUpdate(), "stroke removal postUpdate");
        validation.check(prepared[0]->strokeCoverage.status == CoverageStatus::Empty && !prepared[0]->strokeCoverage.map.data &&
                         !prepared[0]->strokeCoverage.region.texture && !prepared[0]->strokeCoverage.byteSize, "removing stroke clears CPU map and atlas region");
        validation.read(*renderer, target, prepared[0]->fillCoverage, "no-stroke-fill");
        rshapes[0].path.clear();
        renderer->prepare(rshapes[0], prepared[0], transform, clips, 255, RenderUpdateFlag::Path, false);
        validation.check(renderer->postUpdate(), "empty path postUpdate");
        validation.check(prepared[0]->fillCoverage.status == CoverageStatus::Empty && !prepared[0]->fillCoverage.map.data &&
                         !prepared[0]->fillCoverage.region.texture && !prepared[0]->fillCoverage.byteSize, "empty path clears CPU map and atlas region");
    }
    if (!updatesOnly) validation.check(validation.maps == 16 && validation.textures.size() < validation.maps && validation.payloadBytes < validation.denseBytes, "sixteen exact independent maps share pages with compact payloads");
    atlasChecks(validation, *renderer, target);
    if (prepared[1]) {
        auto buffer = prepared[1]->fillCoverage.map.data;
        validation.check(renderer->target(nullptr, target.dc, target.context, 0, Width, Height, ColorSpace::ABGR8888S) == Result::Success, "reset renderer target");
        for (auto shape : prepared) if (shape) validation.check(!shape->fillCoverage.region.texture && !shape->strokeCoverage.region.texture, "target reset clears atlas region handles");
        renderer->prepare(rshapes[1], prepared[1], identity(), clips, 255, RenderUpdateFlag::None, false);
        validation.check(renderer->postUpdate(), "target reset postUpdate");
        validation.check(prepared[1]->fillCoverage.map.data == buffer, "target reset preserves CPU coverage");
        validation.read(*renderer, target, prepared[1]->fillCoverage, "target-reset-fill");
        validation.read(*renderer, target, prepared[1]->strokeCoverage, "target-reset-stroke");
    }
    for (auto shape : prepared) if (shape) renderer->dispose(shape);
    drawChecks(validation, *renderer, target);
    renderer.reset();
    validation.check(Initializer::term() == Result::Success, "terminate");
    std::cerr << "coverage storage: maps=" << validation.maps << " page_textures=" << validation.textures.size() << " cropped_dense_bytes=" << validation.denseBytes
              << " payload_bytes=" << validation.payloadBytes << " page_reserved_bytes=" << validation.reservedBytes
              << " checks=" << validation.checks << " failures=" << validation.failures << '\n';
    return validation.failures ? 1 : 0;
}
