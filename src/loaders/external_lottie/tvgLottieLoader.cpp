/*
 * Copyright (c) 2021 Samsung Electronics Co., Ltd. All rights reserved.

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
#include <memory.h>
#include "tvgLoader.h"
#include "tvgLottieLoader.h"


/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

static void _shapeBuildHelper(Paint *_parent, const LOTLayerNode *layer, int depth)
{
    if (!_parent) return;

    auto parent = reinterpret_cast<Scene*>(_parent);

    parent->reserve(layer->mNodeList.size);

    for (unsigned int i = 0; i < layer->mNodeList.size; i++) {
        LOTNode *node = layer->mNodeList.ptr[i];
        if (!node) continue;

#if THORVG_LOG_ENABLED
        for (int i = 0; i < depth; i++) TVGLOG("Lottie", "    ");
        TVGLOG("Lottie", "[node %03d] type:%s keypath:%s\n", i, (node->mImageInfo.data)?"image":"shape", node->keypath);
#endif

        //Image object
        if (node->mImageInfo.data) {
            auto picture = Picture::gen().release();
            Matrix matrix = {
                node->mImageInfo.mMatrix.m11,  node->mImageInfo.mMatrix.m12, node->mImageInfo.mMatrix.m13,
                node->mImageInfo.mMatrix.m21,  node->mImageInfo.mMatrix.m22, node->mImageInfo.mMatrix.m23,
                node->mImageInfo.mMatrix.m31,  node->mImageInfo.mMatrix.m32, node->mImageInfo.mMatrix.m33
            };
            picture->transform(matrix);
            picture->load((uint32_t *)node->mImageInfo.data, node->mImageInfo.width, node->mImageInfo.height, false);
            picture->opacity(node->mImageInfo.mAlpha);
            parent->push(unique_ptr<Picture>(picture));
            continue;
        }

        const float *data = node->mPath.ptPtr;
        if (!data) continue;

        auto shape = Shape::gen().release();

        //0: Path
        uint32_t cmdCnt = node->mPath.elmCount;
        uint32_t ptsCnt = node->mPath.ptCount * sizeof(float) / sizeof(Point);
        PathCommand cmds[cmdCnt];
        Point pts[ptsCnt];

        uint32_t cmd_i = 0, pts_i = 0;
        for (uint32_t i = 0; i < cmdCnt; i++) {
            switch (node->mPath.elmPtr[i]) {
                case 0:
                    cmds[cmd_i++] = PathCommand::MoveTo;
                    pts[pts_i++] = (Point){data[0], data[1]};
                    data += 2;
                    break;
                case 1:
                    cmds[cmd_i++] = PathCommand::LineTo;
                    pts[pts_i++] = (Point){data[0], data[1]};
                    data += 2;
                    break;
                case 2:
                    cmds[cmd_i++] = PathCommand::CubicTo;
                    pts[pts_i++] = (Point){data[0], data[1]};
                    pts[pts_i++] = (Point){data[2], data[3]};
                    pts[pts_i++] = (Point){data[4], data[5]};
                    data += 6;
                    break;
                case 3:
                    cmds[cmd_i++] = PathCommand::Close;
                    break;
                default:
                    TVGLOG("Lottie", "No reserved path type = %d", node->mPath.elmPtr[i]);
                    break;
            }
        }
        shape->appendPath(cmds, cmd_i, pts, pts_i);

        //1: Stroke
        if (node->mStroke.enable) {
            //Stroke Width
            shape->stroke(node->mStroke.width);

            //Stroke Cap
            StrokeCap cap;
            switch (node->mStroke.cap) {
                case CapFlat:
                    cap = StrokeCap::Butt;
                    break;
                case CapSquare:
                    cap = StrokeCap::Square;
                    break;
                case CapRound:
                    cap = StrokeCap::Round;
                    break;
                default:
                    cap = StrokeCap::Butt;
                    break;
            }
            shape->stroke(cap);

            //Stroke Join
            StrokeJoin join;
            switch (node->mStroke.join) {
                case JoinMiter:
                    join = StrokeJoin::Miter;
                    break;
                case JoinBevel:
                    join = StrokeJoin::Bevel;
                    break;
                case JoinRound:
                    join = StrokeJoin::Round;
                    break;
                default:
                    join = StrokeJoin::Miter;
                    break;
            }
            shape->stroke(join);

            //Stroke Dash
            if (node->mStroke.dashArraySize > 0) shape->stroke(node->mStroke.dashArray, node->mStroke.dashArraySize);
        }

        //2: Fill Method
        switch (node->mBrushType) {
            case BrushSolid:
            {
                if (node->mStroke.enable) shape->stroke(node->mColor.r, node->mColor.g, node->mColor.b, node->mColor.a);
                else shape->fill(node->mColor.r, node->mColor.g, node->mColor.b, node->mColor.a);
            }
            break;
            case BrushGradient:
            {
                Fill* grad = NULL;

                if (node->mGradient.type == GradientLinear) {
                    grad = LinearGradient::gen().release();
                    reinterpret_cast<LinearGradient*>(grad)->linear(node->mGradient.start.x, node->mGradient.start.y, node->mGradient.end.x, node->mGradient.end.y);
                }
                else if (node->mGradient.type == GradientRadial) {
                     grad = RadialGradient::gen().release();
                     reinterpret_cast<RadialGradient*>(grad)->radial(node->mGradient.center.x, node->mGradient.center.y, node->mGradient.cradius);
                }
                else TVGLOG("Lottie", "No reserved gradient type = %d", node->mGradient.type);

                if (grad) {
                    //Gradient Stop
                    Fill::ColorStop *stops = new Fill::ColorStop[node->mGradient.stopCount];

                    if (stops) {
                        for (unsigned int i = 0; i < node->mGradient.stopCount; i++) {
                            stops[i].offset = node->mGradient.stopPtr[i].pos;
                            stops[i].r = node->mGradient.stopPtr[i].r;
                            stops[i].g = node->mGradient.stopPtr[i].g;
                            stops[i].b = node->mGradient.stopPtr[i].b;
                            stops[i].a = node->mGradient.stopPtr[i].a;
                        }
                        grad->colorStops(stops, node->mGradient.stopCount);
                        delete[] stops;
                    }

                    if (node->mStroke.enable) shape->stroke(unique_ptr<Fill>(grad));
                    else shape->fill(unique_ptr<Fill>(grad));
                }
            }
            break;
            default:
                TVGLOG("Lottie", "No reserved brush type = %d", node->mBrushType);
        }

        //3: Fill Rule
        if (node->mFillRule == FillEvenOdd) shape->fill(FillRule::EvenOdd);
        else if (node->mFillRule == FillWinding) shape->fill(FillRule::Winding);

        parent->push(unique_ptr<Shape>(shape));
    }
}


static void _compositeShapeBuildHelper(Paint *_parent, LOTMask *mask, int depth)
{
    const float *data = mask->mPath.ptPtr;
    if (!data) return;

    auto parent = reinterpret_cast<Scene*>(_parent);
    auto shape = Shape::gen().release();

   //Path
    uint32_t cmdCnt = mask->mPath.elmCount;
    uint32_t ptsCnt = mask->mPath.ptCount * sizeof(float) / sizeof(Point);
    PathCommand cmds[cmdCnt];
    Point pts[ptsCnt];

    uint32_t cmd_i = 0, pts_i = 0;
    for (uint32_t i = 0; i < cmdCnt; i++) {
        switch (mask->mPath.elmPtr[i]) {
            case 0:
                cmds[cmd_i++] = PathCommand::MoveTo;
                pts[pts_i++] = (Point){data[0], data[1]};
                data += 2;
                break;
            case 1:
                cmds[cmd_i++] = PathCommand::LineTo;
                pts[pts_i++] = (Point){data[0], data[1]};
                data += 2;
                break;
            case 2:
                cmds[cmd_i++] = PathCommand::CubicTo;
                pts[pts_i++] = (Point){data[0], data[1]};
                pts[pts_i++] = (Point){data[2], data[3]};
                pts[pts_i++] = (Point){data[4], data[5]};
                data += 6;
                break;
            case 3:
                cmds[cmd_i++] = PathCommand::Close;
                break;
            default:
                TVGLOG("Lottie", "No reserved path type = %d", mask->mPath.elmPtr[i]);
                break;
        }
    }
    shape->appendPath(cmds, cmd_i, pts, pts_i);

    //White color and alpha setting
    shape->fill(255, 255, 255, mask->mAlpha);

    parent->push(unique_ptr<Shape>(shape));
}


static Paint* _compositeBuildHelper(Paint *mtarget, LOTMask *masks, unsigned int mask_cnt, int depth)
{
    Scene* msource = Scene::gen().release();
    mtarget->composite(unique_ptr<Paint>(msource), CompositeMethod::AlphaMask);
    mtarget = msource;

    //Make mask layers
    for (unsigned int i = 0; i < mask_cnt; i++) {
        LOTMask *mask = &masks[i];
        _compositeShapeBuildHelper(reinterpret_cast<Paint*>(msource), mask, depth + 1);

#if THORVG_LOG_ENABLED
        for (int i = 0; i < depth; i++) TVGLOG("Lottie", "    ");
        TVGLOG("Lottie", "[mask %03d] mode:%d\n", i, mask->mMode);
#endif
    }
    return mtarget;
}


static void _sceneBuildHelper(Paint *root, const LOTLayerNode *layer, int depth)
{
   Paint *ptree = NULL;

   //Note: We assume that if matte is valid, next layer must be a matte source.
   char matteMode[10]="none"; //temporary, instead of enumulator.
   Paint *mtarget = NULL;
   LOTLayerNode *mlayer = NULL;

   //tvg_paint_set_opacity(root, layer->mAlpha);
   root->opacity(layer->mAlpha);

   //Is this layer a container layer?
   for (unsigned int i = 0; i < layer->mLayerList.size; i++) {
       LOTLayerNode *clayer = layer->mLayerList.ptr[i];

#if THORVG_LOG_ENABLED
       for (int i = 0; i < depth; i++) TVGLOG("Lottie", "    ");
       TVGLOG("Lottie", "[layer %03d] matte:%s keypath:%s%s\n", i, matteMode, clayer->keypath, clayer->mVisible?"":" visible:FALSE");
#endif

       if (!clayer->mVisible) {
           if (ptree && strcmp(matteMode, "none")) ptree->opacity(0);
           strcpy(matteMode,"none");
           mtarget = NULL;

           //If layer has a masking layer, skip it
           if (clayer->mMatte != MatteNone) i++;
           continue;
       }

       //Source Layer
       auto ctree = Scene::gen().release();
       _sceneBuildHelper(ctree, clayer, depth+1);

       if (!strcmp(matteMode, "none")) reinterpret_cast<Scene*>(root)->push(unique_ptr<Paint>((Paint*)ctree));
       else {
           ptree->composite(unique_ptr<Paint>(ctree), CompositeMethod::AlphaMask); //temporary
           mtarget = ctree;
       }

       ptree = ctree;

       //Remap Matte Mode
       switch (clayer->mMatte) {
           case MatteNone:
               strcpy(matteMode, "none");
               //matte_mode = TVG_COMPOSITE_METHOD_NONE;
               break;
           case MatteAlpha:
               strcpy(matteMode, "mask");
               //matte_mode = TVG_COMPOSITE_METHOD_ALPHA_MASK;
               break;
           case MatteAlphaInv:
               strcpy(matteMode, "invmaske");
               //matte_mode = TVG_COMPOSITE_METHOD_INVERSE_ALPHA_MASK;
               break;
           case MatteLuma:
               strcpy(matteMode, "none");
               TVGLOG("Lottie", "TODO: MatteLuma\n");
               break;
           case MatteLumaInv:
               strcpy(matteMode, "none");
               TVGLOG("Lottie", "TODO: MatteLumaInv\n");
               break;
           default:
               strcpy(matteMode, "none");
               break;
       }

       if (clayer->mMaskList.size > 0) {
           mlayer = clayer;
           if (!mtarget) mtarget = ptree;
       }
       else mtarget = NULL;

       //Construct node that have mask.
       if (mlayer && mtarget) ptree = _compositeBuildHelper(mtarget, mlayer->mMaskList.ptr, mlayer->mMaskList.size, depth + 1);
    }

    //Construct drawable nodes.
    if (layer->mNodeList.size > 0) _shapeBuildHelper(root, layer, depth);
}


unique_ptr<Scene> LottieLoader::sceneBuilder(const LOTLayerNode* lotRoot)
{
    auto layer = Scene::gen().release();

    _sceneBuildHelper(layer, lotRoot, 1);

    auto viewBoxClip = Shape::gen();
    viewBoxClip->appendRect(0, 0, w, h, 0, 0);
    viewBoxClip->fill(0, 0, 0, 255);

    auto compositeLayer = Scene::gen();
    compositeLayer->composite(move(viewBoxClip), CompositeMethod::ClipPath);
    compositeLayer->push(unique_ptr<Scene>(layer));

    auto root = Scene::gen().release();
    root->push(move(compositeLayer));
    return unique_ptr<Scene>(root);
}


void LottieLoader::clear()
{
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/


LottieLoader::~LottieLoader()
{
}


bool LottieLoader::header()
{
    return true;
}


bool LottieLoader::open(const string& path)
{
    clear();

    mLottieAnimation = rlottie::Animation::loadFromFile(path.c_str());
    if (!mLottieAnimation) {
        TVGERR("Lottie", "load failed file %s\n", path.c_str());
        return false;
    }

    size_t width, height;
    width = height = 0;
    mLottieAnimation->size(width, height);
    if (w == 0) w = static_cast<float>(width);
    if (h == 0) h = static_cast<float>(height);

    totalFrame = mLottieAnimation->totalFrame();

    return header();
}


bool LottieLoader::open(const char* data, uint32_t size, bool copy)
{
    return false;
}


bool LottieLoader::read()
{
    if (/*!decoder || */w <= 0 || h <= 0) return false;

    TaskScheduler::request(this);

    return true;
}


bool LottieLoader::close()
{
    this->done();
    clear();
    return true;
}


bool LottieLoader::resize(Paint* paint, float w, float h)
{
    if (!paint) return false;

    auto sx = w / this->w;
    auto sy = h / this->h;

    if (preserveAspect) {
        //Scale
        auto scale = sx < sy ? sx : sy;
        paint->scale(scale);
        //Align
        auto tx = 0.0f;
        auto ty = 0.0f;
        auto tw = this->w * scale;
        auto th = this->h * scale;
        if (tw > th) ty -= (h - th) * 0.5f;
        else tx -= (w - tw) * 0.5f;
        paint->translate(-tx, -ty);
    } else {
        //Align
        auto tx = 0.0f;
        auto ty = 0.0f;
        auto tw = this->w * sx;
        auto th = this->h * sy;
        if (tw > th) ty -= (h - th) * 0.5f;
        else tx -= (w - tw) * 0.5f;

        Matrix m = {sx, 0, -tx, 0, sy, -ty, 0, 0, 1};
        paint->transform(m);
    }
    return true;
}


void LottieLoader::run(unsigned tid)
{
    const LOTLayerNode* lotRoot = mLottieAnimation->renderTree(frame, static_cast<size_t>(w), static_cast<size_t>(h));

    root = sceneBuilder(lotRoot);
}


unique_ptr<Paint> LottieLoader::paint()
{
    this->done();
    if (root) return move(root);
    else return nullptr;
}
