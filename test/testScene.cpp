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
    auto scene = tvg::Scene::gen();
    scene->reserve(3);   //reserve 3 shape nodes (optional)

    //Shape1
    auto shape1 = tvg::Shape::gen();
    shape1->rect(0, 0, 400, 400, 0.1);
    shape1->fill(255, 0, 0, 255);
    shape1->rotate(0, 0, 45);   //axis x, y, z
    scene->push(move(shape1));

    //Shape2
    auto shape2 = tvg::Shape::gen();
    shape2->rect(0, 0, 400, 400, 0.1);
    shape2->fill(0, 255, 0, 255);
    shape2->transform(matrix);  //by matrix (var matrix[4 * 4];)
    scene->push(move(shape2));

    //Shape3
    auto shape3 = tvg::Shape::gen();
    shape3->rect(0, 0, 400, 400, 0.1);
    shape3->fill(0, 0, 255, 255);
    shape3->origin(100, 100);   //offset
    scene->push(move(shape3));

    //Draw the Scene onto the Canvas
    canvas->push(move(scene));
    canvas->draw();
    canvas->sync();

    //Terminate TizenVG Engine
    tvg::Engine::term();
}
