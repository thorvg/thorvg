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
    auto pshape1 = shape1->get();      //acquire shape1 pointer to access directly
    shape1->rect(0, 0, 400, 400, 0.1);
    shape1->fill(255, 0, 0, 255);
    shape1->rotate(0, 0, 45);   //axis x, y, z
    scene->push(move(shape1));

    //Draw the Scene onto the Canvas
    canvas->push(move(scene));

    //Draw frame 1
    canvas->draw();
    canvas->sync();

    //Clear previous shape path
    pshape1->clear();
    pshape1->rect(0, 0, 300, 300, 0.1);

    //Prepapre for drawing
    pshape1->update();

    //Draw frame 2
    canvas->draw();
    canvas->sync();

    //Terminate TizenVG Engine
    tvg::Engine::term();
}
