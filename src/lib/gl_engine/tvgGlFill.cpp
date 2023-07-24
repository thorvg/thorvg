#include "tvgGlFill.h"

GlLinearBlock::GlLinearBlock(tvg::LinearGradient* gradient)
{
    gradient->linear(&startPos[0], &startPos[1], &endPos[0], &endPos[1]);

    const tvg::Fill::ColorStop* colorStops = nullptr;

    nStops = static_cast<uint32_t>(gradient->colorStops(&colorStops));

    if (nStops == 0) {
        return;
    }

    for (int32_t i = 0; i < nStops; i++) {
        stopPoints[i] = colorStops[i].offset;

        stopColors[i * 4 + 0] = colorStops[i].r / 255.f;
        stopColors[i * 4 + 1] = colorStops[i].g / 255.f;
        stopColors[i * 4 + 2] = colorStops[i].b / 255.f;
        stopColors[i * 4 + 3] = colorStops[i].a / 255.f;
    }
}