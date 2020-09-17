[![Build Status](https://travis-ci.org/Samsung/thorvg.svg?branch=master)](https://travis-ci.org/Samsung/thorvg)
[![Gitter](https://badges.gitter.im/thorvg/community.svg)](https://gitter.im/thorvg/community?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge)

# ThorVG

<p align="center">
  <img width="360" height="360" src="https://github.com/Samsung/thorvg/blob/master/res/logo2.png">
</p>

ThorVG is a platform independent lightweight standalone C++ library for drawing vector-based shapes and SVG.
<br />
<br />
## Contents
- [Building ThorVG](#building-thorvg)
	- [Meson Build](#meson-build)
- [Quick Start](#quick-start)
- [Tools](#tools)
	- [SVG to PNG](#svg-to-png)
- [API Bindings](#api-bindings)
- [Issues or Feature Requests?](#issues-or-feature-requests)

[](#contents)
<br />
## Building ThorVG
thorvg supports [meson](https://mesonbuild.com/) build system.
<br />
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
<br />
<br />
## Examples
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

Next you can draw multiple shapes onto the canvas.

```cpp
auto rect = tvg::Shape::gen();               //generate a round rectangle
rect->appendRect(50, 50, 200, 200, 20, 20);  //round geometry(x, y, w, h, rx, ry)
rect->fill(100, 100, 0, 255);                //round rectangle color (r, g, b, a)
canvas->push(move(rect));                    //push round rectangle drawing command

auto circle = tvg::Shape::gen();             //generate a circle
circle->appendCircle(400, 400, 100, 100);    //circle geometry(cx, cy, radiusW, radiusH)

auto fill = tvg::RadialGradient::gen();      //generate radial gradient for circle fill
fill->radial(400, 400, 150);                 //radial fill info(cx, cy, radius)

tvg::Fill::ColorStop colorStops[2];          //gradient color info
colorStops[0] = {0, 255, 255, 255, 255};     //index, r, g, b, a (1st color value)
colorStops[1] = {1, 0, 0, 0, 255};           //index, r, g, b, a (2nd color value)
fill.colorStops(colorStop, 2);               //set fil with gradient color info

circle->fill(move(fill));                    //circle color
canvas->push(move(circle));                  //push circle drawing command

```

This code result looks like this.

<p align="center">
  <img width="416" height="411" src="https://github.com/Samsung/thorvg/blob/master/res/example_shapes.png">
</p>

Or you can draw pathes with dash stroking.

```cpp
auto path = tvg::Shape::gen();               //generate a path
path->moveTo(199, 34);                       //set sequential path coordinates
path->lineTo(253, 143);
path->lineTo(374, 160);
path->lineTo(287, 244);
path->lineTo(307, 365);
path->lineTo(199, 309);
path->lineTo(97, 365);
path->lineTo(112, 245);
path->lineTo(26, 161);
path->lineTo(146, 143);
path->close();

path->fill(150, 150, 255, 255);              //path color

path->stroke(3);                             //stroke width
path->stroke(0, 0, 255, 255);                //stroke color
path->stroke(tvg::StrokeJoin::Round);        //stroke join style
path->stroke(tvg::StrokeCap::Round);         //stroke cap style

float pattern[2] = {10, 10};
path->stroke(pattern, 2);                    //stroke dash pattern (line, gap)

canvas->push(move(path));                    //push path drawing command

```

This path drawing result shows like this.

<p align="center">
  <img width="300" height="300" src="https://github.com/Samsung/thorvg/blob/master/res/example_path.png">
</p>

Next, this code snippet shows you how to draw SVG image.

```cpp
auto picture = tvg::Picture::gen();         //generate a picture
picture->load("tiger.svg");                 //Load SVG file.
canvas->push(move(picture));                //push picture drawing command
```

And here is the result.

<p align="center">
  <img width="300" height="300" src="https://github.com/Samsung/thorvg/blob/master/res/example_tiger.png">
</p>

Begin rendering & finish it at a particular time.

```cpp
canvas->draw();
canvas->sync();
```

Now you can acquire the rendered image from the buffer memory.

Lastly, terminate the engine after usage.

```cpp
tvg::Initializer::term(tvg::CanvasEngine::Sw);
```
[Back to contents](#contents)
<br />
<br />
## Tools
### SVG to PNG
ThorVG provides an executable `svg2png` converter which generate a PNG file from a SVG file.

To use `svg2png`, you must turn on its feature in the build option.
```
meson -Dtools=svg2png . build
```
Alternatively, you can add `svg2png` value to `tools` option in `meson_option.txt`. Build output will be located in `{builddir}/src/bin/svg2png/`

For more information, see `svg2png` usages:
```
Usage: 
   svg2png [svgFileName] [Resolution] [bgColor]

Examples: 
    $ svg2png input.svg
    $ svg2png input.svg 200x200
    $ svg2png input.svg 200x200 ff00ff
```

[Back to contents](#contents)
<br />
<br />
## API Bindings
Our main development APIs are written in C++ but ThorVG also provides API bindings such as: C.
<br />
[Back to contents](#contents)
<br />
<br />
## Issues or Feature Requests?
For immediate assistant or support please reach us in [Gitter](https://gitter.im/thorvg/community)
