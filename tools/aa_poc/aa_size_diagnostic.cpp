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

constexpr uint32_t TARGET_SIZE = 64;
constexpr float CENTER_X = 32.375f;
constexpr float CENTER_Y = 32.625f;

struct ModeSpec
{
    aa_poc::ProductAaMode mode;
    const char* name;
};

constexpr ModeSpec MODES[] = {
    {aa_poc::ProductAaMode::FlatDirect, "flat-direct"},
    {aa_poc::ProductAaMode::FlatMask, "flat-mask"},
    {aa_poc::ProductAaMode::CurveDirect, "curve-direct"},
};

struct Options
{
    std::string outputDir = "aa_size_diagnostic-output";
    uint32_t warmup = 20;
    uint32_t frames = 100;
    uint32_t repetitions = 3;
};

struct PaintReleaser
{
    void operator()(Paint* paint) const { Paint::rel(paint); }
};

using ShapePtr = std::unique_ptr<Shape, PaintReleaser>;

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
            std::fprintf(stderr, "aa_size_diagnostic: missing value for %s\n", argument);
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
            std::fprintf(stderr, "aa_size_diagnostic: unknown option: %s\n", argument);
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

bool addCurvedTile(GlCanvas& canvas, float scale, const std::string& diagnostic)
{
    ShapePtr shape(Shape::gen());
    if (!shape) {
        std::fprintf(stderr, "%s: Shape::gen() failed\n", diagnostic.c_str());
        return false;
    }

    auto x = [=](float local) {
        return CENTER_X + local * scale;
    };
    auto y = [=](float local) {
        return CENTER_Y + local * scale;
    };
    if (!checkResult(shape->moveTo(x(-28.0f), y(0.0f)), "Shape::moveTo", diagnostic) || !checkResult(shape->cubicTo(x(-27.0f), y(-13.0f), x(-15.0f), y(-18.0f), x(0.0f), y(-16.0f)), "Shape::cubicTo", diagnostic) || !checkResult(shape->cubicTo(x(17.0f), y(-18.0f), x(28.0f), y(-10.0f), x(27.0f), y(2.0f)), "Shape::cubicTo", diagnostic) || !checkResult(shape->cubicTo(x(27.0f), y(13.0f), x(14.0f), y(18.0f), x(-2.0f), y(16.0f)), "Shape::cubicTo", diagnostic) || !checkResult(shape->cubicTo(x(-17.0f), y(18.0f), x(-29.0f), y(11.0f), x(-28.0f), y(0.0f)), "Shape::cubicTo", diagnostic) || !checkResult(shape->close(), "Shape::close", diagnostic) || !checkResult(shape->fill(242, 122, 15, 255), "Shape::fill", diagnostic) || !checkResult(canvas.add(shape.get()), "Canvas::add", diagnostic)) {
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
             aa_poc::ProductAaMode mode, float scale, TimingResult& timing)
{
    auto diagnostic = std::string("aa_size_diagnostic scale=") + std::to_string(scale) + " mode=" + modeName(mode);
    aa_poc::RenderTarget target;
    if (!target.init(TARGET_SIZE, TARGET_SIZE, 1, diagnostic.c_str())) return false;

    std::unique_ptr<GlCanvas> canvas(GlCanvas::gen(EngineOption::Default));
    if (!canvas) {
        std::fprintf(stderr, "%s: GlCanvas::gen() failed\n", diagnostic.c_str());
        return false;
    }
    if (!aa_poc::setProductAaMode(*canvas, mode)) {
        std::fprintf(stderr, "%s: AA mode must be selected before target creation\n",
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
    if (!addCurvedTile(*canvas, scale, diagnostic)) return false;

    // Creation, program initialization, and warmup are deliberately outside
    // the timed steady-state update/draw/sync batches.
    if (!submitFrame(*canvas, diagnostic)) return false;
    glFinish();
    for (uint32_t frame = 0; frame < options.warmup; ++frame) {
        if (!submitFrame(*canvas, diagnostic)) return false;
    }
    glFinish();

    aa_poc::resetProductAaStats(*canvas);
    std::vector<double> samples;
    samples.reserve(options.repetitions);
    for (uint32_t repetition = 0; repetition < options.repetitions; ++repetition) {
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
    timing.routeValid = timing.stats.mode == mode && timing.stats.rootSamples == 1 && timing.stats.fallback == 0 && timing.actualRouteCount == timing.requestedRouteCount && totalRouteCount(timing.stats) == timing.requestedRouteCount;
    timing.medianNsPerFrame = median(samples);

    if (!timing.routeValid) {
        std::fprintf(
            stderr,
            "%s: route assertion failed: selected=%s root=%u requested=%llu "
            "actual=%llu total=%llu fallback=%llu\n",
            diagnostic.c_str(), modeName(timing.stats.mode), timing.stats.rootSamples,
            static_cast<unsigned long long>(timing.requestedRouteCount),
            static_cast<unsigned long long>(timing.actualRouteCount),
            static_cast<unsigned long long>(totalRouteCount(timing.stats)),
            static_cast<unsigned long long>(timing.stats.fallback));
    }
    return true;
}

std::vector<float> scales()
{
    std::vector<float> result;
    for (uint32_t milli = 100; milli <= 350; milli += 5) {
        result.push_back(static_cast<float>(milli) / 1000.0f);
    }
    result.push_back(0.5f);
    result.push_back(1.0f);
    return result;
}

bool run(aa_poc::GlContext& context, const Options& options)
{
    if (!aa_poc::makeOutputDirectory(options.outputDir)) {
        std::fprintf(stderr, "aa_size_diagnostic: cannot create output directory: %s\n",
                     options.outputDir.c_str());
        return false;
    }
    auto filename = options.outputDir + "/size-diagnostic.tsv";
    std::ofstream output(filename, std::ios::trunc);
    if (!output) {
        std::fprintf(stderr, "aa_size_diagnostic: cannot open %s\n", filename.c_str());
        return false;
    }
    output << "coordinate_scale\ttarget\tcenter_x\tcenter_y\trequested_mode"
              "\tselected_mode\troot_samples\trequested_route_count"
              "\tactual_route_count\tfallback_count\troute_valid\twarmup\tframes"
              "\trepetitions\tmedian_ns_per_frame\tvsync\n";
    output << std::fixed << std::setprecision(3);

    auto allRoutesValid = true;
    for (auto scale : scales()) {
        for (const auto& spec : MODES) {
            TimingResult timing;
            if (!measure(context, options, spec.mode, scale, timing)) return false;
            output << scale << '\t' << TARGET_SIZE << '\t' << CENTER_X << '\t'
                   << CENTER_Y << '\t' << spec.name << '\t'
                   << modeName(timing.stats.mode) << '\t' << timing.stats.rootSamples << '\t'
                   << timing.requestedRouteCount << '\t' << timing.actualRouteCount << '\t'
                   << timing.stats.fallback << '\t'
                   << static_cast<unsigned>(timing.routeValid) << '\t' << options.warmup
                   << '\t' << options.frames << '\t' << options.repetitions << '\t'
                   << timing.medianNsPerFrame << "\toffscreen-no-swap\n";
            output.flush();
            if (!output) {
                std::fprintf(stderr, "aa_size_diagnostic: failed writing %s\n",
                             filename.c_str());
                return false;
            }
            std::printf(
                "SIZE\tscale=%.3f\tmode=%s\tselected=%s\troot-samples=%u\t"
                "route-valid=%u\tmedian-ns/frame=%.3f\tvsync=offscreen-no-swap\n",
                static_cast<double>(scale), spec.name, modeName(timing.stats.mode),
                timing.stats.rootSamples, static_cast<unsigned>(timing.routeValid),
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
        std::fprintf(stderr, "aa_size_diagnostic: failed to create an offscreen GL context\n");
        return EXIT_FAILURE;
    }
    aa_poc::printGlInfo();
    std::printf("vsync=offscreen-no-swap\n");

    if (Initializer::init() != Result::Success) {
        std::fprintf(stderr, "aa_size_diagnostic: ThorVG initialization failed\n");
        return EXIT_FAILURE;
    }

    auto success = run(context, options);
    if (Initializer::term() != Result::Success) {
        std::fprintf(stderr, "aa_size_diagnostic: ThorVG termination failed\n");
        success = false;
    }
    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
