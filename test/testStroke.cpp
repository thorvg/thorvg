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

    //Prepare a Shape
    auto shape1 = tvg::ShapeNode::gen();
    shape1->rect(0, 0, 400, 400, 0.1);     //x, y, w, h, corner_radius
    shape1->fill(0, 255, 0, 255);

    //Stroke Style
    shape1->strokeColor(0, 0, 0, 255);     //r, g, b, a
    shape1->strokeWidth(1);                //1px
    shape1->strokeJoin(tvg::StrokeJoin::Miter);
    shape1->strokeLineCap(tvg::StrokeLineCap::Butt);

    uint32_t dash[] = {3, 1, 5, 1};        //dash pattern
    shape1->strokeDash(dash, 4);

    //Draw the Shape onto the Canvas
    canvas->push(move(shape1));
    canvas->draw();
    canvas->sync();

    //Terminate TizenVG Engine
    tvg::Engine::term();
}
