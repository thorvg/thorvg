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
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <thorvg.h>

#include "aa_poc_cli.h"
#include "aa_poc_gl.h"
#include "aa_product_renderer.h"
#include "aa_product_scenes.h"

using namespace tvg;

namespace
{

using Clock = std::chrono::steady_clock;

struct PaintReleaser
{
    void operator()(Paint* paint) const { Paint::rel(paint); }
};

using ShapePtr = std::unique_ptr<Shape, PaintReleaser>;

enum class Suite
{
    Quality,
    Headline,
};

struct ModeSpec
{
    aa_poc::ProductAaMode mode;
    const char* name;
};

struct ScaleSpec
{
    const char* name;
    float coordinateScale;
    uint32_t targetSize;
};

struct Offset
{
    float x;
    float y;
};

struct Options
{
    Suite suite = Suite::Quality;
    bool allModes = true;
    aa_poc::ProductAaMode mode = aa_poc::ProductAaMode::NoAa;
    bool allScenes = true;
    aa_poc::SceneKind scene = aa_poc::SceneKind::FlatCore;
    bool allScales = true;
    uint32_t scaleIndex = 0;
    std::string outputDir = "aa_product_poc-output";
    float offsetX = 0.0f;
    float offsetY = 0.0f;
    bool hasOffsetX = false;
    bool hasOffsetY = false;
    uint32_t warmup = 100;
    uint32_t frames = 1000;
    uint32_t repetitions = 5;
};

struct TimingResult
{
    std::vector<double> nsPerFrame;
    double medianNsPerFrame = 0.0;
    uint32_t rootSamples = 0;
    uint64_t actualRouteCount = 0;
    uint64_t fallbackCount = 0;
    bool routeValid = false;
};

struct QualityResult
{
    aa_poc::ProductAaMode selectedMode = aa_poc::ProductAaMode::NoAa;
    uint32_t rootSamples = 0;
    uint64_t actualRouteCount = 0;
    uint64_t totalRouteCount = 0;
    uint64_t fallbackCount = 0;
    bool routeValid = false;
};

constexpr ModeSpec MODE_SPECS[] = {
    {aa_poc::ProductAaMode::NoAa, "noaa"},
    {aa_poc::ProductAaMode::Msaa4, "msaa4"},
    {aa_poc::ProductAaMode::FlatDirect, "flat-direct"},
    {aa_poc::ProductAaMode::CurveDirect, "curve-direct"},
    {aa_poc::ProductAaMode::FlatMask, "flat-mask"},
    {aa_poc::ProductAaMode::CurveMask, "curve-mask"},
    {aa_poc::ProductAaMode::Hybrid, "hybrid"},
};

constexpr aa_poc::SceneKind SCENES[] = {
    aa_poc::SceneKind::FlatCore,
    aa_poc::SceneKind::CurveCore,
    aa_poc::SceneKind::MixedProductTile,
    aa_poc::SceneKind::TransparencyCore,
};

constexpr ScaleSpec SCALES[] = {
    {"icon", 0.25f, 64},
    {"component", 1.0f, 256},
    {"large", 4.0f, 1024},
};

constexpr Offset QUALITY_OFFSETS[] = {
    {0.0f, 0.0f},
    {0.125f, 0.375f},
    {0.5f, 0.5f},
    {0.875f, 0.625f},
};

constexpr Offset HEADLINE_OFFSET = {0.375f, 0.625f};

static_assert(aa_poc::PRODUCT_SCENE_SIZE == 256,
              "product scale presets assume a 256 x 256 authored scene");

const char* modeName(aa_poc::ProductAaMode mode)
{
    for (const auto& spec : MODE_SPECS) {
        if (spec.mode == mode) return spec.name;
    }
    return "unknown";
}

bool parseMode(const char* text, aa_poc::ProductAaMode& mode)
{
    for (const auto& spec : MODE_SPECS) {
        if (std::strcmp(text, spec.name) == 0) {
            mode = spec.mode;
            return true;
        }
    }
    return false;
}

bool parseUnsigned(const char* text, uint32_t& value, bool allowZero)
{
    if (!text || !*text || *text == '-') return false;
    errno = 0;
    char* end = nullptr;
    auto parsed = std::strtoul(text, &end, 10);
    if (errno || end == text || *end != '\0' || parsed > std::numeric_limits<uint32_t>::max() || (!allowZero && parsed == 0)) {
        return false;
    }
    value = static_cast<uint32_t>(parsed);
    return true;
}

void printUsage(const char* executable)
{
    std::printf(
        "usage: %s [options]\n"
        "  --suite quality|headline\n"
        "  --mode all|noaa|msaa4|flat-direct|curve-direct|flat-mask|curve-mask|hybrid\n"
        "  --scene all|flat-core|curve-core|mixed-product-tile|transparency-core\n"
        "  --scale all|icon|component|large\n"
        "  --output-dir DIR\n"
        "  --offset-x PX\n"
        "  --offset-y PX\n"
        "  --warmup FRAMES\n"
        "  --frames FRAMES\n"
        "  --repetitions COUNT\n",
        executable);
}

bool parseOptions(int argc, char** argv, Options& options)
{
    for (int index = 1; index < argc; ++index) {
        auto argument = argv[index];
        if (std::strcmp(argument, "--help") == 0 || std::strcmp(argument, "-h") == 0) {
            printUsage(argv[0]);
            std::exit(EXIT_SUCCESS);
        }
        if (index + 1 >= argc) {
            std::fprintf(stderr, "aa_product_poc: missing value for %s\n", argument);
            return false;
        }
        auto value = argv[++index];
        if (std::strcmp(argument, "--suite") == 0) {
            if (std::strcmp(value, "quality") == 0) options.suite = Suite::Quality;
            else if (std::strcmp(value, "headline") == 0) options.suite = Suite::Headline;
            else return false;
        } else if (std::strcmp(argument, "--mode") == 0) {
            options.allModes = std::strcmp(value, "all") == 0;
            if (!options.allModes && !parseMode(value, options.mode)) return false;
        } else if (std::strcmp(argument, "--scene") == 0) {
            options.allScenes = std::strcmp(value, "all") == 0;
            if (!options.allScenes && !aa_poc::parseScene(value, options.scene)) return false;
        } else if (std::strcmp(argument, "--scale") == 0) {
            options.allScales = std::strcmp(value, "all") == 0;
            if (!options.allScales) {
                auto matched = false;
                for (uint32_t i = 0; i < sizeof(SCALES) / sizeof(SCALES[0]); ++i) {
                    if (std::strcmp(value, SCALES[i].name) == 0) {
                        options.scaleIndex = i;
                        matched = true;
                        break;
                    }
                }
                if (!matched) return false;
            }
        } else if (std::strcmp(argument, "--output-dir") == 0) {
            if (!*value) return false;
            options.outputDir = value;
        } else if (std::strcmp(argument, "--offset-x") == 0) {
            if (!aa_poc::parseFloat(value, options.offsetX)) return false;
            options.hasOffsetX = true;
        } else if (std::strcmp(argument, "--offset-y") == 0) {
            if (!aa_poc::parseFloat(value, options.offsetY)) return false;
            options.hasOffsetY = true;
        } else if (std::strcmp(argument, "--warmup") == 0) {
            if (!parseUnsigned(value, options.warmup, true)) return false;
        } else if (std::strcmp(argument, "--frames") == 0) {
            if (!parseUnsigned(value, options.frames, false)) return false;
        } else if (std::strcmp(argument, "--repetitions") == 0) {
            if (!parseUnsigned(value, options.repetitions, false)) return false;
        } else {
            std::fprintf(stderr, "aa_product_poc: unknown option: %s\n", argument);
            return false;
        }
    }
    return true;
}

bool checkResult(Result result, const char* operation, const std::string& diagnostic)
{
    if (result == Result::Success) return true;
    std::fprintf(stderr, "%s: %s failed (Result=%u)\n", diagnostic.c_str(), operation,
                 static_cast<unsigned>(result));
    return false;
}

bool makeDirectory(const std::string& path)
{
    if (aa_poc::makeOutputDirectory(path)) return true;
    std::fprintf(stderr, "aa_product_poc: cannot create output directory: %s\n", path.c_str());
    return false;
}

std::string joinPath(const std::string& left, const std::string& right)
{
    if (left.empty()) return right;
    if (left.back() == '/') return left + right;
    return left + "/" + right;
}

std::string manifestPath(const Options& options, const std::string& filename)
{
    auto prefix = options.outputDir;
    while (prefix.size() > 1 && prefix.back() == '/')
        prefix.pop_back();
    prefix += '/';
    if (filename.compare(0, prefix.size(), prefix) == 0) {
        return filename.substr(prefix.size());
    }
    return filename;
}

std::string numberKey(float value)
{
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.3f", static_cast<double>(value));
    std::string key = buffer;
    for (auto& ch : key) {
        if (ch == '-') ch = 'm';
        else if (ch == '.') ch = 'p';
    }
    return key;
}

std::string qualityDirectory(const Options& options, aa_poc::SceneKind scene,
                             const ScaleSpec& scale, const Offset& offset)
{
    auto quality = joinPath(options.outputDir, "quality");
    auto sceneDir = joinPath(quality, aa_poc::sceneName(scene));
    auto scaleDir = joinPath(sceneDir, scale.name);
    auto offsetDir = joinPath(scaleDir,
                              "offset-x" + numberKey(offset.x) + "-y" + numberKey(offset.y));
    if (!makeDirectory(options.outputDir) || !makeDirectory(quality) || !makeDirectory(sceneDir) || !makeDirectory(scaleDir) || !makeDirectory(offsetDir)) {
        return {};
    }
    return offsetDir;
}

std::vector<aa_poc::ProductAaMode> selectedModes(const Options& options)
{
    std::vector<aa_poc::ProductAaMode> modes;
    if (!options.allModes) {
        modes.push_back(options.mode);
        return modes;
    }
    for (const auto& spec : MODE_SPECS)
        modes.push_back(spec.mode);
    return modes;
}

std::vector<aa_poc::SceneKind> selectedScenes(const Options& options)
{
    if (!options.allScenes) return {options.scene};
    return {SCENES, SCENES + sizeof(SCENES) / sizeof(SCENES[0])};
}

std::vector<ScaleSpec> selectedScales(const Options& options)
{
    if (!options.allScales) return {SCALES[options.scaleIndex]};
    return {SCALES, SCALES + sizeof(SCALES) / sizeof(SCALES[0])};
}

std::vector<Offset> selectedOffsets(const Options& options)
{
    if (options.hasOffsetX || options.hasOffsetY) {
        auto defaultOffset = options.suite == Suite::Headline ? HEADLINE_OFFSET : Offset{0.0f, 0.0f};
        return {{options.hasOffsetX ? options.offsetX : defaultOffset.x,
                 options.hasOffsetY ? options.offsetY : defaultOffset.y}};
    }
    if (options.suite == Suite::Headline) return {HEADLINE_OFFSET};
    return {QUALITY_OFFSETS, QUALITY_OFFSETS + sizeof(QUALITY_OFFSETS) / sizeof(QUALITY_OFFSETS[0])};
}

bool validateTargetSize(uint32_t size, const std::string& diagnostic)
{
    GLint maxRenderbufferSize = 0;
    GLint maxTextureSize = 0;
    glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &maxRenderbufferSize);
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
    if (size <= static_cast<uint32_t>(maxRenderbufferSize) && size <= static_cast<uint32_t>(maxTextureSize)) {
        return true;
    }
    std::fprintf(stderr,
                 "%s: target %ux%u exceeds GL_MAX_RENDERBUFFER_SIZE=%d or GL_MAX_TEXTURE_SIZE=%d\n",
                 diagnostic.c_str(), size, size, maxRenderbufferSize, maxTextureSize);
    return false;
}

bool prepareProductCanvas(aa_poc::GlContext& context, aa_poc::RenderTarget& target,
                          std::unique_ptr<GlCanvas>& canvas,
                          aa_poc::ProductAaMode mode, aa_poc::SceneKind scene,
                          uint32_t targetSize, float coordinateScale,
                          float offsetX, float offsetY, const std::string& diagnostic)
{
    if (!validateTargetSize(targetSize, diagnostic)) return false;
    if (!target.init(targetSize, targetSize, 1, diagnostic.c_str())) return false;

    canvas.reset(GlCanvas::gen(EngineOption::Default));
    if (!canvas) {
        std::fprintf(stderr, "%s: GlCanvas::gen() failed\n", diagnostic.c_str());
        return false;
    }
    if (!aa_poc::setProductAaMode(*canvas, mode)) {
        std::fprintf(stderr, "%s: AA mode must be selected before target creation\n",
                     diagnostic.c_str());
        return false;
    }
    if (!checkResult(context.target(*canvas, target.framebuffer(), targetSize, targetSize,
                                    ColorSpace::ABGR8888S),
                     "GlCanvas::target", diagnostic)) {
        return false;
    }
    if (aa_poc::setProductAaMode(*canvas, mode)) {
        std::fprintf(stderr, "%s: AA mode selector accepted a post-target change\n",
                     diagnostic.c_str());
        return false;
    }
    if (!aa_poc::populateProductScene(*canvas, scene, coordinateScale,
                                      offsetX, offsetY, diagnostic.c_str())) {
        return false;
    }
    return true;
}

bool submitFrame(GlCanvas& canvas, const std::string& diagnostic)
{
    return checkResult(canvas.update(), "Canvas::update", diagnostic) && checkResult(canvas.draw(true), "Canvas::draw", diagnostic) && checkResult(canvas.sync(), "Canvas::sync", diagnostic);
}

bool validateHybridSelfIntersectionRoute(aa_poc::GlContext& context)
{
    constexpr auto DIAGNOSTIC = "aa_product_poc hybrid self-intersection route";
    aa_poc::RenderTarget target;
    if (!target.init(64, 64, 1, DIAGNOSTIC)) return false;

    std::unique_ptr<GlCanvas> canvas(GlCanvas::gen(EngineOption::Default));
    if (!canvas || !aa_poc::setProductAaMode(*canvas, aa_poc::ProductAaMode::Hybrid) || !checkResult(context.target(*canvas, target.framebuffer(), 64, 64, ColorSpace::ABGR8888S), "GlCanvas::target", DIAGNOSTIC)) {
        return false;
    }

    ShapePtr shape(Shape::gen());
    if (!shape || !checkResult(shape->moveTo(8.0f, 8.0f), "Shape::moveTo", DIAGNOSTIC) || !checkResult(shape->lineTo(56.0f, 56.0f), "Shape::lineTo", DIAGNOSTIC) || !checkResult(shape->lineTo(8.0f, 56.0f), "Shape::lineTo", DIAGNOSTIC) || !checkResult(shape->lineTo(56.0f, 8.0f), "Shape::lineTo", DIAGNOSTIC) || !checkResult(shape->close(), "Shape::close", DIAGNOSTIC) || !checkResult(shape->fill(73, 141, 224, 255), "Shape::fill", DIAGNOSTIC) || !checkResult(canvas->add(shape.get()), "Canvas::add", DIAGNOSTIC)) {
        return false;
    }
    shape.release();

    ShapePtr hairpin(Shape::gen());
    if (!hairpin || !checkResult(hairpin->moveTo(8.0f, 8.0f), "Shape::moveTo", DIAGNOSTIC) || !checkResult(hairpin->lineTo(56.0f, 8.0f), "Shape::lineTo", DIAGNOSTIC) || !checkResult(hairpin->lineTo(56.0f, 56.0f), "Shape::lineTo", DIAGNOSTIC) || !checkResult(hairpin->lineTo(32.4f, 56.0f), "Shape::lineTo", DIAGNOSTIC) || !checkResult(hairpin->lineTo(32.4f, 16.0f), "Shape::lineTo", DIAGNOSTIC) || !checkResult(hairpin->lineTo(31.8f, 16.0f), "Shape::lineTo", DIAGNOSTIC) || !checkResult(hairpin->lineTo(31.8f, 56.0f), "Shape::lineTo", DIAGNOSTIC) || !checkResult(hairpin->lineTo(8.0f, 56.0f), "Shape::lineTo", DIAGNOSTIC) || !checkResult(hairpin->close(), "Shape::close", DIAGNOSTIC) || !checkResult(hairpin->fill(224, 109, 73, 255), "Shape::fill", DIAGNOSTIC) || !checkResult(canvas->add(hairpin.get()), "Canvas::add", DIAGNOSTIC)) {
        return false;
    }
    hairpin.release();

    ShapePtr crossingCubics(Shape::gen());
    if (!crossingCubics || !checkResult(crossingCubics->moveTo(8.0f, 20.0f), "Shape::moveTo", DIAGNOSTIC) || !checkResult(crossingCubics->cubicTo(19.0f, 8.0f, 43.0f, 6.0f, 56.0f, 20.0f), "Shape::cubicTo", DIAGNOSTIC) || !checkResult(crossingCubics->cubicTo(42.7f, 6.5f, 19.4f, 8.3f, 8.0f, 20.5f), "Shape::cubicTo", DIAGNOSTIC) || !checkResult(crossingCubics->close(), "Shape::close", DIAGNOSTIC) || !checkResult(crossingCubics->fill(181, 89, 204, 255), "Shape::fill", DIAGNOSTIC) || !checkResult(canvas->add(crossingCubics.get()), "Canvas::add", DIAGNOSTIC)) {
        return false;
    }
    crossingCubics.release();

    ShapePtr disjointCompound(Shape::gen());
    if (!disjointCompound || !checkResult(disjointCompound->moveTo(2.0f, 2.0f), "Shape::moveTo", DIAGNOSTIC) || !checkResult(disjointCompound->lineTo(12.0f, 2.0f), "Shape::lineTo", DIAGNOSTIC) || !checkResult(disjointCompound->lineTo(12.0f, 12.0f), "Shape::lineTo", DIAGNOSTIC) || !checkResult(disjointCompound->lineTo(2.0f, 12.0f), "Shape::lineTo", DIAGNOSTIC) || !checkResult(disjointCompound->close(), "Shape::close", DIAGNOSTIC) || !checkResult(disjointCompound->moveTo(52.0f, 52.0f), "Shape::moveTo", DIAGNOSTIC) || !checkResult(disjointCompound->lineTo(62.0f, 52.0f), "Shape::lineTo", DIAGNOSTIC) || !checkResult(disjointCompound->lineTo(62.0f, 62.0f), "Shape::lineTo", DIAGNOSTIC) || !checkResult(disjointCompound->lineTo(52.0f, 62.0f), "Shape::lineTo", DIAGNOSTIC) || !checkResult(disjointCompound->close(), "Shape::close", DIAGNOSTIC) || !checkResult(disjointCompound->fill(91, 192, 116, 255), "Shape::fill", DIAGNOSTIC) || !checkResult(canvas->add(disjointCompound.get()), "Canvas::add", DIAGNOSTIC)) {
        return false;
    }
    disjointCompound.release();

    ShapePtr malformed(Shape::gen());
    if (!malformed || !checkResult(malformed->moveTo(16.0f, 16.0f), "Shape::moveTo", DIAGNOSTIC) || !checkResult(malformed->lineTo(48.0f, 16.0f), "Shape::lineTo", DIAGNOSTIC) || !checkResult(malformed->lineTo(32.0f, 48.0f), "Shape::lineTo", DIAGNOSTIC) || !checkResult(malformed->close(), "Shape::close", DIAGNOSTIC) || !checkResult(malformed->cubicTo(48.0f, 48.0f, 16.0f, 48.0f, 16.0f, 16.0f), "Shape::cubicTo", DIAGNOSTIC) || !checkResult(malformed->fill(232, 189, 74, 255), "Shape::fill", DIAGNOSTIC) || !checkResult(canvas->add(malformed.get()), "Canvas::add", DIAGNOSTIC)) {
        return false;
    }
    malformed.release();

    aa_poc::resetProductAaStats(*canvas);
    if (!submitFrame(*canvas, DIAGNOSTIC)) return false;
    glFinish();
    auto stats = aa_poc::productAaStats(*canvas);
    auto total = stats.noAa + stats.msaa4 + stats.flatDirect + stats.curveDirect + stats.flatMask + stats.curveMask;
    auto valid = stats.mode == aa_poc::ProductAaMode::Hybrid && stats.rootSamples == 1 && stats.curveDirect == 1 && stats.curveMask == 3 && stats.fallback == 1 && total == 4;
    if (!valid) {
        std::fprintf(stderr,
                     "%s: expected one curve-direct, three curve-mask, and one fallback route, got curve-direct=%llu "
                     "curve-mask=%llu fallback=%llu total=%llu root=%u\n",
                     DIAGNOSTIC,
                     static_cast<unsigned long long>(stats.curveDirect),
                     static_cast<unsigned long long>(stats.curveMask),
                     static_cast<unsigned long long>(stats.fallback),
                     static_cast<unsigned long long>(total), stats.rootSamples);
    }
    return valid;
}

bool validateSingleSampleFallbackTarget(aa_poc::GlContext& context)
{
    constexpr auto DIAGNOSTIC = "aa_product_poc single-sample blend fallback";
    aa_poc::RenderTarget target;
    if (!target.init(64, 64, 1, DIAGNOSTIC)) return false;

    std::unique_ptr<GlCanvas> canvas(GlCanvas::gen(EngineOption::Default));
    if (!canvas || !aa_poc::setProductAaMode(*canvas, aa_poc::ProductAaMode::Hybrid) || !checkResult(context.target(*canvas, target.framebuffer(), 64, 64, ColorSpace::ABGR8888S), "GlCanvas::target", DIAGNOSTIC)) {
        return false;
    }

    auto addRectangle = [&](float x0, float y0, float x1, float y1,
                            uint8_t r, uint8_t g, uint8_t b,
                            BlendMethod blend) {
        ShapePtr shape(Shape::gen());
        if (!shape || !checkResult(shape->moveTo(x0, y0), "Shape::moveTo", DIAGNOSTIC) || !checkResult(shape->lineTo(x1, y0), "Shape::lineTo", DIAGNOSTIC) || !checkResult(shape->lineTo(x1, y1), "Shape::lineTo", DIAGNOSTIC) || !checkResult(shape->lineTo(x0, y1), "Shape::lineTo", DIAGNOSTIC) || !checkResult(shape->close(), "Shape::close", DIAGNOSTIC) || !checkResult(shape->fill(r, g, b, 255), "Shape::fill", DIAGNOSTIC) || !checkResult(shape->blend(blend), "Paint::blend", DIAGNOSTIC) || !checkResult(canvas->add(shape.get()), "Canvas::add", DIAGNOSTIC)) {
            return false;
        }
        shape.release();
        return true;
    };

    if (!addRectangle(0.0f, 0.0f, 64.0f, 64.0f, 128, 128, 128,
                      BlendMethod::Normal)
        || !addRectangle(16.0f, 16.0f, 48.0f, 48.0f, 200, 100, 50,
                         BlendMethod::Multiply)) {
        return false;
    }

    aa_poc::resetProductAaStats(*canvas);
    if (!submitFrame(*canvas, DIAGNOSTIC)) return false;
    glFinish();
    auto stats = aa_poc::productAaStats(*canvas);

    uint8_t pixel[4] = {};
    glBindFramebuffer(GL_FRAMEBUFFER, target.framebuffer());
    glReadPixels(32, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    auto close = [](uint8_t actual, uint8_t expected) {
        return std::abs(static_cast<int>(actual) - static_cast<int>(expected)) <= 2;
    };
    auto valid = stats.mode == aa_poc::ProductAaMode::Hybrid && stats.rootSamples == 1 && stats.curveDirect == 1 && stats.fallback == 1 && close(pixel[0], 100) && close(pixel[1], 50) && close(pixel[2], 25) && pixel[3] == 255;
    if (!valid) {
        std::fprintf(stderr,
                     "%s: expected rgba=(100,50,25,255), got (%u,%u,%u,%u); "
                     "curve-direct=%llu fallback=%llu root=%u\n",
                     DIAGNOSTIC, pixel[0], pixel[1], pixel[2], pixel[3],
                     static_cast<unsigned long long>(stats.curveDirect),
                     static_cast<unsigned long long>(stats.fallback),
                     stats.rootSamples);
    }
    return valid;
}

aa_poc::ProductAaStats expectedRoutes(aa_poc::ProductAaMode mode,
                                      aa_poc::SceneKind scene)
{
    aa_poc::ProductAaStats expected;
    auto shapes = aa_poc::expectedShapeCount(scene);
    switch (mode) {
        case aa_poc::ProductAaMode::NoAa: expected.noAa = shapes; break;
        case aa_poc::ProductAaMode::Msaa4: expected.msaa4 = shapes; break;
        case aa_poc::ProductAaMode::FlatDirect: expected.flatDirect = shapes; break;
        case aa_poc::ProductAaMode::CurveDirect: expected.curveDirect = shapes; break;
        case aa_poc::ProductAaMode::FlatMask: expected.flatMask = shapes; break;
        case aa_poc::ProductAaMode::CurveMask: expected.curveMask = shapes; break;
        case aa_poc::ProductAaMode::Hybrid:
            if (scene == aa_poc::SceneKind::TransparencyCore) {
                expected.curveMask = shapes;
            } else {
                expected.curveDirect = shapes;
            }
            break;
    }
    return expected;
}

uint64_t expectedRouteCount(const aa_poc::ProductAaStats& stats,
                            const aa_poc::ProductAaStats& expected)
{
    uint64_t count = 0;
    if (expected.noAa) count += stats.noAa;
    if (expected.msaa4) count += stats.msaa4;
    if (expected.flatDirect) count += stats.flatDirect;
    if (expected.curveDirect) count += stats.curveDirect;
    if (expected.flatMask) count += stats.flatMask;
    if (expected.curveMask) count += stats.curveMask;
    return count;
}

bool validateStats(const aa_poc::ProductAaStats& stats,
                   aa_poc::ProductAaMode requestedMode,
                   aa_poc::SceneKind scene, uint64_t renderedFrames,
                   const std::string& diagnostic)
{
    auto requiredSamples = requestedMode == aa_poc::ProductAaMode::Msaa4 ? 4u : 1u;
    auto expectedRoutesPerFrame = expectedRoutes(requestedMode, scene);
    auto expected = static_cast<uint64_t>(aa_poc::expectedShapeCount(scene)) * renderedFrames;
    auto total = stats.noAa + stats.msaa4 + stats.flatDirect + stats.curveDirect + stats.flatMask + stats.curveMask;
    auto valid = stats.mode == requestedMode && stats.rootSamples == requiredSamples && stats.fallback == 0 && total == expected && stats.noAa == expectedRoutesPerFrame.noAa * renderedFrames && stats.msaa4 == expectedRoutesPerFrame.msaa4 * renderedFrames && stats.flatDirect == expectedRoutesPerFrame.flatDirect * renderedFrames && stats.curveDirect == expectedRoutesPerFrame.curveDirect * renderedFrames && stats.flatMask == expectedRoutesPerFrame.flatMask * renderedFrames && stats.curveMask == expectedRoutesPerFrame.curveMask * renderedFrames;
    if (valid) return true;

    std::fprintf(stderr,
                 "%s: AA route assertion failed: requested=%s recorded=%s root=%u expected-root=%u "
                 "expected-count=%llu expected-direct=%llu expected-mask=%llu "
                 "noaa=%llu msaa4=%llu flat-direct=%llu "
                 "curve-direct=%llu flat-mask=%llu curve-mask=%llu fallback=%llu\n",
                 diagnostic.c_str(), modeName(requestedMode), modeName(stats.mode),
                 stats.rootSamples, requiredSamples,
                 static_cast<unsigned long long>(expected),
                 static_cast<unsigned long long>(expectedRoutesPerFrame.curveDirect * renderedFrames),
                 static_cast<unsigned long long>(expectedRoutesPerFrame.curveMask * renderedFrames),
                 static_cast<unsigned long long>(stats.noAa),
                 static_cast<unsigned long long>(stats.msaa4),
                 static_cast<unsigned long long>(stats.flatDirect),
                 static_cast<unsigned long long>(stats.curveDirect),
                 static_cast<unsigned long long>(stats.flatMask),
                 static_cast<unsigned long long>(stats.curveMask),
                 static_cast<unsigned long long>(stats.fallback));
    return false;
}

bool renderQualityImage(aa_poc::GlContext& context, aa_poc::ProductAaMode mode,
                        aa_poc::SceneKind scene, uint32_t targetSize,
                        float coordinateScale, const Offset& offset,
                        uint32_t downsample, const std::string& filename,
                        const std::string& diagnostic, QualityResult& result)
{
    aa_poc::RenderTarget target;
    std::unique_ptr<GlCanvas> canvas;
    if (!prepareProductCanvas(context, target, canvas, mode, scene,
                              targetSize, coordinateScale, offset.x, offset.y,
                              diagnostic)) {
        return false;
    }

    aa_poc::resetProductAaStats(*canvas);
    if (!submitFrame(*canvas, diagnostic)) return false;
    glFinish();
    auto stats = aa_poc::productAaStats(*canvas);
    auto expected = expectedRoutes(mode, scene);
    result.routeValid = validateStats(stats, mode, scene, 1, diagnostic);
    result.selectedMode = stats.mode;
    result.rootSamples = stats.rootSamples;
    result.actualRouteCount = expectedRouteCount(stats, expected);
    result.totalRouteCount = stats.noAa + stats.msaa4 + stats.flatDirect + stats.curveDirect + stats.flatMask + stats.curveMask;
    result.fallbackCount = stats.fallback;
    if (!aa_poc::writeFramebufferPng(filename, target.framebuffer(), targetSize,
                                     targetSize, downsample, diagnostic.c_str())) {
        return false;
    }
    std::printf("QUALITY\tscene=%s\tscale-target=%u\tmode=%s\toffset=%.3f,%.3f\tfile=%s\n",
                aa_poc::sceneName(scene), targetSize / downsample, modeName(mode),
                static_cast<double>(offset.x / downsample),
                static_cast<double>(offset.y / downsample), filename.c_str());
    return true;
}

bool runQuality(aa_poc::GlContext& context, const Options& options)
{
    if (!makeDirectory(options.outputDir)) return false;
    auto manifestFilename = joinPath(options.outputDir, "quality-manifest.tsv");
    std::ofstream manifest(manifestFilename, std::ios::trunc);
    if (!manifest) {
        std::fprintf(stderr, "aa_product_poc: cannot open %s\n", manifestFilename.c_str());
        return false;
    }
    manifest << "mode\tscene\tscale\toffset_x\toffset_y\tcandidate_png\treference_png"
                "\tselected_mode\troot_samples\troute_valid\texpected_route_count"
                "\tactual_route_count\ttotal_route_count\tfallback_count\n";
    manifest << std::fixed << std::setprecision(3);

    auto modes = selectedModes(options);
    auto scenes = selectedScenes(options);
    auto scales = selectedScales(options);
    auto offsets = selectedOffsets(options);

    for (auto scene : scenes) {
        for (const auto& scale : scales) {
            for (const auto& offset : offsets) {
                auto directory = qualityDirectory(options, scene, scale, offset);
                if (directory.empty()) return false;
                std::vector<QualityResult> candidateResults;
                std::vector<std::string> candidateFilenames;
                candidateResults.reserve(modes.size());
                candidateFilenames.reserve(modes.size());
                for (auto mode : modes) {
                    auto filename = joinPath(directory, std::string(modeName(mode)) + ".png");
                    auto diagnostic = std::string("aa_product_poc quality ") + aa_poc::sceneName(scene) + "/" + scale.name + "/" + modeName(mode);
                    QualityResult result;
                    if (!renderQualityImage(context, mode, scene, scale.targetSize,
                                            scale.coordinateScale, offset, 1, filename,
                                            diagnostic, result)) {
                        return false;
                    }
                    candidateResults.push_back(result);
                    candidateFilenames.push_back(filename);
                }

                constexpr uint32_t SSAA_SCALE = 8;
                auto ssaaSize = scale.targetSize * SSAA_SCALE;
                Offset ssaaOffset = {offset.x * SSAA_SCALE, offset.y * SSAA_SCALE};
                auto filename = joinPath(directory, "ssaa8.png");
                auto diagnostic = std::string("aa_product_poc quality ") + aa_poc::sceneName(scene) + "/" + scale.name + "/ssaa8";
                QualityResult referenceResult;
                if (!renderQualityImage(context, aa_poc::ProductAaMode::NoAa,
                                        scene, ssaaSize,
                                        scale.coordinateScale * SSAA_SCALE, ssaaOffset,
                                        SSAA_SCALE, filename, diagnostic,
                                        referenceResult)) {
                    return false;
                }
                if (!referenceResult.routeValid) return false;
                auto expected = aa_poc::expectedShapeCount(scene);
                for (size_t i = 0; i < modes.size(); ++i) {
                    manifest << modeName(modes[i]) << '\t' << aa_poc::sceneName(scene) << '\t'
                             << scale.name << '\t' << offset.x << '\t' << offset.y << '\t'
                             << manifestPath(options, candidateFilenames[i]) << '\t'
                             << manifestPath(options, filename) << '\t'
                             << modeName(candidateResults[i].selectedMode) << '\t'
                             << candidateResults[i].rootSamples << '\t'
                             << static_cast<unsigned>(candidateResults[i].routeValid) << '\t'
                             << expected << '\t'
                             << candidateResults[i].actualRouteCount << '\t'
                             << candidateResults[i].totalRouteCount << '\t'
                             << candidateResults[i].fallbackCount << '\n';
                }
                manifest.flush();
                if (!manifest) {
                    std::fprintf(stderr, "aa_product_poc: failed writing %s\n",
                                 manifestFilename.c_str());
                    return false;
                }
            }
        }
    }
    std::printf("wrote %s\n", manifestFilename.c_str());
    return true;
}

double median(std::vector<double> values)
{
    std::sort(values.begin(), values.end());
    auto middle = values.size() / 2;
    if (values.size() % 2) return values[middle];
    return (values[middle - 1] + values[middle]) * 0.5;
}

bool measureHeadlineRow(aa_poc::GlContext& context, const Options& options,
                        aa_poc::ProductAaMode mode, aa_poc::SceneKind scene,
                        const ScaleSpec& scale, const Offset& offset,
                        TimingResult& timing)
{
    auto diagnostic = std::string("aa_product_poc headline ") + aa_poc::sceneName(scene) + "/" + scale.name + "/" + modeName(mode);
    aa_poc::RenderTarget target;
    std::unique_ptr<GlCanvas> canvas;
    if (!prepareProductCanvas(context, target, canvas, mode, scene,
                              scale.targetSize, scale.coordinateScale,
                              offset.x, offset.y, diagnostic)) {
        return false;
    }

    // The first frame initializes common programs and all mode-specific resources.
    if (!submitFrame(*canvas, diagnostic)) return false;
    glFinish();
    for (uint32_t frame = 0; frame < options.warmup; ++frame) {
        if (!submitFrame(*canvas, diagnostic)) return false;
    }
    glFinish();

    aa_poc::resetProductAaStats(*canvas);
    timing.nsPerFrame.reserve(options.repetitions);
    for (uint32_t repetition = 0; repetition < options.repetitions; ++repetition) {
        glFinish();
        auto begin = Clock::now();
        for (uint32_t frame = 0; frame < options.frames; ++frame) {
            if (!submitFrame(*canvas, diagnostic)) return false;
        }
        glFinish();
        auto elapsed = std::chrono::duration<double, std::nano>(Clock::now() - begin).count();
        timing.nsPerFrame.push_back(elapsed / options.frames);
    }

    auto stats = aa_poc::productAaStats(*canvas);
    auto renderedFrames = static_cast<uint64_t>(options.frames) * options.repetitions;
    auto expected = expectedRoutes(mode, scene);
    timing.routeValid = validateStats(stats, mode, scene, renderedFrames,
                                      diagnostic);
    timing.rootSamples = stats.rootSamples;
    timing.actualRouteCount = expectedRouteCount(stats, expected);
    timing.fallbackCount = stats.fallback;
    timing.medianNsPerFrame = median(timing.nsPerFrame);
    return true;
}

bool runHeadline(aa_poc::GlContext& context, const Options& options)
{
    if (!makeDirectory(options.outputDir)) return false;
    auto filename = joinPath(options.outputDir, "headline.tsv");
    std::ofstream output(filename, std::ios::trunc);
    if (!output) {
        std::fprintf(stderr, "aa_product_poc: cannot open %s\n", filename.c_str());
        return false;
    }
    output << "scene\tscale\ttarget\tmode\toffset_x\toffset_y\troot_samples\tshapes"
              "\troute_valid\texpected_route_count\tactual_route_count\tfallback_count"
              "\twarmup\tframes\trepetition\tns_per_frame\tmedian_ns_per_frame\tvsync\n";
    output << std::fixed << std::setprecision(3);

    auto modes = selectedModes(options);
    auto scenes = selectedScenes(options);
    auto scales = selectedScales(options);
    auto offsets = selectedOffsets(options);
    auto offset = offsets.front();
    for (auto scene : scenes) {
        for (const auto& scale : scales) {
            for (auto mode : modes) {
                TimingResult timing;
                if (!measureHeadlineRow(context, options, mode, scene, scale, offset,
                                        timing)) {
                    return false;
                }
                for (uint32_t repetition = 0; repetition < options.repetitions; ++repetition) {
                    output << aa_poc::sceneName(scene) << '\t' << scale.name << '\t'
                           << scale.targetSize << '\t' << modeName(mode) << '\t'
                           << offset.x << '\t' << offset.y << '\t' << timing.rootSamples << '\t'
                           << aa_poc::expectedShapeCount(scene) << '\t'
                           << static_cast<unsigned>(timing.routeValid) << '\t'
                           << static_cast<uint64_t>(aa_poc::expectedShapeCount(scene)) * options.frames * options.repetitions << '\t'
                           << timing.actualRouteCount << '\t' << timing.fallbackCount << '\t'
                           << options.warmup << '\t'
                           << options.frames << '\t' << repetition + 1 << '\t'
                           << timing.nsPerFrame[repetition] << '\t' << timing.medianNsPerFrame
                           << "\toffscreen-no-swap\n";
                }
                output.flush();
                if (!output) {
                    std::fprintf(stderr, "aa_product_poc: failed writing %s\n", filename.c_str());
                    return false;
                }
                std::printf(
                    "HEADLINE\tscene=%s\tscale=%s\ttarget=%u\tmode=%s\troot-samples=%u\t"
                    "route-valid=%u\tmedian-ns/frame=%.3f\tvsync=offscreen-no-swap\n",
                    aa_poc::sceneName(scene), scale.name, scale.targetSize, modeName(mode),
                    timing.rootSamples, static_cast<unsigned>(timing.routeValid),
                    timing.medianNsPerFrame);
            }
        }
    }
    std::printf("wrote %s\n", filename.c_str());
    return true;
}

}  // namespace

int main(int argc, char** argv)
{
    Options options;
    if (!parseOptions(argc, argv, options)) {
        printUsage(argv[0]);
        return EXIT_FAILURE;
    }

    aa_poc::GlContext context;
    if (!context.init()) {
        std::fprintf(stderr, "aa_product_poc: failed to create an offscreen GL context\n");
        return EXIT_FAILURE;
    }
    aa_poc::printGlInfo();
    std::printf("vsync=offscreen-no-swap\n");

    if (Initializer::init() != Result::Success) {
        std::fprintf(stderr, "aa_product_poc: ThorVG initialization failed\n");
        return EXIT_FAILURE;
    }

    if (!validateHybridSelfIntersectionRoute(context)) {
        Initializer::term();
        return EXIT_FAILURE;
    }
    if (!validateSingleSampleFallbackTarget(context)) {
        Initializer::term();
        return EXIT_FAILURE;
    }

    auto success = options.suite == Suite::Quality ? runQuality(context, options) : runHeadline(context, options);

    if (Initializer::term() != Result::Success) {
        std::fprintf(stderr, "aa_product_poc: ThorVG termination failed\n");
        success = false;
    }
    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
