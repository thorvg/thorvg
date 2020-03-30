#include <tizenvg.h>

using namespace std;

#define WIDTH 800
#define HEIGHT 800

static uint32_t buffer[WIDTH * HEIGHT];

int main(int argc, char **argv)
{
    //Initialize TizenVG Engine
    tvg::Engine::init();

    //Create a Composition Source Canvas
    auto canvas1 = tvg::SwCanvas::gen(buffer, WIDTH, HEIGHT);

    //Draw something onto the Canvas and leaving sync to the target.
    canvas1->draw();

    //Create a Main Canvas
    auto canvas2 = tvg::SwCanvas::gen(buffer, WIDTH, HEIGHT);

    //Create a Shape
    auto shape = tvg::ShapeNode::gen();
    shape->composite(canvas, tvg::CompMaskAdd);

    //Draw the Scene onto the Canvas
    canvas2->push(move(shape));
    canvas2->draw();
    canvas2->sync();

    //Terminate TizenVG Engine
    tvg::Engine::term();
}
