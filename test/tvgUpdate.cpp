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

    //Create a Scene
    auto scene = tvg::SceneNode::gen();

    //Shape1
    auto shape1 = tvg::ShapeNode::gen();

    /* Acquire shape1 pointer to access directly later.
       instead, you should consider not to interrupt this pointer life-cycle. */
    auto pshape1 = shape1->get();

    shape1->rect(0, 0, 400, 400, 0.1);
    shape1->fill(255, 0, 0, 255);
    shape1->rotate(0, 0, 45);   //axis x, y, z

    scene->push(move(shape1));
    canvas->push(move(scene));

    //Draw first frame
    canvas->draw();
    canvas->sync();

    /* Clear the previous shape path and Prepare a new shape path.
       You can call clear() to explicitly clear path data. */
    pshape1->rect(0, 0, 300, 300, 0.1);

    //Prepapre for drawing (this may work asynchronously)
    pshape1->update();

    //Draw second frame
    canvas->draw();
    canvas->sync();

    //Explicitly clear all retained paint nodes.
    canvas->clear();

    //Terminate TizenVG Engine
    tvg::Engine::term();
}
