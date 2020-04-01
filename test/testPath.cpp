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

    //Prepare Path
    auto path = tvg::Path::gen();
    path.reserve(cmdCnt, ptCnt);  //command count, points count
    path.moveTo(...);
    path.lineTo(...);
    path.cubicTo(...);
    path.close();

    //Prepare a Shape
    auto shape1 = tvg::ShapeNode::gen();
    shape1->path(move(path));     //migrate owner otherwise,
    shape1->path(path.get());     //copy raw data directly for performance
    shape1->fill(0, 255, 0, 255);

    //Draw the Shape onto the Canvas
    canvas->push(move(shape1));
    canvas->draw();
    canvas->sync();

    //Terminate TizenVG Engine
    tvg::Engine::term();
}
