[![Build Status](https://travis-ci.org/Samsung/thorvg.svg?branch=master)](https://travis-ci.org/Samsung/thorvg)
[![Gitter](https://badges.gitter.im/thorvg/community.svg)](https://gitter.im/thorvg/community?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge)

# ThorVG

<p align="center">
  <img width="240" height="240" src="https://github.com/Samsung/thorvg/blob/master/res/logo2.png">
</p>

ThorVG is a platform independent lightweight standalone C++ library for drawing vector-based shapes and SVG.

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
tvg::Initializer::init(tvg::CanvasEngine::Sw, 0);   //engine method, thread count
```
You can prepare a empty canvas for drawing on it.
```cpp
static uint32_t buffer[WIDTH * HEIGHT];          //canvas target buffer

auto canvas = tvg::SwCanvas::gen();              //generate a canvas
canvas->target(buffer, WIDTH, WIDTH, HEIGHT);    //stride, w, h
```

Next you can draw shapes onto the canvas.
```cpp
auto rect = tvg::Shape::gen();               //generate a round rectangle
rect->appendRect(50, 50, 200, 200, 20, 20);  //round geometry(x, y, w, h, rx, ry)
rect->fill(100, 100, 0, 255);                //set round rectangle color (r, g, b, a)
canvas->push(move(rect));                    //push round rectangle drawing command

auto circle = tvg::Shape::gen();             //generate a circle
circle->appendCircle(400, 400, 100, 100);    //circle geometry(cx, cy, radiusW, radiusH)

auto fill = tvg::RadialGradient::gen();      //generate radial gradient for circle fill
fill->radial(400, 400, 150);                 //radial fill info(cx, cy, radius)

tvg::Fill::ColorStop colorStops[2];          //gradient color info
colorStops[0] = {0, 255, 255, 255, 255};     //index, r, g, b, a (1st color value)
colorStops[1] = {1, 0, 0, 0, 255};           //index, r, g, b, a (2nd color value)

fill.colorStops(colorStop, 2);               //set gradient color info

circle->fill(move(fill));                    //set circle color

canvas->push(move(circle));                  //push circle drawing command

```
This code result look like this.
<p align="center">
  <img width="416" height="411" src="https://github.com/Samsung/thorvg/blob/master/res/example_shapes.jpg">
</p>

Next, this code snippet shows you how to draw SVG image.
```cpp
auto picture = tvg::Picture::gen();         //generate a picture
picture->load("tiger.svg");                 //Load SVG file.

canvas->push(move(picture));                //push picture drawing command
```
And here is the result.
<p align="center">
  <img width="391" height="400" src="https://github.com/Samsung/thorvg/blob/master/res/example_tiger.jpg">
</p>

Begin rendering & finish it at a particular time.
```cpp
canvas->draw();
canvas->sync();
```
Now you can acquire the rendered image in buffer memory.

Lastly, terminate the engine after usage.
```cpp
tvg::Initializer::term(tvg::CanvasEngine::Sw);
```

[Back to contents](#contents)
#
## Issues or Feature Requests?
For immediate assistant or support please reach us in [Gitter](https://gitter.im/thorvg/community)
