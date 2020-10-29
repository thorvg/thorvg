#include <gtest/gtest.h>
#include <iostream>
#include <thread>
#include <thorvg.h>

class CanvasTest : public ::testing::Test {
public:
    void SetUp() {
        auto threads = std::thread::hardware_concurrency();
        //Initialize ThorVG Engine
        if (tvg::Initializer::init(tvgEngine, threads) == tvg::Result::Success) {
            swCanvas = tvg::SwCanvas::gen();
        }
    }
    void TearDown() {

        //Terminate ThorVG Engine
        tvg::Initializer::term(tvgEngine);
    }
public:
    std::unique_ptr<tvg::SwCanvas> swCanvas;
    tvg::CanvasEngine tvgEngine = tvg::CanvasEngine::Sw;
};

TEST_F(CanvasTest, GenerateCanvas) {
    ASSERT_TRUE(swCanvas != nullptr);
}

