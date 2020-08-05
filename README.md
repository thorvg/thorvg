# ThorVG

ThorVG is a platform independent standalone C++ library for drawing vector-based shapes and SVG.

#
## Contents
- [Building ThorVG](#building-thorvg)
	- [Meson Build](#meson-build)
- [Quick Start](#quick-start)
- [Issues or Feature Requests?](#issues-or-feature-requests)
#
## Building ThorVG
thorvg supports [meson](https://mesonbuild.com/) build system.
#
### Meson Build
install [meson](http://mesonbuild.com/Getting-meson.html) and [ninja](https://ninja-build.org/) if not already installed.

Run meson to configure ThorVG.
```
meson build
```
Run ninja to build & install ThorVG.
```
ninja -C build install
```
[Back to contents](#contents)
#
## Quick Start
ThorVG renders vector shapes on a given canvas buffer.

You can initialize ThorVG engine first:
```cpp
tvg::Initializer::init(tvg::CanvasEngine::Sw);
```
You can prepare a empty canvas for drawing on it.
```cpp
static uint32_t buffer[WIDTH * HEIGHT];          //canvas target buffer

auto canvas = tvg::SwCanvas::gen();              //generate a canvas
canvas->target(buffer, WIDTH, WIDTH, HEIGHT);    //stride, w, h
```

Next you can draw shapes onto the canvas.
```cpp
auto shape = tvg::Shape::gen();             //generate a shape
shape->appendRect(0, 0, 200, 200, 0, 0);    //x, y, w, h, rx, ry
shape->appendCircle(400, 400, 100, 100);    //cx, cy, radiusW, radiusH
shape->fill(255, 255, 0, 255);              //r, g, b, a

canvas->push(move(shape));                  //push shape drawing command
```
Begin rendering & finish it at a particular time.
```cpp
canvas->draw();
canvas->sync();
```
Lastly, you can acquire the rendered image in buffer memory.

[Back to contents](#contents)
#
## Issues or Feature Requests?
For immidiate assistant or support please reach us in [Gitter](https://gitter.im/thorvg-dev/community#)
