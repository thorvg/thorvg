/*
 * Copyright (c) 2021 - 2026 ThorVG project. All rights reserved.

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

#include <thorvg.h>
#include <vector>
#include "config.h"
#include "testFramework.h"

using namespace tvg;
using namespace std;

TEST_CASE("Scene Creation", "[tvgScene]")
{
    auto scene = Scene::gen();
    REQUIRE(scene);

    REQUIRE(scene->type() == Type::Scene);

    Paint::rel(scene);
}

TEST_CASE("Pushing Paints Into Scene", "[tvgScene]")
{
    auto scene = Scene::gen();
    REQUIRE(scene);
    REQUIRE(scene->parent() == nullptr);

    Paint* paints[3];

    //Pushing Paints
    paints[0] = Shape::gen();
    paints[0]->ref();
    REQUIRE(paints[0]->parent() == nullptr);
    REQUIRE(scene->add(paints[0]) == Result::Success);
    REQUIRE(paints[0]->parent() == scene);

    paints[1] = Picture::gen();
    paints[1]->ref();
    REQUIRE(paints[1]->parent() == nullptr);
    REQUIRE(scene->add(paints[1]) == Result::Success);
    REQUIRE(paints[1]->parent() == scene);

    paints[2] = Picture::gen();
    paints[2]->ref();
    REQUIRE(paints[2]->parent() == nullptr);
    REQUIRE(scene->add(paints[2]) == Result::Success);
    REQUIRE(paints[2]->parent() == scene);

    //Pushing Null Pointer
    REQUIRE(scene->add(nullptr) == Result::InvalidArguments);

    //Pushing Invalid Paint
    REQUIRE(scene->add(nullptr) == Result::InvalidArguments);

    REQUIRE(scene->remove(paints[0]) == Result::Success);
    REQUIRE(scene->remove(paints[0]) == Result::InsufficientCondition);
    REQUIRE(scene->add(paints[0], paints[1]) == Result::Success);
    REQUIRE(scene->remove(paints[1]) == Result::Success);
    REQUIRE(scene->remove(paints[1]) == Result::InsufficientCondition);
    REQUIRE(scene->remove(paints[2]) == Result::Success);
    REQUIRE(scene->remove(paints[0]) == Result::Success);

    paints[0]->unref();
    paints[1]->unref();
    paints[2]->unref();

    Paint::rel(scene);
}

TEST_CASE("Scene Clear", "[tvgScene]")
{
    auto scene = Scene::gen();
    REQUIRE(scene);

    REQUIRE(scene->add(Shape::gen()) == Result::Success);
    REQUIRE(scene->remove() == Result::Success);

    Paint::rel(scene);
}

TEST_CASE("Scene Clear And Reuse Shape", "[tvgScene]")
{
    REQUIRE(Initializer::init() == Result::Success);
    {
        auto canvas = unique_ptr<SwCanvas>(SwCanvas::gen());
        uint32_t buffer[100*100] = {};
        canvas->target(buffer, 100, 100, 100, ColorSpace::ARGB8888);

        auto scene = Scene::gen();
        REQUIRE(scene);

        auto shape = Shape::gen();
        REQUIRE(shape);
        REQUIRE(shape->ref() == 1);

        REQUIRE(scene->add(shape) == Result::Success);
        REQUIRE(canvas->add(scene) == Result::Success);
        REQUIRE(canvas->update() == Result::Success);

        //No deallocate shape.
        REQUIRE(scene->remove() == Result::Success);

        //Reuse shape.
        REQUIRE(scene->add(shape) == Result::Success);
        REQUIRE(shape->unref() == 1); //The scene still holds 1.
    }
    REQUIRE(Initializer::term() == Result::Success);
}

TEST_CASE("Scene Effects", "[tvgScene]")
{
    REQUIRE(Initializer::init() == Result::Success);
    {
        auto canvas = unique_ptr<SwCanvas>(SwCanvas::gen());
        REQUIRE(canvas);

        uint32_t buffer[100*100] = {};
        REQUIRE(canvas->target(buffer, 100, 100, 100, ColorSpace::ARGB8888) == Result::Success);

        auto shape = Shape::gen();
        REQUIRE(shape);
        REQUIRE(shape->appendCircle(50, 50, 30, 30) == Result::Success);
        REQUIRE(shape->fill(0, 255, 0, 255) == Result::Success);

        auto scene = Scene::gen();
        REQUIRE(scene);
        REQUIRE(scene->add(shape) == Result::Success);

        auto picture = tvg::Picture::gen();
        picture->load(TEST_DIR"/test0.svg");

        scene->add(picture);
        REQUIRE(canvas->add(scene) == Result::Success);

        REQUIRE(scene->add(SceneEffect::Clear) == Result::Success);
        REQUIRE(scene->add(SceneEffect::GaussianBlur, 1.5, 0, 0, 75) == Result::Success);
        REQUIRE(canvas->update() == Result::Success);
        REQUIRE(canvas->draw() == Result::Success);
        REQUIRE(canvas->sync() == Result::Success);

        REQUIRE(scene->add(SceneEffect::Clear) == Result::Success);
        REQUIRE(scene->add(SceneEffect::MotionBlur, 10.0, 0.0, 75) == Result::Success);
        REQUIRE(canvas->update() == Result::Success);
        REQUIRE(canvas->draw() == Result::Success);
        REQUIRE(canvas->sync() == Result::Success);

        REQUIRE(scene->add(SceneEffect::Clear) == Result::Success);
        REQUIRE(scene->add(SceneEffect::MotionBlur, 10.0, 45.0, 75) == Result::Success);
        REQUIRE(canvas->update() == Result::Success);
        REQUIRE(canvas->draw() == Result::Success);
        REQUIRE(canvas->sync() == Result::Success);

        REQUIRE(scene->add(SceneEffect::Clear) == Result::Success);
        REQUIRE(scene->add(SceneEffect::MotionBlur, 10.0, 90.0, 75) == Result::Success);
        REQUIRE(canvas->update() == Result::Success);
        REQUIRE(canvas->draw() == Result::Success);
        REQUIRE(canvas->sync() == Result::Success);

        REQUIRE(scene->add(SceneEffect::Clear) == Result::Success);
        REQUIRE(scene->add(SceneEffect::MotionBlur, 0.0, 45.0, 75) == Result::Success);
        REQUIRE(canvas->update() == Result::Success);
        REQUIRE(canvas->draw() == Result::Success);
        REQUIRE(canvas->sync() == Result::Success);

        REQUIRE(scene->add(SceneEffect::Clear) == Result::Success);
        REQUIRE(scene->add(SceneEffect::DropShadow, 128, 128, 128, 200, 45.0, 5.0, 2.0, 60) == Result::Success);
        REQUIRE(canvas->update() == Result::Success);
        REQUIRE(canvas->draw() == Result::Success);
        REQUIRE(canvas->sync() == Result::Success);

        REQUIRE(scene->add(SceneEffect::Clear) == Result::Success);
        REQUIRE(scene->add(SceneEffect::Fill, 255, 0, 0, 128) == Result::Success);
        REQUIRE(canvas->update() == Result::Success);
        REQUIRE(canvas->draw() == Result::Success);
        REQUIRE(canvas->sync() == Result::Success);

        REQUIRE(scene->add(SceneEffect::Clear) == Result::Success);
        REQUIRE(scene->add(SceneEffect::Tint, 0, 0, 0, 255, 255, 255, 50.0) == Result::Success);
        REQUIRE(canvas->update() == Result::Success);
        REQUIRE(canvas->draw() == Result::Success);
        REQUIRE(canvas->sync() == Result::Success);

        REQUIRE(scene->add(SceneEffect::Clear) == Result::Success);
        REQUIRE(scene->add(SceneEffect::Tritone, 0, 0, 0, 128, 128, 128, 255, 255, 255, 128) == Result::Success);
        REQUIRE(canvas->update() == Result::Success);
        REQUIRE(canvas->draw() == Result::Success);
        REQUIRE(canvas->sync() == Result::Success);

        REQUIRE(scene->add(SceneEffect::GaussianBlur, 1.5, 0, 0, 75) == Result::Success);
        REQUIRE(scene->add(SceneEffect::MotionBlur, 10.0, 45.0, 75) == Result::Success);
        REQUIRE(scene->add(SceneEffect::DropShadow, 128, 128, 128, 200, 45.0, 5.0, 2.0, 60) == Result::Success);

        REQUIRE(canvas->add(scene->duplicate()) == Result::Success);
        REQUIRE(canvas->update() == Result::Success);
        REQUIRE(canvas->draw() == Result::Success);
        REQUIRE(canvas->sync() == Result::Success);
    }
    REQUIRE(Initializer::term() == Result::Success);
}

TEST_CASE("Parents Update", "[tvgScene]")
{
    REQUIRE(Initializer::init() == Result::Success);
    {
        auto parent = Scene::gen();
        REQUIRE(parent);

        auto child = Shape::gen();
        REQUIRE(child);
        REQUIRE(child->ref() == 1);

        auto maskA = Shape::gen();
        REQUIRE(maskA);
        REQUIRE(maskA->ref() == 1);

        auto maskB = Shape::gen();
        REQUIRE(maskB);

        auto clip = Shape::gen();
        REQUIRE(clip);

        REQUIRE(maskA->mask(maskB, MaskMethod::Alpha) == Result::Success);
        REQUIRE(child->mask(maskA, MaskMethod::Alpha) == Result::Success);
        REQUIRE(child->clip(clip) == Result::Success);
        REQUIRE(parent->add(child) == Result::Success);

        REQUIRE(maskB->parent() == parent);
        REQUIRE(maskA->parent() == parent);
        REQUIRE(clip->parent() == parent);
        REQUIRE(child->parent() == parent);

        Paint::rel(parent);

        REQUIRE(maskB->parent() == nullptr);
        REQUIRE(maskA->parent() == nullptr);
        REQUIRE(clip->parent() == nullptr);
        REQUIRE(child->parent() == nullptr);

        REQUIRE(maskA->unref() == 1);
        REQUIRE(child->unref() == 0);
    }
    REQUIRE(Initializer::term() == Result::Success);
}

#ifdef THORVG_CPU_ENGINE_SUPPORT
#ifdef THORVG_PARTIAL_RENDER_SUPPORT
namespace
{
constexpr uint32_t UW = 96, UH = 64;

Shape* disc(float x, float y, float r, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha)
{
    auto shape = Shape::gen();
    REQUIRE(shape->appendCircle(x, y, r, r) == Result::Success);
    REQUIRE(shape->fill(red, green, blue, alpha) == Result::Success);
    return shape;
}

// The reference: the same painter order drawn in full by a fresh canvas.
vector<uint32_t> freshDiscs(bool withBelowAndD)
{
    vector<uint32_t> px(UW * UH, 0);
    auto canvas = SwCanvas::gen(EngineOption::SmartRender);
    REQUIRE(canvas->target(px.data(), UW, UW, UH, ColorSpace::ABGR8888) == Result::Success);
    REQUIRE(canvas->add(disc(30, 30, 13, 220, 60, 60, 150)) == Result::Success);
    if (withBelowAndD) REQUIRE(canvas->add(disc(20, 50, 9, 240, 200, 40, 200)) == Result::Success);
    REQUIRE(canvas->update() == Result::Success);
    REQUIRE(canvas->draw(true) == Result::Success);
    REQUIRE(canvas->sync() == Result::Success);
    delete canvas;
    return px;
}

bool covered(const vector<Region>& regions, int32_t x, int32_t y)
{
    for (auto& r : regions) {
        if (x >= r.xMin && x < r.xMax && y >= r.yMin && y < r.yMax) return true;
    }
    return false;
}
}  // namespace

// A drawn shape that nothing else references is destroyed when its scene removes it. Its old area must still be damaged,
// or the next partial frame keeps its pixels. 0: Scene::remove(paint), 1: Scene::remove() (all paints)
TEST_CASE("Removing a drawn paint from a scene clears its old area", "[tvgScene][partial]")
{
    auto mode = GENERATE(0, 1);

    REQUIRE(Initializer::init(0) == Result::Success);
    {
        vector<uint32_t> target(UW * UH, 0);
        auto canvas = SwCanvas::gen(EngineOption::SmartRender);
        REQUIRE(canvas->target(target.data(), UW, UW, UH, ColorSpace::ABGR8888) == Result::Success);
        auto scene = Scene::gen();
        auto removed = disc(70, 22, 13, 60, 200, 90, 110);
        REQUIRE(scene->add(disc(48, 34, 13, 60, 90, 230, 120)) == Result::Success);
        REQUIRE(scene->add(removed) == Result::Success);
        REQUIRE(canvas->add(disc(30, 30, 13, 220, 60, 60, 150)) == Result::Success);
        REQUIRE(canvas->add(scene) == Result::Success);
        REQUIRE(canvas->update() == Result::Success);
        REQUIRE(canvas->draw(false) == Result::Success);
        REQUIRE(canvas->sync() == Result::Success);

        if (mode == 0) REQUIRE(scene->remove(removed) == Result::Success);
        else REQUIRE(scene->remove() == Result::Success);

        REQUIRE(canvas->update() == Result::Success);
        REQUIRE(canvas->draw(false) == Result::Success);
        REQUIRE(canvas->sync() == Result::Success);

        // the reference: what remains, drawn in full by a fresh canvas
        vector<uint32_t> expected(UW * UH, 0);
        auto ref = SwCanvas::gen(EngineOption::SmartRender);
        REQUIRE(ref->target(expected.data(), UW, UW, UH, ColorSpace::ABGR8888) == Result::Success);
        REQUIRE(ref->add(disc(30, 30, 13, 220, 60, 60, 150)) == Result::Success);
        if (mode == 0) {
            auto kept = Scene::gen();
            REQUIRE(kept->add(disc(48, 34, 13, 60, 90, 230, 120)) == Result::Success);
            REQUIRE(ref->add(kept) == Result::Success);
        }
        REQUIRE(ref->update() == Result::Success);
        REQUIRE(ref->draw(true) == Result::Success);
        REQUIRE(ref->sync() == Result::Success);
        delete ref;

        REQUIRE(target == expected);
        delete canvas;
    }
    REQUIRE(Initializer::term() == Result::Success);
}

// A scene that has been drawn and then given paints that were never updated. Removing it (or its contents) must not
// dereference the missing render data, must still damage the area of the drawn paints, and must leave the next
// partial frame equal to a fresh render.
TEST_CASE("Removing never-updated paints keeps removal damage", "[tvgScene][partial]")
{
    // 0: canvas->remove(scene), 1: canvas->remove(), 2: canvas->remove(scene) with a nested undrawn leaf in a drawn scene
    auto mode = GENERATE(0, 1, 2);

    REQUIRE(Initializer::init(0) == Result::Success);
    {
        vector<uint32_t> target(UW * UH, 0);
        auto canvas = SwCanvas::gen(EngineOption::SmartRender);
        REQUIRE(canvas->target(target.data(), UW, UW, UH, ColorSpace::ABGR8888) == Result::Success);

        auto scene = Scene::gen();
        auto inner = Scene::gen();
        REQUIRE(inner->add(disc(70, 22, 13, 60, 200, 90, 110)) == Result::Success);
        REQUIRE(scene->add(disc(48, 34, 13, 60, 90, 230, 120)) == Result::Success);
        REQUIRE(scene->add(inner) == Result::Success);
        REQUIRE(canvas->add(disc(30, 30, 13, 220, 60, 60, 150)) == Result::Success);
        REQUIRE(canvas->add(scene) == Result::Success);
        REQUIRE(canvas->update() == Result::Success);
        REQUIRE(canvas->draw(false) == Result::Success);
        REQUIRE(canvas->sync() == Result::Success);

        // never updated: a plain shape, and a scene two levels deep
        auto deep = Scene::gen();
        auto deeper = Scene::gen();
        REQUIRE(deeper->add(disc(15, 12, 8, 10, 10, 10, 255)) == Result::Success);
        REQUIRE(deep->add(deeper) == Result::Success);
        REQUIRE(scene->add(disc(15, 12, 8, 10, 10, 10, 255)) == Result::Success);
        REQUIRE(scene->add(deep) == Result::Success);
        if (mode == 2) REQUIRE(inner->add(disc(15, 12, 8, 10, 10, 10, 255)) == Result::Success);

        if (mode == 1) REQUIRE(canvas->remove() == Result::Success);
        else REQUIRE(canvas->remove(scene) == Result::Success);
        REQUIRE(canvas->add(disc(20, 50, 9, 240, 200, 40, 200)) == Result::Success);

        REQUIRE(canvas->update() == Result::Success);
        REQUIRE(canvas->draw(false) == Result::Success);
        auto count = canvas->dirtyRegions();
        vector<Region> regions(count);
        REQUIRE(count > 0);
        REQUIRE(canvas->dirtyRegions(regions.data(), count) == Result::Success);
        REQUIRE(canvas->sync() == Result::Success);

        // the removed, drawn paints' old area is damaged
        REQUIRE(covered(regions, 48, 34));
        REQUIRE(covered(regions, 70, 22));
        if (mode == 1) REQUIRE(covered(regions, 30, 30));
        // and the undrawn paints' would-be area is not
        REQUIRE(!covered(regions, 15, 12));

        auto expected = freshDiscs(true);
        if (mode == 1) {  // only the disc added after the clear remains
            expected.assign(UW * UH, 0);
            auto ref = SwCanvas::gen(EngineOption::SmartRender);
            REQUIRE(ref->target(expected.data(), UW, UW, UH, ColorSpace::ABGR8888) == Result::Success);
            REQUIRE(ref->add(disc(20, 50, 9, 240, 200, 40, 200)) == Result::Success);
            REQUIRE(ref->update() == Result::Success);
            REQUIRE(ref->draw(true) == Result::Success);
            REQUIRE(ref->sync() == Result::Success);
            delete ref;
        }
        REQUIRE(target == expected);
        delete canvas;
    }
    REQUIRE(Initializer::term() == Result::Success);
}

TEST_CASE("Removing a never-updated paint directly leaves a drawn scene untouched", "[tvgScene][partial]")
{
    REQUIRE(Initializer::init(0) == Result::Success);
    {
        vector<uint32_t> target(UW * UH, 0);
        auto canvas = SwCanvas::gen(EngineOption::SmartRender);
        REQUIRE(canvas->target(target.data(), UW, UW, UH, ColorSpace::ABGR8888) == Result::Success);
        auto scene = Scene::gen();
        auto kept = disc(48, 34, 13, 60, 90, 230, 120);
        REQUIRE(scene->add(kept) == Result::Success);
        REQUIRE(canvas->add(disc(30, 30, 13, 220, 60, 60, 150)) == Result::Success);
        REQUIRE(canvas->add(scene) == Result::Success);
        REQUIRE(canvas->update() == Result::Success);
        REQUIRE(canvas->draw(false) == Result::Success);
        REQUIRE(canvas->sync() == Result::Success);
        auto before = target;

        auto fresh = disc(15, 12, 8, 10, 10, 10, 255);
        auto freshScene = Scene::gen();
        REQUIRE(freshScene->add(disc(15, 12, 8, 10, 10, 10, 255)) == Result::Success);
        REQUIRE(scene->add(fresh) == Result::Success);
        REQUIRE(scene->add(freshScene) == Result::Success);
        REQUIRE(scene->remove(fresh) == Result::Success);
        REQUIRE(scene->remove(freshScene) == Result::Success);

        REQUIRE(canvas->update() == Result::Success);
        REQUIRE(canvas->draw(false) == Result::Success);
        REQUIRE(canvas->dirtyRegions() == 0);
        REQUIRE(canvas->sync() == Result::Success);
        REQUIRE(target == before);

        // destroying the canvas with a never-updated paint in a drawn scene is the same path (SceneImpl::clearPaints)
        REQUIRE(scene->add(disc(15, 12, 8, 10, 10, 10, 255)) == Result::Success);
        delete canvas;
    }
    REQUIRE(Initializer::term() == Result::Success);
}

// A drawn scene whose only drawn child is removed and which then receives only never-updated paints: none of its children
// can be bounded, so its bounds are empty. Removing it damages nothing, and the earlier removal's damage is unaffected.
TEST_CASE("Removing a scene whose remaining children were all never updated", "[tvgScene][partial]")
{
    REQUIRE(Initializer::init(0) == Result::Success);
    {
        vector<uint32_t> target(UW * UH, 0);
        auto canvas = SwCanvas::gen(EngineOption::SmartRender);
        REQUIRE(canvas->target(target.data(), UW, UW, UH, ColorSpace::ABGR8888) == Result::Success);
        auto scene = Scene::gen();
        auto drawn = disc(48, 34, 13, 60, 90, 230, 120);
        REQUIRE(scene->add(drawn) == Result::Success);
        REQUIRE(canvas->add(disc(30, 30, 13, 220, 60, 60, 150)) == Result::Success);
        REQUIRE(canvas->add(scene) == Result::Success);
        REQUIRE(canvas->update() == Result::Success);
        REQUIRE(canvas->draw(false) == Result::Success);
        REQUIRE(canvas->sync() == Result::Success);

        // the scene's bounds were never queried since its update, so they are still pending
        REQUIRE(scene->remove(drawn) == Result::Success);
        auto inner = Scene::gen();
        REQUIRE(inner->add(disc(15, 12, 8, 10, 10, 10, 255)) == Result::Success);
        REQUIRE(scene->add(disc(15, 12, 8, 10, 10, 10, 255)) == Result::Success);
        REQUIRE(scene->add(inner) == Result::Success);
        REQUIRE(canvas->remove(scene) == Result::Success);
        REQUIRE(canvas->add(disc(20, 50, 9, 240, 200, 40, 200)) == Result::Success);

        REQUIRE(canvas->update() == Result::Success);
        REQUIRE(canvas->draw(false) == Result::Success);
        auto count = canvas->dirtyRegions();
        vector<Region> regions(count);
        REQUIRE(count > 0);
        REQUIRE(canvas->dirtyRegions(regions.data(), count) == Result::Success);
        REQUIRE(canvas->sync() == Result::Success);

        REQUIRE(covered(regions, 48, 34));   // the removed disc's old area
        REQUIRE(!covered(regions, 15, 12));  // the undrawn paints' would-be area
        for (auto& r : regions) {            // no unbounded region from an empty union
            REQUIRE(r.xMin >= 0);
            REQUIRE(r.yMin >= 0);
            REQUIRE(r.xMax <= int32_t(UW));
            REQUIRE(r.yMax <= int32_t(UH));
        }
        REQUIRE(target == freshDiscs(true));
        delete canvas;
    }
    REQUIRE(Initializer::term() == Result::Success);
}
#endif
#endif
