#include <thorvg.h>

using namespace std;

#define WIDTH 800
#define HEIGHT 800

static uint32_t buffer[WIDTH * HEIGHT];

int main(int argc, char **argv)
{
    //Initialize ThorVG Engine
    tvg::Initializer::init(tvg::CanvasEngine::Sw);

    //Create a Composition Source Canvas
    auto canvas1 = tvg::SwCanvas::gen(buffer, WIDTH, HEIGHT);

    //Draw something onto the Canvas and leaving sync to the target.
    canvas1->draw();

    //Create a Main Canvas
    auto canvas2 = tvg::SwCanvas::gen(buffer, WIDTH, HEIGHT);

    //Create a Shape
    auto shape = tvg::Shape::gen();
    shape->composite(canvas, tvg::CompMaskAdd);

    //Draw the Scene onto the Canvas
    canvas2->push(move(shape));
    canvas2->draw();
    canvas2->sync();

    //Terminate ThorVG Engine
    tvg::Initializer::term(tvg::CanvasEngine::Sw);
}
