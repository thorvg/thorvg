/*
 * Copyright (c) 2023 - 2025 the ThorVG project. All rights reserved.

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

#ifndef _TVG_LOTTIE_BUILDER_H_
#define _TVG_LOTTIE_BUILDER_H_

#include "tvgCommon.h"
#include "tvgInlist.h"
#include "tvgShape.h"
#include "tvgLottieExpressions.h"
#include "tvgLottieModifier.h"

struct LottieComposition;
struct AssetResolver;

struct RenderRepeater
{
    int cnt;
    Matrix transform;
    float offset;
    Point position;
    Point anchor;
    Point scale;
    float rotation;
    uint8_t startOpacity;
    uint8_t endOpacity;
    bool inorder;
};

struct RenderText
{
    Point cursor{};
    int line = 0, space = 0, idx = 0;
    float lineSpace = 0.0f, totalLineSpace = 0.0f;
    char *p;  //current processing character
    int nChars;
    float scale;
    Scene* textScene;
    Scene* lineScene;
    float capScale, firstMargin;
    LottieTextFollowPath* follow;

    RenderText(LottieText* text, const TextDocument& doc) : p(doc.text), nChars(strlen(p)), scale(doc.size), textScene(Scene::gen()), lineScene(Scene::gen())
    {
    }

    ~RenderText()
    {
        Paint::rel(textScene);
        Paint::rel(lineScene);
    }
};

enum RenderFragment : uint8_t {ByNone = 0, ByFill, ByStroke};

struct RenderContext
{
    INLIST_ITEM(RenderContext);

    Shape* propagator = nullptr;  //for propagating the shape properties excluding paths
    Shape* merging = nullptr;  //merging shapes if possible (if shapes have same properties)
    LottieObject** begin = nullptr; //iteration entry point
    Array<RenderRepeater> repeaters;
    Matrix* transform = nullptr;
    LottieRoundnessModifier* roundness = nullptr;
    LottieOffsetModifier* offset = nullptr;
    LottieModifier* modifier = nullptr;
    RenderFragment fragment = ByNone;  //render context has been fragmented
    bool reqFragment = false;  //requirement to fragment the render context

    RenderContext(Shape* propagator)
    {
        to<ShapeImpl>(propagator)->reset();
        propagator->ref();
        this->propagator = propagator;
    }

    ~RenderContext()
    {
        propagator->unref(false);
        delete(transform);
        delete(roundness);
        delete(offset);
    }

    RenderContext(const RenderContext& rhs, Shape* propagator, bool mergeable = false) : propagator(propagator)
    {
        if (mergeable) merging = rhs.merging;
        propagator->ref();
        repeaters = rhs.repeaters;
        fragment = rhs.fragment;
        if (rhs.roundness) {
            roundness = new LottieRoundnessModifier(rhs.roundness->buffer, rhs.roundness->r);
            update(roundness);
        }
        if (rhs.offset) {
            offset = new LottieOffsetModifier(rhs.offset->offset, rhs.offset->miterLimit, rhs.offset->join);
            update(offset);
        }
        if (rhs.transform) {
            transform = new Matrix;
            *transform = *rhs.transform;
        }
    }

    void update(LottieModifier* next)
    {
        if (modifier) modifier = modifier->decorate(next);
        else modifier = next;
    }
};

struct LottieBuilder
{
    LottieBuilder()
    {
        exps = LottieExpressions::instance();
    }

    ~LottieBuilder()
    {
        LottieExpressions::retrieve(exps);
    }

    bool expressions()
    {
        return exps ? true : false;
    }

    void offTween()
    {
        if (tween.active) tween.active = false;
    }

    void onTween(float to, float progress)
    {
        tween.frameNo = to;
        tween.progress = progress;
        tween.active = true;
    }

    bool tweening()
    {
        return tween.active;
    }

    bool update(LottieComposition* comp, float progress);
    void build(LottieComposition* comp);

    const AssetResolver* resolver = nullptr;  //do not free this

private:
    void appendRect(Shape* shape, Point& pos, Point& size, float r, bool clockwise, RenderContext* ctx);
    bool fragmented(LottieGroup* parent, LottieObject** child, Inlist<RenderContext>& contexts, RenderContext* ctx, RenderFragment fragment);
    Shape* textShape(LottieText* text, float frameNo, const TextDocument& doc, LottieGlyph* glyph, const RenderText& ctx);

    void updateStrokeEffect(LottieLayer* layer, LottieFxStroke* effect, float frameNo);
    void updateEffect(LottieLayer* layer, float frameNo, uint8_t quality);
    void updateLayer(LottieComposition* comp, Scene* scene, LottieLayer* layer, float frameNo);
    bool updateMatte(LottieComposition* comp, float frameNo, Scene* scene, LottieLayer* layer);
    void updatePrecomp(LottieComposition* comp, LottieLayer* precomp, float frameNo);
    void updatePrecomp(LottieComposition* comp, LottieLayer* precomp, float frameNo, Tween& tween);
    void updateSolid(LottieLayer* layer);
    void updateImage(LottieGroup* layer);
    void updateURLFont(LottieLayer* layer, float frameNo, LottieText* text, const TextDocument& doc);
    void updateLocalFont(LottieLayer* layer, float frameNo, LottieText* text, const TextDocument& doc);
    bool updateTextRange(LottieText* text, float frameNo, Shape* shape, const TextDocument& doc, RenderText& ctx);
    void updateText(LottieLayer* layer, float frameNo);
    void updateMasks(LottieLayer* layer, float frameNo);
    void updateTransform(LottieLayer* layer, float frameNo);
    void updateChildren(LottieGroup* parent, float frameNo, Inlist<RenderContext>& contexts);
    void updateGroup(LottieGroup* parent, LottieObject** child, float frameNo, Inlist<RenderContext>& pcontexts, RenderContext* ctx);
    void updateTransform(LottieGroup* parent, LottieObject** child, float frameNo, Inlist<RenderContext>& contexts, RenderContext* ctx);
    bool updateSolidFill(LottieGroup* parent, LottieObject** child, float frameNo, Inlist<RenderContext>& contexts, RenderContext* ctx);
    bool updateSolidStroke(LottieGroup* parent, LottieObject** child, float frameNo, Inlist<RenderContext>& contexts, RenderContext* ctx);
    bool updateGradientFill(LottieGroup* parent, LottieObject** child, float frameNo, Inlist<RenderContext>& contexts, RenderContext* ctx);
    bool updateGradientStroke(LottieGroup* parent, LottieObject** child, float frameNo, Inlist<RenderContext>& contexts, RenderContext* ctx);
    void updateRect(LottieGroup* parent, LottieObject** child, float frameNo, Inlist<RenderContext>& contexts, RenderContext* ctx);
    void updateEllipse(LottieGroup* parent, LottieObject** child, float frameNo, Inlist<RenderContext>& contexts, RenderContext* ctx);
    void updatePath(LottieGroup* parent, LottieObject** child, float frameNo, Inlist<RenderContext>& contexts, RenderContext* ctx);
    void updatePolystar(LottieGroup* parent, LottieObject** child, float frameNo, Inlist<RenderContext>& contexts, RenderContext* ctx);
    void updateStar(LottiePolyStar* star, float frameNo, Matrix* transform, Shape* merging, RenderContext* ctx, Tween& tween, LottieExpressions* exps);
    void updatePolygon(LottieGroup* parent, LottiePolyStar* star, float frameNo, Matrix* transform, Shape* merging, RenderContext* ctx, Tween& tween, LottieExpressions* exps);
    void updateTrimpath(LottieGroup* parent, LottieObject** child, float frameNo, Inlist<RenderContext>& contexts, RenderContext* ctx);
    void updateRepeater(LottieGroup* parent, LottieObject** child, float frameNo, Inlist<RenderContext>& contexts, RenderContext* ctx);
    void updateRoundedCorner(LottieGroup* parent, LottieObject** child, float frameNo, Inlist<RenderContext>& contexts, RenderContext* ctx);
    void updateOffsetPath(LottieGroup* parent, LottieObject** child, float frameNo, Inlist<RenderContext>& contexts, RenderContext* ctx);

    RenderPath buffer;   //resusable path
    LottieExpressions* exps;
    Tween tween;
};

#endif //_TVG_LOTTIE_BUILDER_H