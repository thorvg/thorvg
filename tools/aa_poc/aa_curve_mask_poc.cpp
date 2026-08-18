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

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>

#include <thorvg.h>
#include "aa_poc_cli.h"
#include "aa_poc_comparison_scene.h"
#include "aa_poc_curve_mask_scene.h"
#include "aa_poc_gl.h"

using namespace tvg;

namespace
{

constexpr uint32_t NATIVE_WIDTH = aa_poc::CURVE_MASK_WIDTH;
constexpr uint32_t NATIVE_HEIGHT = aa_poc::CURVE_MASK_HEIGHT;
constexpr uint8_t CURVE_MASK_ENGINE_OPTION = 0x40;

bool renderCurveMask(aa_poc::GlContext& context, const std::string& outputDir,
                     float offsetX, float offsetY, bool comparison)
{
    auto width = comparison ? aa_poc::COMPARISON_WIDTH : NATIVE_WIDTH;
    auto height = comparison ? aa_poc::COMPARISON_HEIGHT : NATIVE_HEIGHT;
    aa_poc::RenderTarget renderTarget;
    if (!renderTarget.init(width, height, 1, "aa_curve_mask_poc")) return false;

    auto canvas = std::unique_ptr<GlCanvas>(
        GlCanvas::gen(static_cast<EngineOption>(CURVE_MASK_ENGINE_OPTION)));
    if (!canvas) {
        std::fprintf(stderr, "aa_curve_mask_poc: GlCanvas::gen() failed\n");
        return false;
    }

    auto success = aa_poc::checkCurveMaskThorvg(
        context.target(*canvas, renderTarget.framebuffer(), width, height,
                       ColorSpace::ABGR8888S),
        "GlCanvas::target", "aa_curve_mask_poc");
    if (success) {
        success = aa_poc::checkCurveMaskThorvg(
            canvas->viewport(comparison ? 0 : 8, comparison ? 0 : 8,
                             static_cast<int32_t>(width) - (comparison ? 0 : 16),
                             static_cast<int32_t>(height) - (comparison ? 0 : 16)),
            "Canvas::viewport", "aa_curve_mask_poc");
    }
    if (success) {
        success = comparison ?
            aa_poc::populateComparisonScene(
                *canvas, offsetX, offsetY, 1.0f, "aa_curve_mask_poc") :
            aa_poc::populateCurveMaskScene(
                *canvas, offsetX, offsetY, 1.0f, "aa_curve_mask_poc");
    }
    if (success) {
        success = aa_poc::checkCurveMaskThorvg(
            canvas->update(), "Canvas::update", "aa_curve_mask_poc");
    }
    if (success) {
        aa_poc::clearFramebuffer(renderTarget.framebuffer(), width, height);
        success = aa_poc::checkCurveMaskThorvg(
            canvas->draw(false), "Canvas::draw", "aa_curve_mask_poc");
    }
    if (success) {
        success = aa_poc::checkCurveMaskThorvg(
            canvas->sync(), "Canvas::sync", "aa_curve_mask_poc");
    }
    canvas.reset();

    auto filename = outputDir + "/curve-mask.png";
    if (success) {
        success = aa_poc::writeFramebufferPng(
            filename, renderTarget.framebuffer(), width, height, 1,
            "aa_curve_mask_poc");
    }
    if (success) std::printf("wrote %s\n", filename.c_str());
    return success;
}

} // namespace

int main(int argc, char** argv)
{
    auto comparison = aa_poc::takeComparisonOption(argc, argv);
    aa_poc::RunOptions options;
    options.outputDir = "aa_curve_mask_poc-output";
    if (!aa_poc::parseOptions(argc, argv, options)) {
        aa_poc::printUsage(argv[0], "[--comparison]");
        return EXIT_FAILURE;
    }
    if (!aa_poc::makeOutputDirectory(options.outputDir)) {
        std::fprintf(stderr, "aa_curve_mask_poc: cannot create output directory: %s\n",
                     options.outputDir.c_str());
        return EXIT_FAILURE;
    }

    aa_poc::GlContext context;
    if (!context.init()) {
        std::fprintf(stderr, "aa_curve_mask_poc: failed to create an offscreen GL context\n");
        return EXIT_FAILURE;
    }

    aa_poc::printGlInfo();
    std::printf("verdict scope: solid fills only; sharp joins, patch overlap, self-intersections, and curve-sign failures remain visible POC results\n");

    if (Initializer::init() != Result::Success) {
        std::fprintf(stderr, "aa_curve_mask_poc: ThorVG initialization failed\n");
        return EXIT_FAILURE;
    }

    auto renderAt = [&](float offsetX, float offsetY, const std::string& outputDir) {
        return renderCurveMask(context, outputDir, offsetX, offsetY, comparison);
    };

    auto success = aa_poc::renderOffsets(options, renderAt);

    if (Initializer::term() != Result::Success) {
        std::fprintf(stderr, "aa_curve_mask_poc: ThorVG termination failed\n");
        success = false;
    }

    if (success) {
        if (comparison) {
            std::printf("scene: shared comparison\n");
        } else {
            std::printf("scene: circle | high curvature | inflected S | connected curves | sharp join | concave curve | NonZero hole | EvenOdd hole | 50%% opacity | straight line\n");
        }
        std::printf("subpixel offset: %.3f, %.3f\n",
                    static_cast<double>(options.offsetX),
                    static_cast<double>(options.offsetY));
        return EXIT_SUCCESS;
    }
    return EXIT_FAILURE;
}
