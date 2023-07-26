#include "thorvg.h"

enum
{
    MAX_STOP_COUNT = 4,
};

/**
 * represent the uniform block in linear_gradient.frag
 *
 */
struct GlLinearBlock
{
    int32_t nStops = {};
    alignas(16) float startPos[2] = {};
    alignas(8) float endPos[2] = {};
    alignas(8) float stopPoints[MAX_STOP_COUNT] = {};
    alignas(16) float stopColors[4 * MAX_STOP_COUNT] = {};

    GlLinearBlock(tvg::LinearGradient* gradient);

    ~GlLinearBlock() = default;
};

struct GlRadialBlock
{
    int32_t nStops = {};
    alignas(16) float centerPos[2] = {};
    alignas(8) float radius[2] = {};
    alignas(8) float stopPoints[MAX_STOP_COUNT] = {};
    alignas(16) float stopColors[4 * MAX_STOP_COUNT] = {};

    GlRadialBlock(tvg::RadialGradient* gradient);

    ~GlRadialBlock() = default;
};