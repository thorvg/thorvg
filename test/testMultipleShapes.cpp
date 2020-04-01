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
    canvas->reserve(2);      //reserve 2 shape nodes (optional)

    //Prepare Rectangle
    auto shape1 = tvg::ShapeNode::gen();
    shape1->rect(0, 0, 400, 400, 0.1);        //x, y, w, h, corner_radius
    shape1->fill(0, 255, 0, 255);             //r, g, b, a
    canvas->push(move(shape1));

    //Prepare Circle
    auto shape2 = tvg::ShapeNode::gen();
    shape2->circle(400, 400, 200);            //cx, cy, radius
    shape2->fill(255, 255, 0, 255);           //r, g, b, a
    canvas->push(move(shape2));

    //Draw the Shapes onto the Canvas
    canvas->draw();
    canvas->sync();

    //Terminate TizenVG Engine
    tvg::Engine::term();
}
