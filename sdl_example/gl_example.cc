#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <iostream>
#include <thorvg.h>

int main(int argc, const char **argv) {

  auto window =
      SDL_CreateWindow("Hello world !", SDL_WINDOWPOS_UNDEFINED,
                       SDL_WINDOWPOS_UNDEFINED, 800, 600, SDL_WINDOW_SHOWN);

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);

#ifdef __APPLE__
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#endif

  auto gl_context = SDL_GL_CreateContext(window);

  if (tvg::Initializer::init(tvg::CanvasEngine::Gl, 1) !=
      tvg::Result::Success) {
    std::cerr << "failed to init thorvg" << std::endl;
    return -1;
  }

  auto canvas = tvg::GlCanvas::gen();

  bool runing = true;

  std::cout << "glversion = " << glGetString(GL_VERSION) << std::endl;

  while (runing) {
    SDL_Event event{};

    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) {
        runing = false;
      }

      glClearColor(0.f, 0.f, 0.f, 0.f);

      glClear(GL_COLOR_BUFFER_BIT);

      auto shape1 = tvg::Shape::gen();

      // Appends Paths
      shape1->moveTo(199, 34);
      shape1->lineTo(253, 143);
      shape1->lineTo(374, 160);
      shape1->lineTo(287, 244);
      shape1->lineTo(307, 365);
      shape1->lineTo(199, 309);
      shape1->lineTo(97, 365);
      shape1->lineTo(112, 245);
      shape1->lineTo(26, 161);
      shape1->lineTo(146, 143);
      shape1->close();
      shape1->fill(0, 0, 255);

      canvas->push(std::move(shape1));

      canvas->draw();
      canvas->sync();

      SDL_GL_SwapWindow(window);
    }
  }

  return 0;
}