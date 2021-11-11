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

#include <fstream>
#include <memory.h>
#include "tvgLoader.h"
#include "tvgLottieLoader.h"

//temporary
//#define DEBUG 1


/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

static void _shapeBuildHelper(tvg::Paint *_parent, const LOTLayerNode *layer, int depth)
{
   if (!_parent) return;

   auto parent = reinterpret_cast<tvg::Scene*>(_parent);

   parent->reserve(layer->mNodeList.size);

   for (unsigned int i = 0; i < layer->mNodeList.size; i++)
     {
        LOTNode *node = layer->mNodeList.ptr[i];
        if (!node) continue;

#if DEBUG
        for (int i = 0; i < depth; i++) printf("    ");
        printf("[node %03d] type:%s keypath:%s\n", i, (node->mImageInfo.data)?"image":"shape", node->keypath);
#endif

        //Image object
        if (node->mImageInfo.data)
          {
             //Tvg_Paint* picture = tvg_picture_new();
             auto picture = tvg::Picture::gen().release();
             tvg::Matrix matrix = {
                node->mImageInfo.mMatrix.m11,  node->mImageInfo.mMatrix.m12, node->mImageInfo.mMatrix.m13,
                node->mImageInfo.mMatrix.m21,  node->mImageInfo.mMatrix.m22, node->mImageInfo.mMatrix.m23,
                node->mImageInfo.mMatrix.m31,  node->mImageInfo.mMatrix.m32, node->mImageInfo.mMatrix.m33
             };
             picture->transform(matrix);
             picture->load((uint32_t *)node->mImageInfo.data, node->mImageInfo.width, node->mImageInfo.height, false);
             picture->opacity(node->mImageInfo.mAlpha);
             parent->push(unique_ptr<tvg::Picture>(picture));
             continue;
          }

        const float *data = node->mPath.ptPtr;
        if (!data) continue;

        auto shape = tvg::Shape::gen().release();

        //0: Path
        uint32_t cmdCnt = node->mPath.elmCount;
        uint32_t ptsCnt = node->mPath.ptCount * sizeof(float) / sizeof(tvg::Point);
        tvg::PathCommand cmds[cmdCnt];
        tvg::Point pts[ptsCnt];

        uint32_t cmd_i = 0, pts_i = 0;
        for (uint32_t i = 0; i < cmdCnt; i++)
          {
            switch (node->mPath.elmPtr[i])
              {
               case 0:
                  cmds[cmd_i++] = tvg::PathCommand::MoveTo;
                  pts[pts_i++] = (tvg::Point){data[0], data[1]};
                  data += 2;
                  break;
               case 1:
                  cmds[cmd_i++] = tvg::PathCommand::LineTo;
                  pts[pts_i++] = (tvg::Point){data[0], data[1]};
                  data += 2;
                  break;
               case 2:
                  cmds[cmd_i++] = tvg::PathCommand::CubicTo;
                  pts[pts_i++] = (tvg::Point){data[0], data[1]};
                  pts[pts_i++] = (tvg::Point){data[2], data[3]};
                  pts[pts_i++] = (tvg::Point){data[4], data[5]};
                  data += 6;
                  break;
               case 3:
                  cmds[cmd_i++] = tvg::PathCommand::Close;
                  break;
               default:
                  printf("No reserved path type = %d", node->mPath.elmPtr[i]);
                  break;
              }
          }
        shape->appendPath(cmds, cmd_i, pts, pts_i);

        //1: Stroke
        if (node->mStroke.enable)
          {
             //Stroke Width
             shape->stroke(node->mStroke.width);

             //Stroke Cap
             tvg::StrokeCap cap;
             switch (node->mStroke.cap)
               {
                case CapFlat: cap = tvg::StrokeCap::Butt; break;
                case CapSquare: cap = tvg::StrokeCap::Square; break;
                case CapRound: cap = tvg::StrokeCap::Round; break;
                default: cap = tvg::StrokeCap::Butt; break;
               }
             shape->stroke(cap);

             //Stroke Join
             tvg::StrokeJoin join;
             switch (node->mStroke.join)
               {
                case JoinMiter: join = tvg::StrokeJoin::Miter; break;
                case JoinBevel: join = tvg::StrokeJoin::Bevel; break;
                case JoinRound: join = tvg::StrokeJoin::Round; break;
                default: join = tvg::StrokeJoin::Miter; break;
               }
             shape->stroke(join);

             //Stroke Dash
             if (node->mStroke.dashArraySize > 0)
               {
                  shape->stroke(node->mStroke.dashArray, node->mStroke.dashArraySize);
               }
          }

        //2: Fill Method
        switch (node->mBrushType)
          {
           case BrushSolid:
             {
                if (node->mStroke.enable)
                 shape->stroke(node->mColor.r, node->mColor.g, node->mColor.b, node->mColor.a);
                else
                  shape->fill(node->mColor.r, node->mColor.g, node->mColor.b, node->mColor.a);
             }
             break;
           case BrushGradient:
             {
                tvg::Fill* grad = NULL;

                if (node->mGradient.type == GradientLinear)
                  {
                     grad = tvg::LinearGradient::gen().release();
                     reinterpret_cast<tvg::LinearGradient*>(grad)->linear(node->mGradient.start.x, node->mGradient.start.y, node->mGradient.end.x, node->mGradient.end.y);
                  }
                else if (node->mGradient.type == GradientRadial)
                  {
                     grad = tvg::RadialGradient::gen().release();
                     reinterpret_cast<tvg::RadialGradient*>(grad)->radial(node->mGradient.center.x, node->mGradient.center.y, node->mGradient.cradius);
                  }
                else
                  printf("No reserved gradient type = %d", node->mGradient.type);

                if (grad)
                  {
                     //Gradient Stop
                     tvg::Fill::ColorStop *stops = new tvg::Fill::ColorStop[node->mGradient.stopCount];

                     if (stops)
                       {
                          for (unsigned int i = 0; i < node->mGradient.stopCount; i++)
                            {
                               stops[i].offset = node->mGradient.stopPtr[i].pos;
                               stops[i].r = node->mGradient.stopPtr[i].r;
                               stops[i].g = node->mGradient.stopPtr[i].g;
                               stops[i].b = node->mGradient.stopPtr[i].b;
                               stops[i].a = node->mGradient.stopPtr[i].a;
                            }
                          grad->colorStops(stops, node->mGradient.stopCount);
                          delete[] stops;
                       }

                     if (node->mStroke.enable)
                       {
                          if (node->mGradient.type == GradientLinear)
                             shape->stroke(unique_ptr<tvg::Fill>(grad));
                          else if (node->mGradient.type == GradientRadial)
                             shape->stroke(unique_ptr<tvg::Fill>(grad));
                       }
                     else
                       {
                          if (node->mGradient.type == GradientLinear)
                             shape->fill(unique_ptr<tvg::Fill>(grad));
                          else if (node->mGradient.type == GradientRadial)
                             shape->fill(unique_ptr<tvg::Fill>(grad));
                       }
                  }
             }
             break;
           default:
              printf("No reserved brush type = %d", node->mBrushType);
          }

        //3: Fill Rule
        if (node->mFillRule == FillEvenOdd)
          shape->fill(tvg::FillRule::EvenOdd);
        else if (node->mFillRule == FillWinding)
          shape->fill(tvg::FillRule::Winding);

        parent->push(unique_ptr<tvg::Shape>(shape));
     }
}


static void _sceneBuildHelper(tvg::Paint *root, const LOTLayerNode *layer, int depth)
{
   tvg::Paint *ptree = NULL;

   //Note: We assume that if matte is valid, next layer must be a matte source.
   char matteMode[10]="none"; //temporary, instead of enumulator.
   tvg::Paint *mtarget = NULL;
   LOTLayerNode *mlayer = NULL;

   //tvg_paint_set_opacity(root, layer->mAlpha);
   root->opacity(layer->mAlpha);

   //Is this layer a container layer?
   for (unsigned int i = 0; i < layer->mLayerList.size; i++)
     {
        LOTLayerNode *clayer = layer->mLayerList.ptr[i];

#if DEBUG
        for (int i = 0; i < depth; i++) printf("    ");
        printf("[layer %03d] matte:%s keypath:%s%s\n", i, matteMode, clayer->keypath, clayer->mVisible?"":" visible:FALSE");
#endif

        if (!clayer->mVisible)
          {
             if (ptree && strcmp(matteMode, "none"))
               ptree->opacity(0);
             strcpy(matteMode,"none");
             mtarget = NULL;

             //If layer has a masking layer, skip it
             if (clayer->mMatte != MatteNone) i++;
             continue;
          }

        //Source Layer
        auto ctree = tvg::Scene::gen().release();
        _sceneBuildHelper(ctree, clayer, depth+1);

        if (!strcmp(matteMode, "none"))
          {
             reinterpret_cast<tvg::Scene*>(root)->push(unique_ptr<Paint>((Paint*)ctree));
          }
        else
          {
             ptree->composite(unique_ptr<Paint>(ctree), tvg::CompositeMethod::AlphaMask); //temporary
             mtarget = ctree;
          }

        ptree = ctree;

        //Remap Matte Mode
        switch (clayer->mMatte)
          {
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
              printf("TODO: MatteLuma\n");
              break;
           case MatteLumaInv:
              strcpy(matteMode, "none");
              printf("TODO: MatteLumaInv\n");
              break;
           default:
              strcpy(matteMode, "none");
              break;
          }

        if (clayer->mMaskList.size > 0)
          {
             mlayer = clayer;
             if (!mtarget) mtarget = ptree;
          }
        else
           mtarget = NULL;

        //Construct node that have mask.
//        if (mlayer && mtarget)
          //ptree = _compositeBuildHelper(mtarget, mlayer->mMaskList.ptr, mlayer->mMaskList.size, depth + 1);
     }

   //Construct drawable nodes.
   if (layer->mNodeList.size > 0)
     _shapeBuildHelper(root, layer, depth);
}


unique_ptr<Scene> LottieLoader::sceneBuilder(const LOTLayerNode* lotRoot)
{
   auto root = tvg::Scene::gen().release();

   _sceneBuildHelper(root, lotRoot, 1);
   return unique_ptr<tvg::Scene>(root);

}


void LottieLoader::clear()
{
    data = nullptr;
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

    ifstream f;
    f.open(path);

    if (!f.is_open()) return false;

    svgPath = path;
    getline(f, filePath, '\0');
    f.close();

    if (filePath.empty()) return false;

    content = filePath.c_str();
    size = filePath.size();

    mLottieAnimation = rlottie::Animation::loadFromFile(path.c_str());
    if (!mLottieAnimation) {
        printf("load failed file %s\n", path.c_str());
        return false;
    }

    size_t width, height;
    width = height = 0;
    mLottieAnimation->size(width, height);
    w = static_cast<float>(width);
    h = static_cast<float>(height);

    totalFrame = mLottieAnimation->totalFrame();
    duration = (float)mLottieAnimation->duration();

    //printf("load file %s %f %f %ld\n", path.c_str(), w, h, mTotalFrame);

    return header();
}


bool LottieLoader::open(const char* data, uint32_t size, bool copy)
{
    clear();

    if (copy) {
        content = (char*)malloc(size);
        if (!content) return false;
        memcpy((char*)content, data, size);
    } else content = data;

    this->size = size;
    this->copy = copy;

    return header();
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
