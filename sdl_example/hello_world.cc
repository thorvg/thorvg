
#include <SDL.h>
#include <iostream>
#include <thorvg.h>
#include <vector>

int main(int argc, const char **argv) {
  std::cout << "Hello World" << std::endl;

  SDL_Window *window = nullptr;
  SDL_Surface *w_surface = nullptr;
  SDL_Surface *r_surface = nullptr;
  Uint32 rmask, gmask, bmask, amask;
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
  rmask = 0xff000000;
  gmask = 0x00ff0000;
  bmask = 0x0000ff00;
  amask = 0x000000ff;
#else
  rmask = 0x000000ff;
  gmask = 0x0000ff00;
  bmask = 0x00ff0000;
  amask = 0xff000000;
#endif

  SDL_Init(SDL_INIT_EVERYTHING);

  window =
      SDL_CreateWindow("Hello world !", SDL_WINDOWPOS_UNDEFINED,
                       SDL_WINDOWPOS_UNDEFINED, 800, 600, SDL_WINDOW_SHOWN);

  if (window == nullptr) {
    std::cerr << "Failed to create window" << std::endl;
  }

  w_surface = SDL_GetWindowSurface(window);
  r_surface = SDL_CreateRGBSurface(0, 800, 600, 32, rmask, gmask, bmask, amask);

  std::vector<uint32_t> buffer(800 * 600);

  tvg::CanvasEngine tvgEngine = tvg::CanvasEngine::Sw;

  if (tvg::Initializer::init(tvgEngine, 1) != tvg::Result::Success) {
    std::cerr << "failed to init thorvg" << std::endl;
    return -1;
  }

  auto canvas = tvg::SwCanvas::gen();

  canvas->target(buffer.data(), 800, 800, 600, tvg::SwCanvas::ABGR8888);

  auto rect = tvg::Shape::gen(); // generate a shape

  rect->moveTo(199, 34); // set sequential path coordinates
  rect->lineTo(253, 143);
  rect->lineTo(374, 160);
  rect->lineTo(287, 244);
  rect->lineTo(307, 365);
  rect->lineTo(199, 309);
  rect->lineTo(97, 365);
  rect->lineTo(112, 245);
  rect->lineTo(26, 161);
  rect->lineTo(146, 143);
  rect->close();

  rect->fill(100, 100, 100, 255); // set its color (r, g, b, a)

  auto result = tvg::Shape::triangulation(rect.get());

  canvas->push(std::move(rect));

  result->translate(300, 0);

  result->stroke(3.f);

  result->stroke(255, 0, 0);

  canvas->push(std::move(result));

  canvas->draw();
  canvas->sync();

  SDL_LockSurface(r_surface);

  std::memcpy(r_surface->pixels, buffer.data(), buffer.size() * 4);

  SDL_UnlockSurface(r_surface);

  bool quite = false;

  while (!quite) {
    SDL_Event e{0};
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT) {
        quite = true;
        break;
      }

      SDL_FillRect(w_surface, nullptr,
                   SDL_MapRGB(w_surface->format, 0xFF, 0xFF, 0xFF));

      SDL_Rect stretchRect;
      stretchRect.x = 0;
      stretchRect.y = 0;
      stretchRect.w = 800;
      stretchRect.h = 600;

      SDL_BlitScaled(r_surface, nullptr, w_surface, &stretchRect);

      SDL_UpdateWindowSurface(window);
    }
  }

  SDL_DestroyWindow(window);

  SDL_Quit();

  return 0;
}