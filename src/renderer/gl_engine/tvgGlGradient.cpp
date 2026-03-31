/*
 * Copyright (c) 2026 ThorVG project. All rights reserved.

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <cstring>
#include "tvgFill.h"
#include "tvgGlGradient.h"

static float gradientClampOffset(float offset)
{
    if (offset < 0.0f) return 0.0f;
    if (offset > 1.0f) return 1.0f;
    return offset;
}

static void gradientRampColor(uint8_t* data, uint32_t texel, const Fill::ColorStop& stop)
{
    data[texel * 4 + 0] = stop.r;
    data[texel * 4 + 1] = stop.g;
    data[texel * 4 + 2] = stop.b;
    data[texel * 4 + 3] = stop.a;
}

static void gradientRampColor(uint8_t* data, uint32_t texel, const Fill::ColorStop& lhs, const Fill::ColorStop& rhs, float t)
{
    data[texel * 4 + 0] = tvg::lerp(lhs.r, rhs.r, t);
    data[texel * 4 + 1] = tvg::lerp(lhs.g, rhs.g, t);
    data[texel * 4 + 2] = tvg::lerp(lhs.b, rhs.b, t);
    data[texel * 4 + 3] = tvg::lerp(lhs.a, rhs.a, t);
}

static void bakeGradientRamp(const Fill* fill, uint8_t* data)
{
    struct GradientStopGroup
    {
        float offset = 0.0f;
        Fill::ColorStop first = {};
        Fill::ColorStop last = {};
    };

    memset(data, 0, GL_TEXTURE_GRADIENT_SIZE * 4);
    if (!fill) return;

    const Fill::ColorStop* stops = nullptr;
    auto stopCnt = fill->colorStops(&stops);
    if (stopCnt == 0) return;

    Array<GradientStopGroup> groups(stopCnt);
    GradientStopGroup group;
    group.offset = gradientClampOffset(stops[0].offset);
    group.first = stops[0];
    group.first.offset = group.offset;
    group.last = group.first;
    groups.push(group);

    for (uint32_t i = 1; i < stopCnt; ++i) {
        auto stop = stops[i];
        stop.offset = gradientClampOffset(stop.offset);

        if (groups.last().offset < stop.offset) {
            GradientStopGroup group;
            group.offset = stop.offset;
            group.first = stop;
            group.last = stop;
            groups.push(group);
        } else if (groups.last().offset == stop.offset) {
            groups.last().last = stop;
        }
    }

    if (groups.empty()) return;

    uint32_t groupIdx = 0;
    for (uint32_t texel = 0; texel < GL_TEXTURE_GRADIENT_SIZE; ++texel) {
        auto pos = float(texel) / float(GL_TEXTURE_GRADIENT_SIZE - 1);

        while (groupIdx + 1 < groups.count && pos >= groups[groupIdx + 1].offset) {
            ++groupIdx;
        }

        if (pos < groups.first().offset) {
            gradientRampColor(data, texel, groups.first().first);
            continue;
        }

        if (groupIdx + 1 >= groups.count) {
            gradientRampColor(data, texel, groups.last().last);
            continue;
        }

        const auto& current = groups[groupIdx];
        const auto& next = groups[groupIdx + 1];
        auto interval = next.offset - current.offset;

        if (tvg::zero(interval)) {
            gradientRampColor(data, texel, next.last);
            continue;
        }

        auto t = (pos - current.offset) / interval;
        gradientRampColor(data, texel, current.last, next.first, t);
    }
}

template<typename T>
static void gradientArrayErase(Array<T>& array, uint32_t index)
{
    if (index + 1 < array.count) {
        memmove(array.data + index, array.data + index + 1, sizeof(T) * (array.count - index - 1));
    }
    array.pop();
}

static void gradientAtlasDirtyReset(GlGradientAtlas& atlas)
{
    atlas.dirtyMin = UINT32_MAX;
    atlas.dirtyMax = 0;
}

static void gradientSlotDirty(GlGradientAtlas& atlas, uint32_t slot)
{
    if (atlas.dirtyMin == UINT32_MAX || slot < atlas.dirtyMin) atlas.dirtyMin = slot;
    if (slot > atlas.dirtyMax) atlas.dirtyMax = slot;
}

static void gradientFreeSlotRemove(GlGradientAtlas& atlas, uint32_t slotId)
{
    for (uint32_t i = 0; i < atlas.freeSlots.count; ++i) {
        if (atlas.freeSlots[i] > slotId) return;
        if (atlas.freeSlots[i] != slotId) continue;

        gradientArrayErase(atlas.freeSlots, i);
        return;
    }
}

static void gradientSlotTrimTrailingFreeRows(GlGradientAtlas& atlas)
{
    while (!atlas.slots.empty() && !atlas.slots.last().used) {
        auto slotId = atlas.slots.count - 1;
        gradientFreeSlotRemove(atlas, slotId);
        atlas.slots.pop();
    }
}

static void gradientAtlasClampDirtyRange(GlGradientAtlas& atlas)
{
    if (atlas.dirtyMin == UINT32_MAX) return;
    if (atlas.slots.empty() || atlas.dirtyMin >= atlas.slots.count) {
        gradientAtlasDirtyReset(atlas);
        return;
    }
    if (atlas.dirtyMax >= atlas.slots.count) atlas.dirtyMax = atlas.slots.count - 1;
}

static void gradientAtlasBind(GLuint texId)
{
    GL_CHECK(glBindTexture(GL_TEXTURE_2D, texId));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
}

static uint32_t gradientAtlasHeight(uint32_t required)
{
    uint32_t next = 1;
    while (next < required) next <<= 1u;
    return next;
}

static void gradientAtlasDestroyTexture(GlGradientAtlas& atlas)
{
    if (!atlas.texId) return;

    GL_CHECK(glDeleteTextures(1, &atlas.texId));
    atlas.texId = 0;
    atlas.height = 0;
}

static void gradientAtlasUpload(GlGradientAtlas& atlas)
{
    if (!atlas.texId || atlas.height == 0) return;

    auto size = GL_TEXTURE_GRADIENT_SIZE * atlas.height * 4;
    Array<uint8_t> data(size);
    memset(data.data, 0, size);
    for (uint32_t slot = 0; slot < atlas.slots.count; ++slot) {
        if (!atlas.slots[slot].used) continue;
        memcpy(&data[slot * GL_TEXTURE_GRADIENT_SIZE * 4], atlas.slots[slot].data, GL_TEXTURE_GRADIENT_SIZE * 4);
    }

    gradientAtlasBind(atlas.texId);
    GL_CHECK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, GL_TEXTURE_GRADIENT_SIZE, atlas.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data.data));
    GL_CHECK(glBindTexture(GL_TEXTURE_2D, 0));
}

static void gradientAtlasUploadDirty(GlGradientAtlas& atlas)
{
    if (!atlas.texId || atlas.dirtyMin == UINT32_MAX) return;

    auto rowCount = atlas.dirtyMax - atlas.dirtyMin + 1;
    auto size = GL_TEXTURE_GRADIENT_SIZE * rowCount * 4;
    Array<uint8_t> data(size);
    memset(data.data, 0, size);

    for (uint32_t slot = atlas.dirtyMin; slot <= atlas.dirtyMax; ++slot) {
        if (!atlas.slots[slot].used) continue;
        memcpy(&data[(slot - atlas.dirtyMin) * GL_TEXTURE_GRADIENT_SIZE * 4], atlas.slots[slot].data, GL_TEXTURE_GRADIENT_SIZE * 4);
    }

    gradientAtlasBind(atlas.texId);
    GL_CHECK(glTexSubImage2D(GL_TEXTURE_2D, 0, 0, atlas.dirtyMin, GL_TEXTURE_GRADIENT_SIZE, rowCount, GL_RGBA, GL_UNSIGNED_BYTE, data.data));
    GL_CHECK(glBindTexture(GL_TEXTURE_2D, 0));
}

void GlGradientAtlas::update(uint32_t& slotId, const Fill* fill)
{
    const Fill::ColorStop* stops = nullptr;
    if (!fill || fill->colorStops(&stops) < 2) {
        release(slotId);
        return;
    }

    if (slotId == UINT32_MAX) {
        if (freeSlots.empty()) {
            GlGradientAtlas::Slot slot = {};
            slots.push(slot);
            slotId = slots.count - 1;
        } else {
            slotId = freeSlots.first();
            gradientArrayErase(freeSlots, 0);
        }
    }

    auto& slot = slots[slotId];
    slot.used = true;
    bakeGradientRamp(fill, slot.data);
    gradientSlotDirty(*this, slotId);
}

void GlGradientAtlas::flush()
{
    gradientSlotTrimTrailingFreeRows(*this);
    gradientAtlasClampDirtyRange(*this);

    if (slots.empty()) {
        gradientAtlasDestroyTexture(*this);
        fullUpload = false;
        gradientAtlasDirtyReset(*this);
        return;
    }

    auto targetHeight = gradientAtlasHeight(slots.count);
    if (!texId) {
        GL_CHECK(glGenTextures(1, &texId));
        fullUpload = true;
    }
    if (height != targetHeight) {
        height = targetHeight;
        fullUpload = true;
    }

    if (fullUpload) {
        gradientAtlasUpload(*this);
    } else {
        gradientAtlasUploadDirty(*this);
    }

    fullUpload = false;
    gradientAtlasDirtyReset(*this);
}

void GlGradientAtlas::invalidate()
{
    gradientAtlasDestroyTexture(*this);
    fullUpload = !slots.empty();
    gradientAtlasDirtyReset(*this);
}

void GlGradientAtlas::release(uint32_t& slotId)
{
    if (slotId == UINT32_MAX) return;

    auto& slot = slots[slotId];
    slot.used = false;
    freeSlots.push(slotId);
    auto index = freeSlots.count - 1;
    while (index > 0 && freeSlots[index - 1] > slotId) {
        freeSlots[index] = freeSlots[index - 1];
        --index;
    }
    freeSlots[index] = slotId;
    gradientSlotDirty(*this, slotId);
    gradientSlotTrimTrailingFreeRows(*this);
    gradientAtlasClampDirtyRange(*this);
    slotId = UINT32_MAX;
}

void GlGradientAtlas::clear()
{
    gradientAtlasDestroyTexture(*this);
    slots.clear();
    freeSlots.clear();
    fullUpload = false;
    gradientAtlasDirtyReset(*this);
}

float GlGradientAtlas::rowCenter(uint32_t slotId) const
{
    if (slotId == UINT32_MAX || height == 0) return 0.5f;
    return (float(slotId) + 0.5f) / float(height);
}
