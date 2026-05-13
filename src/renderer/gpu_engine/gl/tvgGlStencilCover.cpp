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

#include "tvgGlCommon.h"
#include "tvgGlStencilCover.h"

static uint32_t _alignPow2(uint32_t value)
{
    uint32_t ret = 1;
    while (ret < value) ret <<= 1;
    return ret;
}

void GlStencilCover::add(GlStencilCoverSlot* slot)
{
    if (!slot) return;
    for (uint32_t i = 0; i < slots.count; ++i) {
        if (slots[i] == slot) return;
    }
    slots.push(slot);
}

void GlStencilCover::remove(GlStencilCoverSlot* slot)
{
    if (!slot) return;
    for (uint32_t i = 0; i < slots.count; ++i) {
        if (slots[i] == slot) {
            slots[i] = slots[--slots.count];
            break;
        }
    }
    slot->reset();
}

void GlStencilCover::update(GlShape& sdata)
{
    auto updateSlot = [&](GlStencilCoverSlot& slot, RenderUpdateFlag flag, bool enabled) {
        auto mode = sdata.geometry.getStencilMode(flag);
        auto bounds = sdata.geometry.stencilBounds(flag);
        bounds.intersect(sdata.geometry.viewport);
        if (!enabled || mode == GlStencilMode::None || bounds.invalid()) {
            remove(&slot);
            return;
        }

        slot.bounds = bounds;
        slot.atlas.reset();
        slot.packed = false;
        add(&slot);
    };

    updateSlot(sdata.fillStencilCoverSlot, RenderUpdateFlag::Color, sdata.validFill);
    updateSlot(sdata.strokeStencilCoverSlot, RenderUpdateFlag::Stroke, sdata.validStroke);
}

bool GlStencilCover::ensure(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0) return true;
    if (texture && fbo && this->width >= width && this->height >= height) return true;

    width = _alignPow2(max(width, this->width));
    height = _alignPow2(max(height, this->height));

    GLint restoreFbo = 0;
    GL_CHECK(glGetIntegerv(GL_FRAMEBUFFER_BINDING, &restoreFbo));

    if (!texture) GL_CHECK(glGenTextures(1, &texture));
    GL_CHECK(glBindTexture(GL_TEXTURE_2D, texture));
    GL_CHECK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RG8, width, height, 0, GL_RG, GL_UNSIGNED_BYTE, nullptr));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
    GL_CHECK(glBindTexture(GL_TEXTURE_2D, 0));

    if (!fbo) GL_CHECK(glGenFramebuffers(1, &fbo));
    GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, fbo));
    GL_CHECK(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0));
    GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, restoreFbo));

    this->width = width;
    this->height = height;
    return true;
}

bool GlStencilCover::pack()
{
    uint32_t count = 0;
    uint32_t atlasWidth = width;

    for (uint32_t i = 0; i < slots.count; ++i) {
        auto slot = slots[i];
        if (!slot || slot->bounds.invalid()) {
            if (slot) slot->packed = false;
            continue;
        }
        slot->packed = false;
        slots[count++] = slot;
        atlasWidth = max(atlasWidth, static_cast<uint32_t>(slot->bounds.w()));
    }
    slots.count = count;

    if (slots.empty()) return true;

    atlasWidth = _alignPow2(atlasWidth);

    uint32_t x = 0;
    uint32_t y = 0;
    uint32_t rowHeight = 0;

    for (uint32_t i = 0; i < slots.count; ++i) {
        auto slot = slots[i];
        auto slotWidth = static_cast<uint32_t>(slot->bounds.w());
        auto slotHeight = static_cast<uint32_t>(slot->bounds.h());

        if (x > 0 && x + slotWidth > atlasWidth) {
            y += rowHeight;
            x = 0;
            rowHeight = 0;
        }

        slot->atlas = {{static_cast<int32_t>(x), static_cast<int32_t>(y)}, {static_cast<int32_t>(x + slotWidth), static_cast<int32_t>(y + slotHeight)}};
        slot->packed = true;

        x += slotWidth;
        rowHeight = max(rowHeight, slotHeight);
    }

    return ensure(atlasWidth, _alignPow2(y + rowHeight));
}

void GlStencilCover::reset()
{
    ARRAY_FOREACH(slot, slots) {
        if (*slot) (*slot)->reset();
    }
    slots.clear();

    if (fbo) GL_CHECK(glDeleteFramebuffers(1, &fbo));
    if (texture) GL_CHECK(glDeleteTextures(1, &texture));

    fbo = 0;
    texture = 0;
    width = 0;
    height = 0;
}

const GlStencilCoverSlot* GlStencilCover::slot(const GlShape& sdata, RenderUpdateFlag flag)
{
    if ((flag & RenderUpdateFlag::Stroke) || (flag & RenderUpdateFlag::GradientStroke)) return &sdata.strokeStencilCoverSlot;
    return &sdata.fillStencilCoverSlot;
}
