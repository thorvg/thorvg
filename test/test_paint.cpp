#include <gtest/gtest.h>
#include <iostream>
#include <thread>
#include <thorvg.h>

class PaintTest : public ::testing::Test {
public:
    void SetUp() {
        auto threads = std::thread::hardware_concurrency();
        //Initialize ThorVG Engine
        if (tvg::Initializer::init(tvgEngine, threads) == tvg::Result::Success) {
            swCanvas = tvg::SwCanvas::gen();

            scene = tvg::Scene::gen();

            shape = tvg::Shape::gen();
        }
    }
    void TearDown() {

        //Terminate ThorVG Engine
        tvg::Initializer::term(tvgEngine);
    }
public:
    std::unique_ptr<tvg::SwCanvas> swCanvas;
    std::unique_ptr<tvg::Scene> scene;
    std::unique_ptr<tvg::Shape> shape;
    tvg::CanvasEngine tvgEngine = tvg::CanvasEngine::Sw;
};

TEST_F(PaintTest, GenerateShape) {
    ASSERT_TRUE(swCanvas != nullptr);
    ASSERT_TRUE(shape != nullptr);
}

TEST_F(PaintTest, GenerateScene) {
    ASSERT_TRUE(swCanvas != nullptr);
    ASSERT_TRUE(scene != nullptr);
}

TEST_F(PaintTest, SceneBounds) {
    ASSERT_TRUE(swCanvas != nullptr);
    ASSERT_TRUE(scene != nullptr);

    ASSERT_EQ(shape->appendRect(10.0, 20.0, 100.0, 200.0, 0, 0), tvg::Result::Success);

    ASSERT_EQ(scene->push(std::move(shape)), tvg::Result::Success);

    float x, y, w, h;
    ASSERT_EQ(scene->bounds(&x, &y, &w, &h), tvg::Result::Success);
    ASSERT_EQ(x, 10.0);
    ASSERT_EQ(y, 20.0);
    ASSERT_EQ(w, 100.0);
    ASSERT_EQ(h, 200.0);
}

