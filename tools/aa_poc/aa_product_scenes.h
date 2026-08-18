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

#ifndef THORVG_TOOLS_AA_POC_PRODUCT_SCENES_H
#define THORVG_TOOLS_AA_POC_PRODUCT_SCENES_H

#include <cstdint>

#include <thorvg.h>

namespace aa_poc
{

// The product proxies are authored in a normalized 256 x 256 component space.
// The test plan does not prescribe exact pixel dimensions, so the harness uses
// coordinate scales 0.25, 1, and 4 for 64 x 64, 256 x 256, and 1024 x 1024
// targets respectively. Offsets are target-pixel values applied after scaling;
// an SSAA renderer should scale both coordinateScale and the offsets.
constexpr uint32_t PRODUCT_SCENE_SIZE = 256;

enum class SceneKind
{
    FlatCore,
    CurveCore,
    MixedProductTile,
    TransparencyCore,
};

const char* sceneName(SceneKind scene);
bool parseScene(const char* name, SceneKind& scene);
uint32_t expectedShapeCount(SceneKind scene);

// Adds only normal-blended solid fills. The fixtures intentionally contain no
// strokes, gradients, clips, masks, or paint-to-paint overlap.
bool populateProductScene(tvg::GlCanvas& canvas, SceneKind scene,
                          float coordinateScale, float offsetX, float offsetY,
                          const char* diagnosticName);

}  // namespace aa_poc

#endif  // THORVG_TOOLS_AA_POC_PRODUCT_SCENES_H
