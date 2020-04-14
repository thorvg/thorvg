#include <tizenvg.h>

using namespace std;

#define WIDTH 800
#define HEIGHT 800

static uint32_t buffer[WIDTH * HEIGHT];

int main(int argc, char **argv)
{
    //Initialize TizenVG Engine
    tvg::Engine::init();

    //Create a Canvas
    auto canvas = tvg::SwCanvas::gen(buffer, WIDTH, HEIGHT);

    //Prepare a Shape (Rectangle)
    auto shape1 = tvg::ShapeNode::gen();
    shape1->appendRect(0, 0, 400, 400, 0);      //x, y, w, h, corner_radius
    shape1->fill(0, 255, 0, 255);               //r, g, b, a

    /* Push the shape into the Canvas drawing list
       When this shape is into the canvas list, the shape could update & prepare
       internal data asynchronously for coming rendering.
       Canvas keeps this shape node unless user call canvas->clear() */
    canvas->push(move(shape1));

    canvas->draw();
    canvas->sync();

    //Terminate TizenVG Engine
    tvg::Engine::term();
}
