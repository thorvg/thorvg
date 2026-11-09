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

#include "tvgTvgParser.h"
#include "tvgTvgBuilder.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

void TvgBuilder::build(TvgShape* shape, float progress)
{
    if (shape->statical) return;
    shape->build(progress);
}

void TvgBuilder::build(TvgScene* scene, float progress)
{
    // out of frame
    if (progress < scene->i[0] || progress >= scene->i[1]) {
        scene->obj->visible(false);
        return;
    }

    for (auto c : scene->scene()->paints()) {
        switch (c->type()) {
            case Type::Scene: return build((TvgScene*)(c->data), progress);
            case Type::Shape: return build((TvgShape*)(c->data), scene->map(progress));
            case Type::Picture: return;
            case Type::Text: return;
            default: break;
        }
    }

    scene->obj->visible(true);
}

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

TvgBuilder::~TvgBuilder()
{
    delete (parser);
    delete (main);
}

void TvgBuilder::run()
{
    if (parser) {
        main = parser->parse();
        delete (parser);
        parser = nullptr;
    }

    if (main) build(main, progress);
}

Result TvgBuilder::init(const char* data, uint32_t size, uint16_t& ver, float& w, float& h)
{
    parser = new TvgParser(data, size);
    return parser->header(ver, w, h, duration);
}

Paint* TvgBuilder::paint()
{
    return main ? main->obj : nullptr;
}
