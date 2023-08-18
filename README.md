[![License](https://img.shields.io/badge/licence-MIT-green.svg?style=flat)](LICENSE)
![BinarySize](https://img.shields.io/badge/Size-150kb-blue)
[![Discord](https://img.shields.io/badge/Community-5865f2?style=flat&logo=discord&logoColor=white)](https://discord.gg/n25xj6J6HM)
<br>
[![CodeFactor](https://www.codefactor.io/repository/github/hermet/thorvg/badge)](https://www.codefactor.io/repository/github/hermet/thorvg)
[![Build Linux](https://github.com/thorvg/thorvg/actions/workflows/build_linux.yml/badge.svg?branch=main&event=push)](https://github.com/thorvg/thorvg/actions/workflows/actions.yml)
[![Build Windows](https://github.com/thorvg/thorvg/actions/workflows/build_win.yml/badge.svg?branch=main&event=push)](https://github.com/thorvg/thorvg/actions/workflows/build_win.yml)

# ThorVG
<p align="center">
  <img width="800" height="400" src="https://github.com/thorvg/thorvg/blob/main/res/logo/512/thorvg-banner.png">
</p>
ThorVG is an open-source graphics library designed for creating vector-based scenes and animations. Embracing the philosophy of "Simpler is better," the ThorVG project offers intuitive and user-friendly interfaces, all the while maintaining a compact size and minimal software complexity. <br />
<br />
The following list shows primitives that are supported by ThorVG: <br />
<br />

 * Shapes: Line, Arc, Curve, Path, Polygon
 * Filling: Solid & Gradients and Texture Mapping
 * Scene Graph & Affine Transformation (translation, rotation, scale, ...)
 * Stroking: Width, Join, Cap, Dash
 * Composition: Blending, Masking, Path Clipping
 * Images: TVG, SVG, JPG, PNG, WebP, Bitmap
 * Animations: Lottie
<p align="center">
  <img width="900" height="472" src="https://github.com/thorvg/thorvg/blob/main/res/example_primitives.png">
</p>
<br />
If your program includes the main renderer, you can seamlessly utilize ThorVG APIs by transitioning drawing contexts between the main renderer and ThorVG. Throughout these API calls, ThorVG effectively serializes drawing commands among volatile paint nodes. Subsequently, it undertakes synchronous or asynchronous rendering via its backend raster engines.<br />
<br />
ThorVG is adept at handling vector images, including formats like SVG, and it remains adaptable for accommodating additional popular formats as needed. In the rendering process, the library may generate intermediate frame buffers for scene compositing, though only when essential. The accompanying diagram provides a concise overview of how to effectively incorporate ThorVG within your system.<br />
<br />
<p align="center">
  <img width="900" height="288" src="https://github.com/thorvg/thorvg/blob/main/res/example_flow.png">
</p>
<br />
ThorVG incorporates a threading mechanism that aims to seamlessly acquire subsequent scenes without unnecessary delays. It operates using a finely-tuned task scheduler based on thread pools, encompassing various tasks such as encoding, decoding, updating, and rendering. This design ensures that all tasks can effectively leverage multi-processing capabilities.<br />
<br />
The task scheduler has been meticulously crafted to conceal complexity, streamline integration, and enhance user convenience. Therefore, the policy it employs is optional, allowing users to select it based on their specific requirements..<br />
<br />
<p align="center">
  <img width="900" height="313" src="https://github.com/thorvg/thorvg/blob/main/res/example_thread.png">
</p>
<br />

## Contents
- [ThorVG](#thorvg)
  - [Installation](#installation)
    - [Meson Build](#meson-build)
    - [Using with Visual Studio](#using-with-visual-studio)
    - [vcpkg](#vcpkg)
  - [Quick Start](#quick-start)
  - [SVG](#svg)
  - [Lottie](#lottie)
  - [TVG Picture](#tvg-picture)
  - [Practices](#practices)
    - [Tizen](#tizen)
    - [Rive](#rive)
    - [Godot](#godot)
  - [Examples](#examples)
  - [Documentation](#documentation)
  - [Tools](#tools)
    - [ThorVG Viewer](#thorvg-viewer)
    - [SVG to PNG](#svg-to-png)
    - [SVG to TVG](#svg-to-tvg)
  - [API Bindings](#api-bindings)
  - [Dependencies](#dependencies)
  - [Issues or Feature Requests](#issues-or-feature-requests)

[](#contents)
<br />
## Installation
You can install ThorVG from either Meson build or vcpkg package manager.
<br />
### Meson Build
ThorVG supports [meson](https://mesonbuild.com/) build system. Install [meson](http://mesonbuild.com/Getting-meson.html) and [ninja](https://ninja-build.org/) if not already installed.

Run meson to configure ThorVG:
```
meson build
```
Run ninja to build & install ThorVG:
```
ninja -C build install
```

### Using with Visual Studio
If you want to create Visual Studio project files, use the command --backend=vs. The resulting solution file (thorvg.sln) will be located in the build folder.
```
meson build --backend=vs
```

### vcpkg
You can download and install pre-packaged ThorVG using the [vcpkg](https://vcpkg.io/en/index.html) package manager.

Clone the vcpkg repo. Make sure you are in the directory you want the tool installed to before doing this.
```
git clone https://github.com/Microsoft/vcpkg.git
```
Run the bootstrap script to build the vcpkg.
```
./bootstrap-vcpkg.sh
```
Install the ThorVG package.
```
./vcpkg install thorvg
```

[Back to contents](#contents)
<br />
<br />
## Quick Start
ThorVG renders vector shapes to a given canvas buffer. The following is a quick start to show you how to use the essential APIs.

First, you should initialize the ThorVG engine:

```cpp
tvg::Initializer::init(tvg::CanvasEngine::Sw, 0);   //engine method, thread count
```

Then it would be best if you prepared an empty canvas for drawing on it:

```cpp
static uint32_t buffer[WIDTH * HEIGHT];                                 //canvas target buffer

auto canvas = tvg::SwCanvas::gen();                                     //generate a canvas
canvas->target(buffer, WIDTH, WIDTH, HEIGHT, tvg::SwCanvas::ARGB8888);  //buffer, stride, w, h, Colorspace
```

Next you can draw multiple shapes on the canvas:

```cpp
auto rect = tvg::Shape::gen();               //generate a shape
rect->appendRect(50, 50, 200, 200, 20, 20);  //define it as a rounded rectangle (x, y, w, h, rx, ry)
rect->fill(100, 100, 100, 255);              //set its color (r, g, b, a)
canvas->push(move(rect));                    //push the rectangle into the canvas

auto circle = tvg::Shape::gen();             //generate a shape
circle->appendCircle(400, 400, 100, 100);    //define it as a circle (cx, cy, rx, ry)

auto fill = tvg::RadialGradient::gen();      //generate a radial gradient
fill->radial(400, 400, 150);                 //set the radial gradient geometry info (cx, cy, radius)

tvg::Fill::ColorStop colorStops[2];          //gradient colors
colorStops[0] = {0.0, 255, 255, 255, 255};   //1st color values (offset, r, g, b, a)
colorStops[1] = {1.0, 0, 0, 0, 255};         //2nd color values (offset, r, g, b, a)
fill->colorStops(colorStops, 2);             //set the gradient colors info

circle->fill(move(fill));                    //set the circle fill
canvas->push(move(circle));                  //push the circle into the canvas

```

This code generates the following result:

<p align="center">
  <img width="416" height="411" src="https://github.com/thorvg/thorvg/blob/main/res/example_shapes.png">
</p>

You can also draw you own shapes and use dashed stroking:

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

float pattern[2] = {10, 10};                 //stroke dash pattern (line, gap)
path->stroke(pattern, 2);                    //set the stroke pattern

canvas->push(move(path));                    //push the path into the canvas

```

The code generates the following result:

<p align="center">
  <img width="300" height="300" src="https://github.com/thorvg/thorvg/blob/main/res/example_path.png">
</p>

Now begin rendering & finish it at a particular time:

```cpp
canvas->draw();
canvas->sync();
```

Then you can acquire the rendered image from the buffer memory.

Lastly, terminate the engine after its usage:

```cpp
tvg::Initializer::term(tvg::CanvasEngine::Sw);
```
[Back to contents](#contents)
<br />
<br />
## SVG

ThorVG facilitates [SVG Tiny Specification](https://www.w3.org/TR/SVGTiny12/) rendering via its dedicated SVG interpreter. Adhering to the SVG Tiny Specification, the implementation maintains a lightweight profile, rendering it particularly advantageous for embedded systems. While ThorVG comprehensively adheres to most of the SVG Tiny specs, certain features remain unsupported within the current framework. These include:</br>

 - Animation
 - Fonts & Text
 - Interactivity
 - Multimedia

The following code snippet shows how to draw SVG image using ThorVG:

```cpp
auto picture = tvg::Picture::gen();         //generate a picture
picture->load("tiger.svg");                 //load a SVG file
canvas->push(move(picture));                //push the picture into the canvas
```

The result is:

<p align="center">
  <img width="300" height="300" src="https://github.com/thorvg/thorvg/blob/main/res/example_tiger.png">
</p>

[Back to contents](#contents)
<br />
<br />
## Lottie

ThorVG aims to fully support Lottie Animation features. Lottie is a JSON-based vector animation file format that enables seamless distribution of animations on any platform, akin to shipping static assets. These files are compact and compatible with various devices, scaling up or down without pixelation. With Lottie, you can easily create, edit, test, collaborate, and distribute animations in a user-friendly manner. For more information, please visit [LottieFiles](https://www.lottiefiles.com)' website. <br />
<br />
Currently, ThorVG provides experimental support for Lottie Animation, and while most features are supported, a few advanced properties of Lottie may not be available yet:
<br />

 - Trimpath 
 - Texts
 - Polystar
 - Repeater
 - Merge-path
 - Filter Effects
 - Expressions

The following code snippet demonstrates how to use ThorVG to play a Lottie animation.
```cpp
auto animation = tvg::Animation::gen();     //generate an animation
auto picture = animation->picture()         //acquire a picture which associated with the animation.
picture->load("lottie.json");               //load a Lottie file
auto duration = animation->duration();      //figure out the animation duration time in seconds.
canvas->push(tvg::cast(picture));           //push the picture into the canvas
```
First, an animation and a picture are generated. The Lottie file (lottie.json) is loaded into the picture, and then the picture is added to the canvas. The animation frames are controlled using the animation object to play the Lottie animation. Also you might want to know the animation duration time to run your animation loop.
```cpp
animation->frame(animation->totalFrame() * progress);  //Set a current animation frame to display
canvas->update(animation->picture());                  //Update the picture to be redrawn.
```
Let's suppose the progress variable determines the position of the animation, ranging from 0 to 1 based on the total duration time of the animation. Adjusting the progress value allows you to control the animation at the desired position. Afterwards, the canvas is updated to redraw the picture with the updated animation frame.<br />
<br />
<p align="center">
  <img width="800" height="802" src="https://github.com/thorvg/thorvg/blob/main/res/example_lottie.gif">
</p>

[Back to contents](#contents)
<br />
<br />
## TVG Picture

ThorVG introduces the dedicated vector data format, known as TVG Picture, designed to efficiently store Paint node properties within a scene in binary form. This format is meticulously optimized in advance, ensuring compact file sizes and swift data loading processes. </br>
</br>
To leverage the TVG Picture format, ThorVG employs a specialized module called TVG Saver. This module is responsible for optimizing the data associated with all scene-tree nodes and storing them in binary form. During the optimization phase, TVG Saver intelligently eliminates unused information, eliminates duplicated properties, consolidates overlapping shapes, and employs data compression where feasible. Remarkably, these optimizations maintain compatibility with future versions of ThorVG libraries, with data compression utilizing the [Lempel-Ziv-Welchi](https://en.wikipedia.org/wiki/Lempel%E2%80%93Ziv%E2%80%93Welch) algorithm when applicable.</br>
</br>
As a result of these efforts, the final data size is notably smaller than other text-based vector data formats, such as SVG. This reduction in data size not only minimizes I/O operations but also mitigates memory bandwidth requirements during data loading. This aspect proves particularly beneficial for programs reliant on substantial vector resources. </br>
</br>
Furthermore, TVG Picture substantially streamlines resource loading tasks by circumventing the need for data interpretation, resulting in reduced runtime memory demands and rendering tasks that subsequently enhance performance. </br>
</br>
By adopting TVG Picture, you can achieve an average reduction of over 30% in data size and loading times (for more details, refer to "[See More](https://github.com/thorvg/thorvg/wiki/TVG-Picture-Binary-Size)"). Notably, the extent of performance improvement is contingent on resource size and complexity. </br>
</br>
<p align="center">
  <img width="909" height="314" src="https://github.com/thorvg/thorvg/blob/main/res/example_tvgsize.png">
</p>

As TVG Saver facilitates the export of the scene-tree into TVG Picture data files (TVG), the subsequent task of importing and restoring this data to programmable instances is efficiently handled by the TVG Loader. For seamless conversion from SVG to TVG, the ThorVG Viewer provides a swift solution.

<p align="center">
  <img width="710" height="215" src="https://github.com/thorvg/thorvg/blob/main/res/example_tvgmodule.png">
</p>


[Back to contents](#contents)
<br />
<br />
## Practices
### Tizen
ThorVG has been integrated into the [Tizen](https://www.tizen.org) platform as the vector graphics engine. [NUI](https://docs.tizen.org/application/dotnet/guides/user-interface/nui/overview/) is the name of Tizen UI framework which is written in C#. ThorVG is the backend engine of the [NUI Vector Graphics](https://docs.tizen.org/application/dotnet/guides/user-interface/nui/vectorgraphics/Overview/) which is used for vector primitive drawings and scalable image contents such as SVG and Lottie Animation among the Tizen applications.

<p align="center">
  <img width="798" height="285" src="https://github.com/thorvg/thorvg/blob/main/res/example_tizen.png">
</p>

### Godot
ThorVG has been integrated into the [Godot](https://www.godotengine.org) project to enable the creation of sleek and visually appealing user interfaces (UIs) and vector resources in the Godot game engine. Godot is a modern game engine that is both free and open-source, offering a comprehensive range of tools. With Godot, you can concentrate on developing your game without the need to recreate existing functionalities.

<p align="center">
  <img width="798" height="440" src="https://github.com/thorvg/thorvg/blob/main/res/example_godot.png">
</p>

### Rive
We're also building a [Rive](https://rive.app/) port that supports Rive Animation run through the ThorVG backend. Rive is a brand new animation platform
that supports fancy, user-interactive vector animations. For more details see [Rive-Tizen](https://github.com/rive-app/rive-tizen) on [Github](https://github.com/rive-app/).

<p align="center">
  <img width="600" height="324" src="https://github.com/thorvg/thorvg/blob/main/res/example_rive.gif">
</p>

[Back to contents](#contents)
<br />
<br />
## Examples
here are plenty of sample code in `thorvg/src/examples` to help you in understanding the ThorVG APIs.

To execute these examples, you can build them with the following meson build option:
```
meson . build -Dexamples=true
```
Note that these examples require the EFL dev package for launching. If you're using Linux-based OS, you can easily install this package from your OS distribution server. For Ubuntu, you can install it with this command.
```
apt-get install libefl-all-dev
```
Alternatively, you can download the package [here](https://download.enlightenment.org/rel/win/efl/) for Windows. For more information, please visit the official [EFL page](https://enlightenment.org/).

[Back to contents](#contents)
<br />
<br />

## Documentation
ThorVG API documentation is available at [thorvg.org/apis](https://www.thorvg.org/apis), and can also found in the [docs](/docs) folder of this repo.

[Back to contents](#contents)
<br />
<br />
## Tools
### ThorVG Viewer
ThorVG provides the resource verification tool for the ThorVG Engine. [ThorVG viewer](https://thorvg.github.io/thorvg.viewer/) does immediate rendering via web browser running on the ThorVG web-assembly binary, allowing real-time editing of the vector elements on it. It doesn't upload your resources to any external server while allowing to export to supported formats such as TVG, so the designer resource copyright is protected.

https://user-images.githubusercontent.com/71131832/130445967-fb8f7d81-9c89-4598-b7e4-2c046d5d7438.mp4

### SVG to PNG
ThorVG provides an executable `svg2png` converter that generates a PNG file from an SVG file.

To use the `svg2png`, you must turn on this feature in the build option:
```
meson . build -Dtools=svg2png
```
Alternatively, you can add the `svg2png` value to the `tools` option in `meson_option.txt`. The build output will be located in `{builddir}/src/bin/svg2png/`.
<br />

To use the `svg2png` converter you have to pass the `SVG files` parameter. It can be a file name with the `.svg` extension or a directory name. Multiple files or directories separated by a space are also accepted. If a directory is passed, the `.svg` file extension is searched inside it and in all of its subdirectories.

Optionally, the image resolution can be provided in the `WxH` format (two numbers separated by an `x` sign) following the `-r` flag.
<br />
The background color can be set with the `-b` flag. The `bgColor` parameter should be passed as a three-bytes hexadecimal value in the `ffffff` format. The default background is transparent.
<br />
Both flags, if provided, are applied to all of the `.svg` files.

The usage examples of the `svg2png`:
```
Usage:
    svg2png [SVG files] [-r resolution] [-b bgColor]

Flags:
    -r set the output image resolution.
    -b set the output image background color.

Examples:
    $ svg2png input.svg
    $ svg2png input.svg -r 200x200
    $ svg2png input.svg -r 200x200 -b ff00ff
    $ svg2png input1.svg input2.svg -r 200x200 -b ff00ff
    $ svg2png . -r 200x200
```

### SVG to TVG
ThorVG provides an executable `svg2tvg` converter that generates a TVG file from an SVG file.

To use `svg2tvg`, you need to activate this feature in the build option:
```
meson . build -Dtools=svg2tvg
```
Alternatively, you can add the `svg2tvg` value to the `tools` option in `meson_option.txt`. The build output will be located in `{builddir}/src/bin/svg2tvg/`.

Examples of the usage of the `svg2tvg`:
```
Usage:
   svg2tvg [SVG file] or [SVG folder]

Examples:
    $ svg2tvg input.svg
    $ svg2tvg svgfolder
```

[Back to contents](#contents)
<br />
<br />
## API Bindings
Our main development APIs are written in C++, but ThorVG also provides API bindings for C.

To enable CAPI binding, you need to activate this feature in the build options:
```
meson . build -Dbindings="capi"
```
[Back to contents](#contents)
<br />
<br />
## Dependencies
ThorVG core has no dependencies. However, ThorVG has optional feature extensions. Some of these have dependencies as follows:

* GL renderer: [EGL](https://www.khronos.org/egl), [GLESv2](https://www.khronos.org/opengles/)
* External PNG support: [libpng](https://github.com/glennrp/libpng)
* External JPG support: [turbojpeg](https://github.com/libjpeg-turbo/libjpeg-turbo)
* Examples: [EFL](https://www.enlightenment.org/about-efl.md)

Note that ThorVG supports both static/external image loaders. If your system has no external libraries, you can choose static loaders instead.

[Back to contents](#contents)
<br />
<br />
## Issues or Feature Requests
For support, please reach us in [Discord](https://discord.gg/n25xj6J6HM)
