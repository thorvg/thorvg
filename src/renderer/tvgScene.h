/*
 * Copyright (c) 2020 - 2025 the ThorVG project. All rights reserved.

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

#ifndef _TVG_SCENE_H_
#define _TVG_SCENE_H_

#include <algorithm>
#include <cstdarg>
#include "tvgMath.h"
#include "tvgPaint.h"

#define SCENE(A) static_cast<SceneImpl*>(A)
#define CONST_SCENE(A) static_cast<const SceneImpl*>(A)

struct SceneIterator : Iterator
{
    list<Paint*>* paints;
    list<Paint*>::iterator itr;

    SceneIterator(list<Paint*>* p) : paints(p)
    {
        begin();
    }

    const Paint* next() override
    {
        if (itr == paints->end()) return nullptr;
        auto paint = *itr;
        ++itr;
        return paint;
    }

    uint32_t count() override
    {
       return paints->size();
    }

    void begin() override
    {
        itr = paints->begin();
    }
};

struct SceneImpl : Scene
{
    Paint::Impl impl;
    list<Paint*> paints;     //children list
    RenderRegion vport = {0, 0, INT32_MAX, INT32_MAX};
    Array<RenderEffect*>* effects = nullptr;
    uint8_t compFlag = CompositionFlag::Invalid;
    uint8_t opacity;         //for composition

    SceneImpl() : impl(Paint::Impl(this))
    {
    }

    ~SceneImpl()
    {
        resetEffects();
        clearPaints();
    }

    uint8_t needComposition(uint8_t opacity)
    {
        compFlag = CompositionFlag::Invalid;

        if (opacity == 0 || paints.empty()) return 0;

        //post effects, masking, blending may require composition
        if (effects) compFlag |= CompositionFlag::PostProcessing;
        if (PAINT(this)->mask(nullptr) != MaskMethod::None) compFlag |= CompositionFlag::Masking;
        if (impl.blendMethod != BlendMethod::Normal) compFlag |= CompositionFlag::Blending;

        //Half translucent requires intermediate composition.
        if (opacity == 255) return compFlag;

        //If scene has several children or only scene, it may require composition.
        //OPTIMIZE: the bitmap type of the picture would not need the composition.
        //OPTIMIZE: a single paint of a scene would not need the composition.
        if (paints.size() == 1 && paints.front()->type() == Type::Shape) return compFlag;

        compFlag |= CompositionFlag::Opacity;

        return 1;
    }

    RenderData update(RenderMethod* renderer, const Matrix& transform, Array<RenderData>& clips, uint8_t opacity, RenderUpdateFlag flag, TVG_UNUSED bool clipper)
    {
        this->vport = renderer->viewport();

        if (needComposition(opacity)) {
            /* Overriding opacity value. If this scene is half-translucent,
               It must do intermediate composition with that opacity value. */
            this->opacity = opacity;
            opacity = 255;
        }
        for (auto paint : paints) {
            paint->pImpl->update(renderer, transform, clips, opacity, flag, false);
        }

        if (effects) {
            ARRAY_FOREACH(p, *effects) {
                renderer->prepare(*p, transform);
            }
        }

        return nullptr;
    }

    bool render(RenderMethod* renderer)
    {
        RenderCompositor* cmp = nullptr;
        auto ret = true;

        renderer->blend(impl.blendMethod);

        if (compFlag) {
            cmp = renderer->target(bounds(renderer), renderer->colorSpace(), static_cast<CompositionFlag>(compFlag));
            renderer->beginComposite(cmp, MaskMethod::None, opacity);
        }

        for (auto paint : paints) {
            ret &= paint->pImpl->render(renderer);
        }

        if (cmp) {
            //Apply post effects if any.
            if (effects) {
                //Notify the possiblity of the direct composition of the effect result to the origin surface.
                auto direct = (effects->count == 1) & (compFlag == CompositionFlag::PostProcessing);
                ARRAY_FOREACH(p, *effects) {
                    if ((*p)->valid) renderer->render(cmp, *p, direct);
                }
            }
            renderer->endComposite(cmp);
        }

        return ret;
    }

    RenderRegion bounds(RenderMethod* renderer) const
    {
        if (paints.empty()) return {0, 0, 0, 0};

        int32_t x1 = INT32_MAX;
        int32_t y1 = INT32_MAX;
        int32_t x2 = 0;
        int32_t y2 = 0;

        for (auto paint : paints) {
            auto region = paint->pImpl->bounds(renderer);

            //Merge regions
            if (region.x < x1) x1 = region.x;
            if (x2 < region.x + region.w) x2 = (region.x + region.w);
            if (region.y < y1) y1 = region.y;
            if (y2 < region.y + region.h) y2 = (region.y + region.h);
        }

        //Extends the render region if post effects require
        int32_t ex = 0, ey = 0, ew = 0, eh = 0;
        if (effects) {
            ARRAY_FOREACH(p, *effects) {
                auto effect = *p;
                if (effect->valid && renderer->region(effect)) {
                    ex = std::min(ex, effect->extend.x);
                    ey = std::min(ey, effect->extend.y);
                    ew = std::max(ew, effect->extend.w);
                    eh = std::max(eh, effect->extend.h);
                }
            }
        }

        auto ret = RenderRegion{x1 + ex, y1 + ey, (x2 - x1) + ew, (y2 - y1) + eh};
        ret.intersect(this->vport);
        return ret;
    }

    Result bounds(Point* pt4, Matrix& m, bool obb, bool stroking)
    {
        if (paints.empty()) return Result::InsufficientCondition;

        Point min = {FLT_MAX, FLT_MAX};
        Point max = {-FLT_MAX, -FLT_MAX};

        for (auto paint : paints) {
            Point tmp[4];
            if (PAINT(paint)->bounds(tmp, obb ? nullptr : &m, false, stroking) != Result::Success) continue;
            //Merge regions
            for (int i = 0; i < 4; ++i) {
                if (tmp[i].x < min.x) min.x = tmp[i].x;
                if (tmp[i].x > max.x) max.x = tmp[i].x;
                if (tmp[i].y < min.y) min.y = tmp[i].y;
                if (tmp[i].y > max.y) max.y = tmp[i].y;
            }
        }
        pt4[0] = min;
        pt4[1] = Point{max.x, min.y};
        pt4[2] = max;
        pt4[3] = Point{min.x, max.y};

        if (obb) {
            pt4[0] *= m;
            pt4[1] *= m;
            pt4[2] *= m;
            pt4[3] *= m;
        }

        return Result::Success;
    }

    Paint* duplicate(Paint* ret)
    {
        if (ret) TVGERR("RENDERER", "TODO: duplicate()");

        auto scene = Scene::gen();
        auto dup = SCENE(scene);

        for (auto paint : paints) {
            auto cdup = paint->duplicate();
            PAINT(cdup)->parent = scene;
            cdup->ref();
            dup->paints.push_back(cdup);
        }

        if (effects) TVGERR("RENDERER", "TODO: Duplicate Effects?");

        return scene;
    }

    Result clearPaints()
    {
        auto itr = paints.begin();
        while (itr != paints.end()) {
            PAINT((*itr))->unref();
            paints.erase(itr++);
        }
        return Result::Success;
    }

    Result remove(Paint* paint)
    {
        if (PAINT(paint)->parent != this) return Result::InsufficientCondition;
        PAINT(paint)->unref();
        paints.remove(paint);
        return Result::Success;
    }

    Result insert(Paint* target, Paint* at)
    {
        if (!target) return Result::InvalidArguments;
        auto timpl = PAINT(target);
        if (timpl->parent) return Result::InsufficientCondition;

        target->ref();

        //Relocated the paint to the current scene space
        timpl->mark(RenderUpdateFlag::Transform);

        if (!at) {
            paints.push_back(target);
        } else {
            //OPTIMIZE: Remove searching?
            auto itr = find_if(paints.begin(), paints.end(),[&at](const Paint* paint){ return at == paint; });
            if (itr == paints.end()) return Result::InvalidArguments;
            paints.insert(itr, target);
        }
        timpl->parent = this;
        if (timpl->clipper) PAINT(timpl->clipper)->parent = this;
        if (timpl->maskData) PAINT(timpl->maskData->target)->parent = this;
        return Result::Success;
    }

    Iterator* iterator()
    {
        return new SceneIterator(&paints);
    }

    Result resetEffects()
    {
        if (effects) {
            ARRAY_FOREACH(p, *effects) {
                impl.renderer->dispose(*p);
                delete(*p);
            }
            delete(effects);
            effects = nullptr;
        }
        return Result::Success;
    }

    Result push(SceneEffect effect, va_list& args)
    {
        if (effect == SceneEffect::ClearAll) return resetEffects();

        if (!this->effects) this->effects = new Array<RenderEffect*>;

        RenderEffect* re = nullptr;

        switch (effect) {
            case SceneEffect::GaussianBlur: {
                re = RenderEffectGaussianBlur::gen(args);
                break;
            }
            case SceneEffect::DropShadow: {
                re = RenderEffectDropShadow::gen(args);
                break;
            }
            case SceneEffect::Fill: {
                re = RenderEffectFill::gen(args);
                break;
            }
            case SceneEffect::Tint: {
                re = RenderEffectTint::gen(args);
                break;
            }
            case SceneEffect::Tritone: {
                re = RenderEffectTritone::gen(args);
                break;
            }
            default: break;
        }

        if (!re) return Result::InvalidArguments;

        this->effects->push(re);

        return Result::Success;
    }
};

#endif //_TVG_SCENE_H_
