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
#include <string>
#include <vector>

#include <thorvg.h>

#include "aa_poc_cli.h"
#include "aa_poc_gl.h"
#include "aa_product_renderer.h"

using namespace tvg;

namespace
{

using Clock = std::chrono::steady_clock;

constexpr uint32_t TARGET_SIZE = 1024;
constexpr float CENTER_X = 512.375f;
constexpr float CENTER_Y = 512.625f;
constexpr float RADIUS = 400.0f;
constexpr float CIRCLE_KAPPA = 0.552284749831f;
// Mirrors the experimental renderer's GL_FLAT_MASK_AA_RADIUS without including
// its private GL loader headers in this native CGL runner.
constexpr float PATCH_EXPANSION = 0.5f;
constexpr std::array<uint32_t, 6> SUBDIVISIONS = {{1, 2, 4, 8, 16, 32}};

struct Point
{
    float x;
    float y;
};

struct Cubic
{
    Point p0;
    Point p1;
    Point p2;
    Point p3;
};

struct ModeSpec
{
    aa_poc::ProductAaMode mode;
    const char* name;
};

constexpr ModeSpec MODES[] = {
    {aa_poc::ProductAaMode::Msaa4, "msaa4"},
    {aa_poc::ProductAaMode::CurveDirect, "curve-direct"},
    {aa_poc::ProductAaMode::CurveMask, "curve-mask"},
};

struct Options
{
    std::string outputDir = "aa_aabb_diagnostic-output";
    uint32_t warmup = 20;
    uint32_t frames = 100;
    uint32_t repetitions = 3;
};

struct PaintReleaser
{
    void operator()(Paint* paint) const { Paint::rel(paint); }
};

using ShapePtr = std::unique_ptr<Shape, PaintReleaser>;

struct PatchMetrics
{
    uint32_t patchCount = 0;
    double controlAabbArea = 0.0;
    double expandedClippedAabbArea = 0.0;
};

struct TimingResult
{
    aa_poc::ProductAaStats stats;
    uint64_t requestedRouteCount = 0;
    uint64_t actualRouteCount = 0;
    bool routeValid = false;
    double medianNsPerFrame = 0.0;
};

const char* modeName(aa_poc::ProductAaMode mode)
{
    switch (mode) {
        case aa_poc::ProductAaMode::NoAa: return "noaa";
        case aa_poc::ProductAaMode::Msaa4: return "msaa4";
        case aa_poc::ProductAaMode::FlatDirect: return "flat-direct";
        case aa_poc::ProductAaMode::CurveDirect: return "curve-direct";
        case aa_poc::ProductAaMode::FlatMask: return "flat-mask";
        case aa_poc::ProductAaMode::CurveMask: return "curve-mask";
        case aa_poc::ProductAaMode::Hybrid: return "hybrid";
    }
    return "unknown";
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
        "  --output-dir DIR\n"
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
            std::fprintf(stderr, "aa_aabb_diagnostic: missing value for %s\n",
                         argument);
            return false;
        }
        auto value = argv[++index];
        if (std::strcmp(argument, "--output-dir") == 0) {
            if (!*value) return false;
            options.outputDir = value;
        } else if (std::strcmp(argument, "--warmup") == 0) {
            if (!parseUnsigned(value, options.warmup, true)) return false;
        } else if (std::strcmp(argument, "--frames") == 0) {
            if (!parseUnsigned(value, options.frames, false)) return false;
        } else if (std::strcmp(argument, "--repetitions") == 0) {
            if (!parseUnsigned(value, options.repetitions, false)) return false;
        } else {
            std::fprintf(stderr, "aa_aabb_diagnostic: unknown option: %s\n",
                         argument);
            return false;
        }
    }
    return true;
}

bool checkResult(Result result, const char* operation,
                 const std::string& diagnostic)
{
    if (result == Result::Success) return true;
    std::fprintf(stderr, "%s: %s failed (Result=%u)\n", diagnostic.c_str(),
                 operation, static_cast<unsigned>(result));
    return false;
}

Point midpoint(const Point& lhs, const Point& rhs)
{
    return {(lhs.x + rhs.x) * 0.5f, (lhs.y + rhs.y) * 0.5f};
}

std::array<Cubic, 2> splitHalf(const Cubic& curve)
{
    auto p01 = midpoint(curve.p0, curve.p1);
    auto p12 = midpoint(curve.p1, curve.p2);
    auto p23 = midpoint(curve.p2, curve.p3);
    auto p012 = midpoint(p01, p12);
    auto p123 = midpoint(p12, p23);
    auto split = midpoint(p012, p123);
    return {{{curve.p0, p01, p012, split},
             {split, p123, p23, curve.p3}}};
}

void subdivide(const Cubic& curve, uint32_t pieces, std::vector<Cubic>& output)
{
    if (pieces == 1) {
        output.push_back(curve);
        return;
    }
    auto halves = splitHalf(curve);
    subdivide(halves[0], pieces / 2, output);
    subdivide(halves[1], pieces / 2, output);
}

std::vector<Cubic> silhouette(uint32_t subdivisions)
{
    auto tangent = RADIUS * CIRCLE_KAPPA;
    const std::array<Cubic, 4> base = {{
        {{CENTER_X + RADIUS, CENTER_Y},
         {CENTER_X + RADIUS, CENTER_Y + tangent},
         {CENTER_X + tangent, CENTER_Y + RADIUS},
         {CENTER_X, CENTER_Y + RADIUS}},
        {{CENTER_X, CENTER_Y + RADIUS},
         {CENTER_X - tangent, CENTER_Y + RADIUS},
         {CENTER_X - RADIUS, CENTER_Y + tangent},
         {CENTER_X - RADIUS, CENTER_Y}},
        {{CENTER_X - RADIUS, CENTER_Y},
         {CENTER_X - RADIUS, CENTER_Y - tangent},
         {CENTER_X - tangent, CENTER_Y - RADIUS},
         {CENTER_X, CENTER_Y - RADIUS}},
        {{CENTER_X, CENTER_Y - RADIUS},
         {CENTER_X + tangent, CENTER_Y - RADIUS},
         {CENTER_X + RADIUS, CENTER_Y - tangent},
         {CENTER_X + RADIUS, CENTER_Y}},
    }};

    std::vector<Cubic> result;
    result.reserve(base.size() * subdivisions);
    for (const auto& curve : base)
        subdivide(curve, subdivisions, result);
    return result;
}

PatchMetrics measurePatches(const std::vector<Cubic>& curves)
{
    PatchMetrics metrics;
    metrics.patchCount = static_cast<uint32_t>(curves.size());
    const auto regionExpansion = std::ceil(PATCH_EXPANSION);
    const auto regionMinX = std::max(
        0.0f, std::floor(CENTER_X - RADIUS) - regionExpansion);
    const auto regionMinY = std::max(
        0.0f, std::floor(CENTER_Y - RADIUS) - regionExpansion);
    const auto regionMaxX = std::min(
        static_cast<float>(TARGET_SIZE),
        std::ceil(CENTER_X + RADIUS) + regionExpansion);
    const auto regionMaxY = std::min(
        static_cast<float>(TARGET_SIZE),
        std::ceil(CENTER_Y + RADIUS) + regionExpansion);

    for (const auto& curve : curves) {
        auto minX = std::min(std::min(curve.p0.x, curve.p1.x),
                             std::min(curve.p2.x, curve.p3.x));
        auto minY = std::min(std::min(curve.p0.y, curve.p1.y),
                             std::min(curve.p2.y, curve.p3.y));
        auto maxX = std::max(std::max(curve.p0.x, curve.p1.x),
                             std::max(curve.p2.x, curve.p3.x));
        auto maxY = std::max(std::max(curve.p0.y, curve.p1.y),
                             std::max(curve.p2.y, curve.p3.y));
        metrics.controlAabbArea += static_cast<double>(maxX - minX) * static_cast<double>(maxY - minY);

        minX = std::max(0.0f, std::max(regionMinX, minX - PATCH_EXPANSION));
        minY = std::max(0.0f, std::max(regionMinY, minY - PATCH_EXPANSION));
        maxX = std::min(static_cast<float>(TARGET_SIZE),
                        std::min(regionMaxX, maxX + PATCH_EXPANSION));
        maxY = std::min(static_cast<float>(TARGET_SIZE),
                        std::min(regionMaxY, maxY + PATCH_EXPANSION));
        if (minX < maxX && minY < maxY) {
            metrics.expandedClippedAabbArea += static_cast<double>(maxX - minX) * static_cast<double>(maxY - minY);
        }
    }
    return metrics;
}

bool addSilhouette(GlCanvas& canvas, const std::vector<Cubic>& curves,
                   const std::string& diagnostic)
{
    ShapePtr shape(Shape::gen());
    if (!shape) {
        std::fprintf(stderr, "%s: Shape::gen() failed\n", diagnostic.c_str());
        return false;
    }
    if (!checkResult(shape->moveTo(curves.front().p0.x, curves.front().p0.y),
                     "Shape::moveTo", diagnostic)) {
        return false;
    }
    for (const auto& curve : curves) {
        if (!checkResult(shape->cubicTo(curve.p1.x, curve.p1.y, curve.p2.x,
                                        curve.p2.y, curve.p3.x, curve.p3.y),
                         "Shape::cubicTo", diagnostic)) {
            return false;
        }
    }
    if (!checkResult(shape->close(), "Shape::close", diagnostic) || !checkResult(shape->fill(242, 122, 15, 255), "Shape::fill", diagnostic) || !checkResult(canvas.add(shape.get()), "Canvas::add", diagnostic)) {
        return false;
    }
    shape.release();
    return true;
}

bool submitFrame(GlCanvas& canvas, const std::string& diagnostic)
{
    return checkResult(canvas.update(), "Canvas::update", diagnostic) && checkResult(canvas.draw(true), "Canvas::draw", diagnostic) && checkResult(canvas.sync(), "Canvas::sync", diagnostic);
}

uint64_t routeCount(const aa_poc::ProductAaStats& stats,
                    aa_poc::ProductAaMode mode)
{
    switch (mode) {
        case aa_poc::ProductAaMode::NoAa: return stats.noAa;
        case aa_poc::ProductAaMode::Msaa4: return stats.msaa4;
        case aa_poc::ProductAaMode::FlatDirect: return stats.flatDirect;
        case aa_poc::ProductAaMode::CurveDirect: return stats.curveDirect;
        case aa_poc::ProductAaMode::FlatMask: return stats.flatMask;
        case aa_poc::ProductAaMode::CurveMask: return stats.curveMask;
        case aa_poc::ProductAaMode::Hybrid: break;
    }
    return 0;
}

uint64_t totalRouteCount(const aa_poc::ProductAaStats& stats)
{
    return stats.noAa + stats.msaa4 + stats.flatDirect + stats.curveDirect + stats.flatMask + stats.curveMask;
}

double median(std::vector<double> values)
{
    std::sort(values.begin(), values.end());
    auto middle = values.size() / 2;
    if (values.size() % 2) return values[middle];
    return (values[middle - 1] + values[middle]) * 0.5;
}

bool measure(aa_poc::GlContext& context, const Options& options,
             aa_poc::ProductAaMode mode, uint32_t subdivisions,
             const std::vector<Cubic>& curves, TimingResult& timing)
{
    auto diagnostic = std::string("aa_aabb_diagnostic subdivisions=") + std::to_string(subdivisions) + " mode=" + modeName(mode);
    aa_poc::RenderTarget target;
    if (!target.init(TARGET_SIZE, TARGET_SIZE, 1, diagnostic.c_str())) return false;

    std::unique_ptr<GlCanvas> canvas(GlCanvas::gen(EngineOption::Default));
    if (!canvas) {
        std::fprintf(stderr, "%s: GlCanvas::gen() failed\n", diagnostic.c_str());
        return false;
    }
    if (!aa_poc::setProductAaMode(*canvas, mode)) {
        std::fprintf(stderr,
                     "%s: AA mode must be selected before target creation\n",
                     diagnostic.c_str());
        return false;
    }
    if (!checkResult(context.target(*canvas, target.framebuffer(), TARGET_SIZE,
                                    TARGET_SIZE, ColorSpace::ABGR8888S),
                     "GlCanvas::target", diagnostic)) {
        return false;
    }
    if (aa_poc::setProductAaMode(*canvas, mode)) {
        std::fprintf(stderr, "%s: AA selector accepted a post-target change\n",
                     diagnostic.c_str());
        return false;
    }
    if (!addSilhouette(*canvas, curves, diagnostic)) return false;

    // Creation, program initialization, and warmup are outside the timed
    // steady-state update/draw/sync batches. This diagnostic performs no
    // readback, encoding, or report generation in the measured interval.
    if (!submitFrame(*canvas, diagnostic)) return false;
    glFinish();
    for (uint32_t frame = 0; frame < options.warmup; ++frame) {
        if (!submitFrame(*canvas, diagnostic)) return false;
    }
    glFinish();

    aa_poc::resetProductAaStats(*canvas);
    std::vector<double> samples;
    samples.reserve(options.repetitions);
    for (uint32_t repetition = 0; repetition < options.repetitions;
         ++repetition) {
        glFinish();
        auto begin = Clock::now();
        for (uint32_t frame = 0; frame < options.frames; ++frame) {
            if (!submitFrame(*canvas, diagnostic)) return false;
        }
        glFinish();
        auto elapsed = std::chrono::duration<double, std::nano>(Clock::now() - begin).count();
        samples.push_back(elapsed / options.frames);
    }

    timing.stats = aa_poc::productAaStats(*canvas);
    timing.requestedRouteCount = static_cast<uint64_t>(options.frames) * options.repetitions;
    timing.actualRouteCount = routeCount(timing.stats, mode);
    auto expectedSamples = mode == aa_poc::ProductAaMode::Msaa4 ? 4u : 1u;
    timing.routeValid = timing.stats.mode == mode && timing.stats.rootSamples == expectedSamples && timing.stats.fallback == 0 && timing.actualRouteCount == timing.requestedRouteCount && totalRouteCount(timing.stats) == timing.requestedRouteCount;
    timing.medianNsPerFrame = median(samples);

    if (!timing.routeValid) {
        std::fprintf(
            stderr,
            "%s: route assertion failed: selected=%s root=%u requested=%llu "
            "actual=%llu total=%llu fallback=%llu\n",
            diagnostic.c_str(), modeName(timing.stats.mode),
            timing.stats.rootSamples,
            static_cast<unsigned long long>(timing.requestedRouteCount),
            static_cast<unsigned long long>(timing.actualRouteCount),
            static_cast<unsigned long long>(totalRouteCount(timing.stats)),
            static_cast<unsigned long long>(timing.stats.fallback));
    }
    return true;
}

bool run(aa_poc::GlContext& context, const Options& options)
{
    if (!aa_poc::makeOutputDirectory(options.outputDir)) {
        std::fprintf(stderr,
                     "aa_aabb_diagnostic: cannot create output directory: %s\n",
                     options.outputDir.c_str());
        return false;
    }
    auto filename = options.outputDir + "/aabb-diagnostic.tsv";
    std::ofstream output(filename, std::ios::trunc);
    if (!output) {
        std::fprintf(stderr, "aa_aabb_diagnostic: cannot open %s\n",
                     filename.c_str());
        return false;
    }
    output
        << "silhouette\ttarget\tdiameter_px\tsubdivisions_per_cubic"
           "\tcubic_patch_count\tcontrol_aabb_area_sum_px2"
           "\texpanded_clipped_aabb_area_sum_px2\tfill_region_area_px2"
           "\texpanded_aabb_to_fill_region_ratio\trequested_mode\tselected_mode"
           "\troot_samples\trequested_route_count\tactual_route_count"
           "\tfallback_count\troute_valid\twarmup\tframes\trepetitions"
           "\tmedian_ns_per_frame\tvsync\n";
    output << std::fixed << std::setprecision(6);

    const auto regionExpansion = std::ceil(PATCH_EXPANSION);
    const auto fillRegionWidth = std::ceil(CENTER_X + RADIUS) - std::floor(CENTER_X - RADIUS) + 2.0f * regionExpansion;
    const auto fillRegionHeight = std::ceil(CENTER_Y + RADIUS) - std::floor(CENTER_Y - RADIUS) + 2.0f * regionExpansion;
    const auto fillRegionArea = static_cast<double>(fillRegionWidth) * static_cast<double>(fillRegionHeight);
    auto allRoutesValid = true;
    for (auto subdivisions : SUBDIVISIONS) {
        auto curves = silhouette(subdivisions);
        auto patchMetrics = measurePatches(curves);
        auto aabbRatio = patchMetrics.expandedClippedAabbArea / fillRegionArea;
        for (const auto& spec : MODES) {
            TimingResult timing;
            if (!measure(context, options, spec.mode, subdivisions, curves,
                         timing)) {
                return false;
            }
            output << "four-cubic-circle\t" << TARGET_SIZE << '\t'
                   << 2.0f * RADIUS << '\t' << subdivisions << '\t'
                   << patchMetrics.patchCount << '\t'
                   << patchMetrics.controlAabbArea << '\t'
                   << patchMetrics.expandedClippedAabbArea << '\t'
                   << fillRegionArea << '\t' << aabbRatio << '\t' << spec.name
                   << '\t' << modeName(timing.stats.mode) << '\t'
                   << timing.stats.rootSamples << '\t'
                   << timing.requestedRouteCount << '\t'
                   << timing.actualRouteCount << '\t' << timing.stats.fallback
                   << '\t' << static_cast<unsigned>(timing.routeValid) << '\t'
                   << options.warmup << '\t' << options.frames << '\t'
                   << options.repetitions << '\t' << timing.medianNsPerFrame
                   << "\toffscreen-no-swap\n";
            output.flush();
            if (!output) {
                std::fprintf(stderr,
                             "aa_aabb_diagnostic: failed writing %s\n",
                             filename.c_str());
                return false;
            }
            std::printf(
                "AABB\tsubdivisions=%u\tpatches=%u\taabb-ratio=%.6f\t"
                "mode=%s\tselected=%s\troot-samples=%u\troute-valid=%u\t"
                "median-ns/frame=%.3f\tvsync=offscreen-no-swap\n",
                subdivisions, patchMetrics.patchCount, aabbRatio, spec.name,
                modeName(timing.stats.mode), timing.stats.rootSamples,
                static_cast<unsigned>(timing.routeValid),
                timing.medianNsPerFrame);
            allRoutesValid = timing.routeValid && allRoutesValid;
        }
    }
    std::printf("wrote %s\n", filename.c_str());
    return allRoutesValid;
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
        std::fprintf(stderr,
                     "aa_aabb_diagnostic: failed to create an offscreen GL context\n");
        return EXIT_FAILURE;
    }
    aa_poc::printGlInfo();
    std::printf("vsync=offscreen-no-swap\n");

    if (Initializer::init() != Result::Success) {
        std::fprintf(stderr,
                     "aa_aabb_diagnostic: ThorVG initialization failed\n");
        return EXIT_FAILURE;
    }

    auto success = run(context, options);
    if (Initializer::term() != Result::Success) {
        std::fprintf(stderr,
                     "aa_aabb_diagnostic: ThorVG termination failed\n");
        success = false;
    }
    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
