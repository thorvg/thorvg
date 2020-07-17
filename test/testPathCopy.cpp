#include "testCommon.h"

/************************************************************************/
/* Drawing Commands                                                     */
/************************************************************************/

void tvgDrawCmds(tvg::Canvas* canvas)
{
    if (!canvas) return;

    /* Star */

    //Prepare Path Commands
    tvg::PathCommand cmds[11];
    cmds[0] = tvg::PathCommand::MoveTo;
    cmds[1] = tvg::PathCommand::LineTo;
    cmds[2] = tvg::PathCommand::LineTo;
    cmds[3] = tvg::PathCommand::LineTo;
    cmds[4] = tvg::PathCommand::LineTo;
    cmds[5] = tvg::PathCommand::LineTo;
    cmds[6] = tvg::PathCommand::LineTo;
    cmds[7] = tvg::PathCommand::LineTo;
    cmds[8] = tvg::PathCommand::LineTo;
    cmds[9] = tvg::PathCommand::LineTo;
    cmds[10] = tvg::PathCommand::Close;

    //Prepare Path Points
    tvg::Point pts[10];
    pts[0] = {199, 34};    //MoveTo
    pts[1] = {253, 143};   //LineTo
    pts[2] = {374, 160};   //LineTo
    pts[3] = {287, 244};   //LineTo
    pts[4] = {307, 365};   //LineTo
    pts[5] = {199, 309};   //LineTo
    pts[6] = {97, 365};    //LineTo
    pts[7] = {112, 245};   //LineTo
    pts[8] = {26, 161};    //LineTo
    pts[9] = {146, 143};   //LineTo

    auto shape1 = tvg::Shape::gen();
    shape1->appendPath(cmds, 11, pts, 10);     //copy path data
    shape1->fill(0, 255, 0, 255);
    if (canvas->push(move(shape1)) != tvg::Result::Success) return;

    /* Circle */
    auto cx = 550.0f;
    auto cy = 550.0f;
    auto radius = 125.0f;
    auto halfRadius = radius * 0.552284f;

    //Prepare Path Commands
    tvg::PathCommand cmds2[6];
    cmds2[0] = tvg::PathCommand::MoveTo;
    cmds2[1] = tvg::PathCommand::CubicTo;
    cmds2[2] = tvg::PathCommand::CubicTo;
    cmds2[3] = tvg::PathCommand::CubicTo;
    cmds2[4] = tvg::PathCommand::CubicTo;
    cmds2[5] = tvg::PathCommand::Close;

    //Prepare Path Points
    tvg::Point pts2[13];
    pts2[0] = {cx, cy - radius};    //MoveTo
    //CubicTo 1
    pts2[1] = {cx + halfRadius, cy - radius};      //Ctrl1
    pts2[2] = {cx + radius, cy - halfRadius};      //Ctrl2
    pts2[3] = {cx + radius, cy};                   //To
    //CubicTo 2
    pts2[4] = {cx + radius, cy + halfRadius};      //Ctrl1
    pts2[5] = {cx + halfRadius, cy + radius};      //Ctrl2
    pts2[6] = {cx, cy+ radius};                    //To
    //CubicTo 3
    pts2[7] = {cx - halfRadius, cy + radius};      //Ctrl1
    pts2[8] = {cx - radius, cy + halfRadius};      //Ctrl2
    pts2[9] = {cx - radius, cy};                   //To
    //CubicTo 4
    pts2[10] = {cx - radius, cy - halfRadius};     //Ctrl1
    pts2[11] = {cx - halfRadius, cy - radius};     //Ctrl2
    pts2[12] = {cx, cy - radius};                  //To

    auto shape2 = tvg::Shape::gen();
    shape2->appendPath(cmds2, 6, pts2, 13);     //copy path data
    shape2->fill(255, 255, 0, 255);
    if (canvas->push(move(shape2)) != tvg::Result::Success) return;

}

/************************************************************************/
/* Sw Engine Test Code                                                  */
/************************************************************************/

static unique_ptr<tvg::SwCanvas> swCanvas;

void tvgSwTest(uint32_t* buffer)
{
    //Create a Canvas
    swCanvas = tvg::SwCanvas::gen();
    swCanvas->target(buffer, WIDTH, WIDTH, HEIGHT);

    /* Push the shape into the Canvas drawing list
       When this shape is into the canvas list, the shape could update & prepare
       internal data asynchronously for coming rendering.
       Canvas keeps this shape node unless user call canvas->clear() */
    tvgDrawCmds(swCanvas.get());
}

void drawSwView(void* data, Eo* obj)
{
    if (swCanvas->draw() == tvg::Result::Success) {
        swCanvas->sync();
    }
}


/************************************************************************/
/* GL Engine Test Code                                                  */
/************************************************************************/

static unique_ptr<tvg::GlCanvas> glCanvas;

void initGLview(Evas_Object *obj)
{
    static constexpr auto BPP = 4;

    //Create a Canvas
    glCanvas = tvg::GlCanvas::gen();
    glCanvas->target(nullptr, WIDTH * BPP, WIDTH, HEIGHT);

    /* Push the shape into the Canvas drawing list
       When this shape is into the canvas list, the shape could update & prepare
       internal data asynchronously for coming rendering.
       Canvas keeps this shape node unless user call canvas->clear() */
    tvgDrawCmds(glCanvas.get());
}

void drawGLview(Evas_Object *obj)
{
    auto gl = elm_glview_gl_api_get(obj);
    gl->glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    gl->glClear(GL_COLOR_BUFFER_BIT);

    if (glCanvas->draw() == tvg::Result::Success) {
        glCanvas->sync();
    }
}


/************************************************************************/
/* Main Code                                                            */
/************************************************************************/

int main(int argc, char **argv)
{
    tvg::CanvasEngine tvgEngine = tvg::CanvasEngine::Sw;

    if (argc > 1) {
        if (!strcmp(argv[1], "gl")) tvgEngine = tvg::CanvasEngine::Gl;
    }

    //Initialize ThorVG Engine
    if (tvgEngine == tvg::CanvasEngine::Sw) {
        cout << "tvg engine: software" << endl;
    } else {
        cout << "tvg engine: opengl" << endl;
    }

    //Initialize ThorVG Engine
    if (tvg::Initializer::init(tvgEngine) == tvg::Result::Success) {

        elm_init(argc, argv);

        if (tvgEngine == tvg::CanvasEngine::Sw) {
            createSwView();
        } else {
            createGlView();
        }

        elm_run();
        elm_shutdown();

        //Terminate ThorVG Engine
        tvg::Initializer::term(tvgEngine);

    } else {
        cout << "engine is not supported" << endl;
    }
    return 0;
}
