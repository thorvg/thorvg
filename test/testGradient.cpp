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
    auto shape1 = tvg::Shape::gen();
    shape1->rect(0, 0, 400, 400, 0.1);        //x, y, w, h, corner_radius

    //Linear Gradient Fill
    auto fill1 = tvg::LinearFill::gen();
    fill1->range(fill1, 0, 0, 400, 400);      //from(x, y), to(x, y)
    fill1->color(0, 255, 0, 0, 255);          //color Stop 0: Red
    fill1->color(0.5, 0, 255, 0, 255);        //color Stop 1: Green
    fill1->color(1, 0, 0, 255, 255);          //color Stop 2: Blue
    shape1.fill(fill1);

    canvas->push(move(shape1));

    //Prepare Circle
    auto shape2 = tvg::Shape::gen();
    shape2->circle(400, 400, 200);            //cx, cy, radius

    //Radial Gradient Fill
    auto fill2 = tvg::RadialFill::gen();
    fill2->range(400, 400, 200);              //center(x, y), radius
    fill2->color(0, 255, 0, 0, 255);          //color Stop 0: Red
    fill2->color(0.5, 0, 255, 0, 255);        //color Stop 1: Green
    fill2->color(1, 0, 0, 255, 255);          //color Stop 2: Blue
    shape2.fill(fill2);

    canvas->push(move(shape2));

    //Draw the Shapes onto the Canvas
    canvas->draw();
    canvas->sync();

    //Terminate TizenVG Engine
    tvg::Engine::term();
}
