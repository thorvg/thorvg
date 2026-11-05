/*
 * Copyright (c) 2022 - 2026 ThorVG project. All rights reserved.

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

#include "tvgSvgUtil.h"
#include "tvgSvgCssStyle.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

static inline bool _isImportanceApplicable(const SvgStyleFlags& toFlagsImportance, SvgStyleFlags fromFlagsImportance, SvgStyleFlags flag)
{
    return !(toFlagsImportance & flag) && (fromFlagsImportance & flag);
}

static bool _takesPrecedence(const SvgStyleProperty* from, const SvgStyleProperty* to, SvgStyleFlags flag, bool overwrite)
{
    if (overwrite) return !(to->flagsImportance & flag) || (from->flagsImportance & flag);
    return !(to->flags & flag) || _isImportanceApplicable(to->flagsImportance, from->flagsImportance, flag);
}

static void _markCopied(SvgStyleProperty* to, const SvgStyleProperty* from, SvgStyleFlags flag)
{
    to->flags |= flag;
    if (from->flagsImportance & flag) to->flagsImportance |= flag;
}

static void _replaceOrClearUrl(char** to, const char* from)
{
    tvg::free(*to);
    *to = nullptr;
    svgUtilReplace(to, from);
}

static void _copyStyle(SvgStyleProperty* to, const SvgStyleProperty* from, bool overwrite)
{
    if (!from) return;

    auto keep = overwrite ? SvgStyleFlags(int(to->flagsImportance) & ~int(from->flagsImportance)) : to->flags;

    //Copy the properties of 'from' only if they were explicitly set (not the default ones).
    if ((from->curColorSet && !(keep & SvgStyleFlags::Color)) ||
        _isImportanceApplicable(to->flagsImportance, from->flagsImportance, SvgStyleFlags::Color)) {
        to->color = from->color;
        to->curColorSet = true;
        _markCopied(to, from, SvgStyleFlags::Color);
    }
    if (((from->flags & SvgStyleFlags::PaintOrder) && !(keep & SvgStyleFlags::PaintOrder)) ||
        _isImportanceApplicable(to->flagsImportance, from->flagsImportance, SvgStyleFlags::PaintOrder)) {
        to->paintOrder = from->paintOrder;
        _markCopied(to, from, SvgStyleFlags::PaintOrder);
    }
    if (((from->flags & SvgStyleFlags::Display) && !(keep & SvgStyleFlags::Display)) ||
        _isImportanceApplicable(to->flagsImportance, from->flagsImportance, SvgStyleFlags::Display)) {
        to->display = from->display;
        _markCopied(to, from, SvgStyleFlags::Display);
    }
    //Fill
    if (((from->fill.flags & SvgFillFlags::Paint) && !(keep & SvgStyleFlags::Fill)) ||
        _isImportanceApplicable(to->flagsImportance, from->flagsImportance, SvgStyleFlags::Fill)) {
        to->fill.paint.color = from->fill.paint.color;
        to->fill.paint.none = from->fill.paint.none;
        to->fill.paint.curColor = from->fill.paint.curColor;
        if (from->fill.paint.url) svgUtilReplace(&to->fill.paint.url, from->fill.paint.url);
        to->fill.flags |= SvgFillFlags::Paint;
        _markCopied(to, from, SvgStyleFlags::Fill);
    }
    if (((from->fill.flags & SvgFillFlags::Opacity) && !(keep & SvgStyleFlags::FillOpacity)) ||
        _isImportanceApplicable(to->flagsImportance, from->flagsImportance, SvgStyleFlags::FillOpacity)) {
        to->fill.opacity = from->fill.opacity;
        to->fill.flags |= SvgFillFlags::Opacity;
        _markCopied(to, from, SvgStyleFlags::FillOpacity);
    }
    if (((from->fill.flags & SvgFillFlags::FillRule) && !(keep & SvgStyleFlags::FillRule)) ||
        _isImportanceApplicable(to->flagsImportance, from->flagsImportance, SvgStyleFlags::FillRule)) {
        to->fill.fillRule = from->fill.fillRule;
        to->fill.flags |= SvgFillFlags::FillRule;
        _markCopied(to, from, SvgStyleFlags::FillRule);
    }
    //Stroke
    if (((from->stroke.flags & SvgStrokeFlags::Paint) && !(keep & SvgStyleFlags::Stroke)) ||
        _isImportanceApplicable(to->flagsImportance, from->flagsImportance, SvgStyleFlags::Stroke)) {
        to->stroke.paint.color = from->stroke.paint.color;
        to->stroke.paint.none = from->stroke.paint.none;
        to->stroke.paint.curColor = from->stroke.paint.curColor;
        if (from->stroke.paint.url) svgUtilReplace(&to->stroke.paint.url, from->stroke.paint.url);
        to->stroke.flags |= SvgStrokeFlags::Paint;
        _markCopied(to, from, SvgStyleFlags::Stroke);
    }
    if (((from->stroke.flags & SvgStrokeFlags::Opacity) && !(keep & SvgStyleFlags::StrokeOpacity)) ||
        _isImportanceApplicable(to->flagsImportance, from->flagsImportance, SvgStyleFlags::StrokeOpacity)) {
        to->stroke.opacity = from->stroke.opacity;
        to->stroke.flags |= SvgStrokeFlags::Opacity;
        _markCopied(to, from, SvgStyleFlags::StrokeOpacity);
    }
    if (((from->stroke.flags & SvgStrokeFlags::Width) && !(keep & SvgStyleFlags::StrokeWidth)) ||
        _isImportanceApplicable(to->flagsImportance, from->flagsImportance, SvgStyleFlags::StrokeWidth)) {
        to->stroke.width = from->stroke.width;
        to->stroke.flags |= SvgStrokeFlags::Width;
        _markCopied(to, from, SvgStyleFlags::StrokeWidth);
    }
    if (((from->stroke.flags & SvgStrokeFlags::Dash) && !(keep & SvgStyleFlags::StrokeDashArray)) ||
        _isImportanceApplicable(to->flagsImportance, from->flagsImportance, SvgStyleFlags::StrokeDashArray)) {
        if (from->stroke.flags & SvgStrokeFlags::Dash) {
            to->stroke.dash.array.clear();
            to->stroke.dash.array.reserve(from->stroke.dash.array.count);
            ARRAY_FOREACH(p, from->stroke.dash.array) to->stroke.dash.array.push(*p);
            to->stroke.flags |= SvgStrokeFlags::Dash;
            _markCopied(to, from, SvgStyleFlags::StrokeDashArray);
        }
    }
    if (((from->stroke.flags & SvgStrokeFlags::Cap) && !(keep & SvgStyleFlags::StrokeLineCap)) ||
        _isImportanceApplicable(to->flagsImportance, from->flagsImportance, SvgStyleFlags::StrokeLineCap)) {
        to->stroke.cap = from->stroke.cap;
        to->stroke.flags |= SvgStrokeFlags::Cap;
        _markCopied(to, from, SvgStyleFlags::StrokeLineCap);
    }
    if (((from->stroke.flags & SvgStrokeFlags::Join) && !(keep & SvgStyleFlags::StrokeLineJoin)) ||
        _isImportanceApplicable(to->flagsImportance, from->flagsImportance, SvgStyleFlags::StrokeLineJoin)) {
        to->stroke.join = from->stroke.join;
        to->stroke.flags |= SvgStrokeFlags::Join;
        _markCopied(to, from, SvgStyleFlags::StrokeLineJoin);
    }
    //Opacity
    if (((from->flags & SvgStyleFlags::Opacity) && !(keep & SvgStyleFlags::Opacity)) ||
        _isImportanceApplicable(to->flagsImportance, from->flagsImportance, SvgStyleFlags::Opacity)) {
        to->opacity = from->opacity;
        _markCopied(to, from, SvgStyleFlags::Opacity);
    }
    if (((from->flags & SvgStyleFlags::BlendMode) && !(keep & SvgStyleFlags::BlendMode)) ||
        _isImportanceApplicable(to->flagsImportance, from->flagsImportance, SvgStyleFlags::BlendMode)) {
        to->blendMode = from->blendMode;
        _markCopied(to, from, SvgStyleFlags::BlendMode);
    }
    if (((from->flags & SvgStyleFlags::TextAnchor) && !(keep & SvgStyleFlags::TextAnchor)) || _isImportanceApplicable(to->flagsImportance, from->flagsImportance, SvgStyleFlags::TextAnchor)) {
        to->textAnchor = from->textAnchor;
        _markCopied(to, from, SvgStyleFlags::TextAnchor);
    }
    if (((from->flags & SvgStyleFlags::AlignmentBaseline) && !(keep & SvgStyleFlags::AlignmentBaseline)) || _isImportanceApplicable(to->flagsImportance, from->flagsImportance, SvgStyleFlags::AlignmentBaseline)) {
        to->alignmentBaseline = from->alignmentBaseline;
        _markCopied(to, from, SvgStyleFlags::AlignmentBaseline);
    }
    if (((from->flags & SvgStyleFlags::DominantBaseline) && !(keep & SvgStyleFlags::DominantBaseline)) || _isImportanceApplicable(to->flagsImportance, from->flagsImportance, SvgStyleFlags::DominantBaseline)) {
        to->dominantBaseline = from->dominantBaseline;
        _markCopied(to, from, SvgStyleFlags::DominantBaseline);
    }
    if (((from->flags & SvgStyleFlags::FontWeight) && !(keep & SvgStyleFlags::FontWeight)) || _isImportanceApplicable(to->flagsImportance, from->flagsImportance, SvgStyleFlags::FontWeight)) {
        to->fontWeight = from->fontWeight;
        _markCopied(to, from, SvgStyleFlags::FontWeight);
    }
    if (((from->flags & SvgStyleFlags::LetterSpacing) && !(keep & SvgStyleFlags::LetterSpacing)) || _isImportanceApplicable(to->flagsImportance, from->flagsImportance, SvgStyleFlags::LetterSpacing)) {
        to->letterSpacing = from->letterSpacing;
        to->letterSpacingRelative = from->letterSpacingRelative;
        _markCopied(to, from, SvgStyleFlags::LetterSpacing);
    }
    if (((from->flags & SvgStyleFlags::WordSpacing) && !(keep & SvgStyleFlags::WordSpacing)) || _isImportanceApplicable(to->flagsImportance, from->flagsImportance, SvgStyleFlags::WordSpacing)) {
        to->wordSpacing = from->wordSpacing;
        to->wordSpacingRelative = from->wordSpacingRelative;
        _markCopied(to, from, SvgStyleFlags::WordSpacing);
    }
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/


void cssCopyStyleAttr(SvgNode* to, const SvgNode* from, bool overwrite)
{
    //Copy matrix attribute
    if (from->transform && _takesPrecedence(from->style, to->style, SvgStyleFlags::Transform, overwrite)) {
        if (!to->transform) to->transform = tvg::malloc<Matrix>(sizeof(Matrix));
        *to->transform = *from->transform;
        _markCopied(to->style, from->style, SvgStyleFlags::Transform);
    }
    //Copy style attribute
    _copyStyle(to->style, from->style, overwrite);

    if ((from->style->flags & SvgStyleFlags::ClipPath) && _takesPrecedence(from->style, to->style, SvgStyleFlags::ClipPath, overwrite)) {
        _replaceOrClearUrl(&to->style->clipPath.url, from->style->clipPath.url);
        _markCopied(to->style, from->style, SvgStyleFlags::ClipPath);
    }
    if ((from->style->flags & SvgStyleFlags::Mask) && _takesPrecedence(from->style, to->style, SvgStyleFlags::Mask, overwrite)) {
        _replaceOrClearUrl(&to->style->mask.url, from->style->mask.url);
        _markCopied(to->style, from->style, SvgStyleFlags::Mask);
    }
}


SvgNode* cssFindStyleNode(const SvgNode* style, const char* title, SvgNodeType type)
{
    if (!style) return nullptr;
    ARRAY_FOREACH(p, style->child) {
        if ((*p)->type == type) {
            if ((!title && !(*p)->id) || (title && (*p)->id && STR_AS((*p)->id, title))) return *p;
        }
    }
    return nullptr;
}


SvgNode* cssFindStyleNode(const SvgNode* style, const char* title)
{
    if (!style || !title) return nullptr;
    ARRAY_FOREACH(p, style->child) {
        if ((*p)->type == SvgNodeType::CssStyle) {
            if ((*p)->id && STR_AS((*p)->id, title)) return *p;
        }
    }
    return nullptr;
}


void cssUpdateStyle(SvgNode* doc, SvgNode* style)
{
    ARRAY_FOREACH(p, doc->child) {
        if (auto cssNode = cssFindStyleNode(style, nullptr, (*p)->type)) {
            cssCopyStyleAttr(*p, cssNode);
        }
        cssUpdateStyle(*p, style);
    }
}
