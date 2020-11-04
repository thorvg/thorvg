[![Build Status](https://travis-ci.org/Samsung/thorvg.svg?branch=master)](https://travis-ci.org/Samsung/thorvg)
[![Gitter](https://badges.gitter.im/thorvg/community.svg)](https://gitter.im/thorvg/community?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge)

# ThorVG

<p align="center">
  <img width="360" height="360" src="https://github.com/Samsung/thorvg/blob/master/res/logo2.png">
</p>

ThorVG is a platform independent lightweight standalone C++ library for drawing vector-based shapes and SVG.
The next list shows drawing primitives ThorVG providing.<br />
 - Paints: Line, Arc, Curve, Path, Shapes, Polygons
 - Filling: Solid, Linear, Radial Gradient
 - Scene Graph & Affine Transformation (translation, rotation, scale ...)
 - Stroking: Width, Join, Cap, Dash
 - Composition: Blending, Masking, Path Clipping, etc
 - Pictures: SVG, Bitmap, ... 
<p align="center">
  <img width="930" height="212" src="https://github.com/Samsung/thorvg/blob/master/res/example_primitives.png">
</p>
<br />
Next figure shows you a brief strategy how to use ThorVG in your system.<br />
<p align="center">
  <img width="900" height="288" src="https://github.com/Samsung/thorvg/blob/master/res/working_flow.png">
</p>
<br />
 
## Contents
- [Building ThorVG](#building-thorvg)
	- [Meson Build](#meson-build)
- [Quick Start](#quick-start)
- [Examples](#examples)
- [Tools](#tools)
	- [ThorVG Viewer](#thorvg-viewer)
	- [SVG to PNG](#svg-to-png)
- [API Bindings](#api-bindings)
- [Issues or Feature Requests?](#issues-or-feature-requests)

[](#contents)
<br />
## Building ThorVG
Basically, ThorVG supports [meson](https://mesonbuild.com/) build system.
<br />
### Meson Build
Install [meson](http://mesonbuild.com/Getting-meson.html) and [ninja](https://ninja-build.org/) if not already installed.

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
## Quick Start
ThorVG renders vector shapes on a given canvas buffer. Here shows quick start to learn basic API usages.

First, You can initialize ThorVG engine.

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
## Examples
There are various examples to understand ThorVG APIs, Please check sample code in `thorvg/src/examples`

To execute examples, you can build them with this meson option.
```
meson -Dexamples=true . build
```
Note that these examples are required EFL `elementary` package for launching. If you're using Linux-based OS, you could easily install its package from your OS distribution server. Otherwise, please visit the official [EFL page](https://enlightenment.org/) for more information.

[Back to contents](#contents)
<br />
<br />
## Tools
### ThorVG Viewer
[ThorVG viewer](https://samsung.github.io/thorvg.viewer) supports immediate rendering through your browser. You can drag & drop SVG files on the page, see the rendering result on the spot.

[Back to contents](#contents)
<br />
<br />
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

[Back to contents](#contents)
<br />
<br />
## Issues or Feature Requests?
For immediate assistant or support please reach us in [Gitter](https://gitter.im/thorvg/community)
