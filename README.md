[![Discord](https://img.shields.io/badge/Community-5865f2?style=flat&logo=discord&logoColor=white)](https://discord.gg/n25xj6J6HM)
[![ThorVGPT](https://img.shields.io/badge/ThorVGPT-76A99C?style=flat&logo=openai&logoColor=white)](https://chat.openai.com/g/g-Ht3dYIwLO-thorvgpt)
[![OpenCollective](https://img.shields.io/badge/OpenCollective-84B5FC?style=flat&logo=opencollective&logoColor=white)](https://opencollective.com/thorvg)
[![License](https://img.shields.io/badge/licence-MIT-green.svg?style=flat)](LICENSE)
![BinarySize](https://img.shields.io/badge/Size-200kb-blue)
[![CodeFactor](https://www.codefactor.io/repository/github/hermet/thorvg/badge)](https://www.codefactor.io/repository/github/hermet/thorvg)
<br>
[![Build Ubuntu](https://github.com/thorvg/thorvg/actions/workflows/build_ubuntu.yml/badge.svg?branch=main&event=push)](https://github.com/thorvg/thorvg/actions/workflows/build_ubuntu.yml)
[![Build Windows](https://github.com/thorvg/thorvg/actions/workflows/build_windows.yml/badge.svg?branch=main&event=push)](https://github.com/thorvg/thorvg/actions/workflows/build_windows.yml)
[![Build macOS](https://github.com/thorvg/thorvg/actions/workflows/build_macos.yml/badge.svg?branch=main&event=push)](https://github.com/thorvg/thorvg/actions/workflows/build_macos.yml)
[![Build iOS](https://github.com/thorvg/thorvg/actions/workflows/build_ios.yml/badge.svg?branch=main&event=push)](https://github.com/thorvg/thorvg/actions/workflows/build_ios.yml)
[![Build Android](https://github.com/thorvg/thorvg/actions/workflows/build_android.yml/badge.svg?branch=main&event=push)](https://github.com/thorvg/thorvg/actions/workflows/build_android.yml)


# ThorVG
<p align="center">
  <img width="800" height="auto" src="https://github.com/thorvg/thorvg/blob/main/res/logo/512/thorvg-banner.png">
</p>
ThorVG is an open-source graphics library designed for creating vector-based scenes and animations. Embracing the philosophy of "Simpler is better," the ThorVG project offers intuitive and user-friendly interfaces, all the while maintaining a compact size and minimal software complexity. <br />
<br />
The following list shows primitives that are supported by ThorVG: <br />
<br />
 
- **Lines & Shapes**: rectangles, circles, and paths with coordinate control
- **Filling**: solid colors, linear & radial gradients, and path clipping
- **Stroking**: stroke width, joins, caps, dash patterns, and trimming
- **Scene Management**: Retainable scene graph and object transformations
- **Composition**: various blendings and maskings
- **Text**: Unicode characters with horizontal text layout using scalable fonts (TTF)
- **Images**: TVG, SVG, JPG, PNG, WebP, and raw bitmaps
- **Animations**: Lottie

<p align="center">
  <img width="700" height="auto" src="https://github.com/thorvg/thorvg/blob/main/res/example_primitives.png">
</p>
​ThorVG is designed for a wide range of programs, offering adaptability for integration and use in various applications and systems. It achieves this through a single binary with selectively buildable, modular components in a building block style. This ensures both optimal size and easy maintenance. <br />
<br />
<p align="center">
  <img width="700" height="auto" src="https://github.com/thorvg/thorvg/blob/main/res/example_structure.png">
</p>
If your program includes the main renderer, you can seamlessly utilize ThorVG APIs by transitioning drawing contexts between the main renderer and ThorVG. Throughout these API calls, ThorVG effectively serializes drawing commands among volatile paint nodes. Subsequently, it undertakes synchronous or asynchronous rendering via its backend raster engines.<br />
<br />
ThorVG is adept at handling vector images, including formats like SVG, and it remains adaptable for accommodating additional popular formats as needed. In the rendering process, the library may generate intermediate frame buffers for scene compositing, though only when essential. The accompanying diagram provides a concise overview of how to effectively incorporate ThorVG within your system.<br />
<br />
<p align="center">
  <img width="900" height="auto" src="https://github.com/thorvg/thorvg/blob/main/res/example_flow.png">
</p>
ThorVG incorporates a threading mechanism that aims to seamlessly acquire subsequent scenes without unnecessary delays. It operates using a finely-tuned task scheduler based on thread pools, encompassing various tasks such as encoding, decoding, updating, and rendering. This design ensures that all tasks can effectively leverage multi-processing capabilities.<br />
<br />
The task scheduler has been meticulously crafted to conceal complexity, streamline integration, and enhance user convenience. Therefore, the policy it employs is optional, allowing users to select it based on their specific requirements.<br />
<br />
<p align="center">
  <img width="900" height="auto" src="https://github.com/thorvg/thorvg/blob/main/res/example_thread.png">
</p>

## Contents
- [ThorVG](#thorvg)
  - [Installation](#installation)
    - [Build and Install](#build-and-install)
    - [Build with Visual Studio](#build-with-visual-studio)
    - [Install with vcpkg](#install-with-vcpkg)
    - [Install with Conan](#install-with-conan)
    - [Install with MSYS2](#install-with-msys2)
  - [Quick Start](#quick-start)
  - [SVG](#svg)
  - [Lottie](#lottie)
  - [TVG Picture](#tvg-picture)
  - [In Practice](#in-practice)
    - [dotLottie](#dotlottie)
    - [Godot](#godot)
    - [Tizen](#tizen)
  - [Examples](#examples)
  - [Documentation](#documentation)
  - [Tools](#tools)
    - [ThorVG Viewer](#thorvg-viewer)
    - [Lottie to GIF](#lottie-to-gif)
    - [SVG to PNG](#svg-to-png)
    - [SVG to TVG](#svg-to-tvg)
  - [API Bindings](#api-bindings)
  - [Dependencies](#dependencies)
  - [Contributors](#contributors)
  - [Communication](#communication)

[](#contents)
<br />
## Installation
This section details the steps required to configure the environment for installing ThorVG.<br />
<br />
### Build and Install
ThorVG supports [meson](https://mesonbuild.com/) build system. Install [meson](http://mesonbuild.com/Getting-meson.html) and [ninja](https://ninja-build.org/) if you don't have them already.

Run meson to configure ThorVG in the thorvg root folder.
```
meson setup builddir
```
Run ninja to build & install ThorVG:
```
ninja -C builddir install
```

Regardless of the installation, all build results (symbols, executable) are generated in the builddir folder in thorvg. Some results such as examples won't be installed, you can check More examples section to see how to change it. <br/>
​<br/>
Note that some systems might include ThorVG package as a default component. In that case, you can skip this manual installation.</br>

### Build with Visual Studio
If you want to create Visual Studio project files, use the command --backend=vs. The resulting solution file (thorvg.sln) will be located in the build folder.
```
meson setup builddir --backend=vs
```

### Install with vcpkg
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

### Install with Conan
You can download and install pre-packaged ThorVG using the [Conan](https://conan.io/) package manager.

Follow the instructions on [this page on how to set up Conan](https://conan.io/downloads).

Install the ThorVG package:

```
conan install --requires="thorvg/[*]" --build=missing
```

### Install with MSYS2
You can download and install pre-packaged ThorVG using the [MSYS2](https://www.msys2.org/) package manager.

Download and execute the MSYS2 installer on the web page above and follow the steps. When done, just launch one of the terminals in the start menu, according to the architecture and compiler you want (either 32 or 64 bits, with MSVCRT or UCRT library). Then you can install the ThorVG package :

```
pacman -S thorvg
```

To update to a newer release (and update all the packages, which is preferable), run :

```
pacman -Syu
```

[Back to contents](#contents)
<br />
<br />
## Quick Start
ThorVG renders vector shapes to a given canvas buffer. The following is a quick start to show you how to use the essential APIs.

First, you should initialize the ThorVG engine:

```cpp
tvg::Initializer::init(0);   //thread count
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
rect->fill(100, 100, 100);                   //set its color (r, g, b)
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
  <img width="416" height="auto" src="https://github.com/thorvg/thorvg/blob/main/res/example_shapes.png">
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

path->fill(150, 150, 255);                   //path color

path->strokeWidth(3);                        //stroke width
path->strokeFill(0, 0, 255);                 //stroke color
path->strokeJoin(tvg::StrokeJoin::Round);    //stroke join style
path->strokeCap(tvg::StrokeCap::Round);      //stroke cap style

float pattern[2] = {10, 10};                 //stroke dash pattern (line, gap)
path->strokeDash(pattern, 2);                //set the stroke pattern

canvas->push(move(path));                    //push the path into the canvas

```

The code generates the following result:

<p align="center">
  <img width="300" height="auto" src="https://github.com/thorvg/thorvg/blob/main/res/example_path.png">
</p>

Now begin rendering & finish it at a particular time:

```cpp
canvas->draw();
canvas->sync();
```

Then you can acquire the rendered image from the buffer memory.

Lastly, terminate the engine after its usage:

```cpp
tvg::Initializer::term();
```
[Back to contents](#contents)
<br />
<br />
## SVG

ThorVG facilitates [SVG Tiny Specification](https://www.w3.org/TR/SVGTiny12/) rendering via its dedicated SVG interpreter. Adhering to the SVG Tiny Specification, the implementation maintains a lightweight profile, rendering it particularly advantageous for embedded systems. While ThorVG comprehensively adheres to most of the SVG Tiny specs, certain features remain unsupported within the current framework. These include:</br>

 - Animation 
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
  <img width="300" height="auto" src="https://github.com/thorvg/thorvg/blob/main/res/example_tiger.png">
</p>

[Back to contents](#contents)
<br />
<br />
## Lottie

ThorVG supports the most powerful Lottie Animation [features](https://lottiefiles.com/supported-features). Lottie is an industry standard, JSON-based vector animation file format that enables seamless distribution of animations on any platform, akin to shipping static assets. These files are compact and compatible with various devices, scaling up or down without pixelation. With Lottie, you can easily create, edit, test, collaborate, and distribute animations in a user-friendly manner. For more information, please visit [Lottie Animation Community](https://lottie.github.io/)' website. <br />
<br />
ThorVG offers great flexibility in building its binary. Besides serving as a general graphics engine, it can be configured as a compact Lottie animation playback library with specific build options:

```
$meson setup builddir -Dloaders="lottie"
```

Alternatively, to support additional bitmap image formats:

```
$meson setup builddir -Dloaders="lottie, png, jpg, webp"
```

Please note that ThorVG supports Lottie Expressions by default. Lottie Expressions are small JavaScript code snippets that can be applied to animated properties in your Lottie animations, evaluating to a single value. This is an advanced feature in the Lottie specification and may impact binary size and performance, especially when targeting small devices such as MCUs. If this feature is not essential for your requirements, you can disable it using the `extra` build option in ThorVG:

```
$meson setup builddir -Dloaders="lottie" -Dextra=""
```

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
  <img width="600" height="auto" src="https://github.com/thorvg/thorvg/blob/main/res/example_lottie.gif">
</p>

Please check out the [ThorVG Test App](https://thorvg-perf-test.vercel.app/) to see the performance of various Lottie animations powered by ThorVG.

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
  <img width="909" height="auto" src="https://github.com/thorvg/thorvg/blob/main/res/example_tvgsize.png">
</p>

As TVG Saver facilitates the export of the scene-tree into TVG Picture data files (TVG), the subsequent task of importing and restoring this data to programmable instances is efficiently handled by the TVG Loader. For seamless conversion from SVG to TVG, the ThorVG Viewer provides a swift solution.

<p align="center">
  <img width="710" height="auto" src="https://github.com/thorvg/thorvg/blob/main/res/example_tvgmodule.png">
</p>


[Back to contents](#contents)
<br />
<br />
## In Practice
### dotLottie
[dotLottie](https://dotlottie.io/) is an open-source file format that aggregates one or more Lottie files and their associated resources, such as images and fonts, into a single file. This enables an efficient and easy distribution of animations. dotLottie files are ZIP archives compressed with the Deflate compression method and carry the file extension of “.lottie”. Think of it as a superset of Lottie. [LottieFiles](https://lottiefiles.com/) aims to achieve just that. [dotLottie player](https://github.com/LottieFiles/dotlottie-rs) by LottieFiles is now powered by ThorVG.
<p align="center">
  <img width="798" height="auto" src="https://github.com/thorvg/thorvg/blob/main/res/example_dotlottie.png">
</p>

### Godot
ThorVG has been integrated into the [Godot](https://www.godotengine.org) project to enable the creation of sleek and visually appealing user interfaces (UIs) and vector resources in the Godot game engine. Godot is a modern game engine that is both free and open-source, offering a comprehensive range of tools. With Godot, you can concentrate on developing your game without the need to recreate existing functionalities.

<p align="center">
  <img width="798" height="auto" src="https://github.com/thorvg/thorvg/blob/main/res/example_godot.png">
</p>

### LVGL
[LVGL](https://lvgl.io/) is an open-source graphics library specifically designed for embedded systems with limited resources. It is lightweight and highly customizable, providing support for graphical user interfaces (GUIs) on microcontrollers, IoT devices, and other embedded platforms. ThorVG serves as the vector drawing primitives library in the LVGL framework.

<p align="center">
  <img width="700" height="auto" src="https://github.com/thorvg/thorvg/blob/main/res/example_lvgl.png">
</p>

### Tizen
ThorVG has been integrated into the [Tizen](https://www.tizen.org) platform as the vector graphics engine. [NUI](https://docs.tizen.org/application/dotnet/guides/user-interface/nui/overview/) is the name of Tizen UI framework which is written in C#. ThorVG is the backend engine of the [NUI Vector Graphics](https://docs.tizen.org/application/dotnet/guides/user-interface/nui/vectorgraphics/Overview/) which is used for vector primitive drawings and scalable image contents such as SVG and Lottie Animation among the Tizen applications.

<p align="center">
  <img width="798" height="auto" src="https://github.com/thorvg/thorvg/blob/main/res/example_tizen.png">
</p>

[Back to contents](#contents)
<br />
<br />
## Examples
There are plenty of sample code in `thorvg/examples` to help you in understanding the ThorVG APIs.

To execute these examples, you can build them with the following meson build option:
```
meson setup builddir -Dexamples=true
```
Note that these examples require the SDL dev package for launching. If you're using Linux-based OS, you can easily install this package from your OS distribution server. For Ubuntu, you can install it with this command.
```
apt-get install libsdl2-dev
```
Alternatively, you can read the official guidance [here](https://wiki.libsdl.org/SDL2/Installation) for other platforms. Fore more information, please visit the official [SDL](https://www.libsdl.org/) site.

[Back to contents](#contents)
<br />
<br />

## Documentation
The ThorVG API documentation can be accessed at [thorvg.org/apis](https://www.thorvg.org/apis), and is also available in the [C++ API](https://github.com/thorvg/thorvg/blob/main/inc/thorvg.h), [C API](https://github.com/thorvg/thorvg/blob/main/src/bindings/capi/thorvg_capi.h) within this repository.

[Back to contents](#contents)
<br />
<br />
## Tools
### ThorVG Viewer
ThorVG provides the resource verification tool for the ThorVG Engine. [ThorVG viewer](https://thorvg.github.io/thorvg.viewer/) does immediate rendering via web browser running on the ThorVG web-assembly binary, allowing real-time editing of the vector elements on it. It doesn't upload your resources to any external server while allowing to export to supported formats such as TVG, so the designer resource copyright is protected.</br>
</br>

<p align="center">
  <img width="700" height="auto" src="https://github.com/thorvg/thorvg/assets/3711518/edadcc5e-3bbf-489d-a9a1-9570079c7d55"/>
</p>

### Lottie to GIF
ThorVG provides an executable `lottie2gif` converter that generates a GIF file from a Lottie file.

To use the `lottie2gif`, you must turn on this feature in the build option:
```
meson setup builddir -Dtools=lottie2gif -Dsavers=gif
```
To use the 'lottie2gif' converter, you need to provide the 'Lottie files' parameter. This parameter can be a file name with the '.json' extension or a directory name. It also accepts multiple files or directories separated by spaces. If a directory is specified, the converter will search for files with the '.json' extension within that directory and all its subdirectories.<br />
<br />
Optionally, you can specify the image resolution in the 'WxH' format, with two numbers separated by an 'x' sign, following the '-r' flag.<br />
<br />
Both flags, if provided, are applied to all of the `.json` files.

The usage examples of the `lottie2gif`:
```
Usage:
    lottie2gif [Lottie file] or [Lottie folder] [-r resolution] [-f fps] [-b background color]

Flags:
    -r set the output image resolution.
    -f specifies the frames per second (fps) for the generated animation.
    -b specifies the base background color (RGB in hex). If not specified, the background color will follow the original content.

Examples:
    $ lottie2gif input.json
    $ lottie2gif input.json -f 30
    $ lottie2gif input.json -r 600x600 -f 30
    $ lottie2gif lottiefolder
    $ lottie2gif lottiefolder -r 600x600
    $ lottie2gif lottiefolder -r 600x600 -f 30 -b fa7410
```

### SVG to PNG
ThorVG provides an executable `svg2png` converter that generates a PNG file from an SVG file.

To use the `svg2png`, you must turn on this feature in the build option:
```
meson setup builddir -Dtools=svg2png
```
To use the 'svg2png' converter, you need to provide the 'SVG files' parameter. This parameter can be a file name with the '.svg' extension or a directory name. It also accepts multiple files or directories separated by spaces. If a directory is specified, the converter will search for files with the '.svg' extension within that directory and all its subdirectories.<br />
<br />
Optionally, you can specify the image resolution in the 'WxH' format, with two numbers separated by an 'x' sign, following the '-r' flag.<br />
<br />
The background color can be set with the `-b` flag. The `bgColor` parameter should be passed as a three-bytes hexadecimal value in the `ffffff` format. The default background is transparent.<br />
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
meson setup builddir -Dtools=svg2tvg -Dsavers=tvg
```

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
meson setup builddir -Dbindings="capi"
```
[Back to contents](#contents)
<br />
<br />
## Dependencies
ThorVG offers versatile support for image loading, accommodating both static and external loaders. This flexibility ensures that, even in environments without external libraries, users can still leverage static loaders as a reliable alternative. At its foundation, the ThorVG core library is engineered to function autonomously, free from external dependencies. However, it is important to note that ThorVG also encompasses a range of optional feature extensions, each with its specific set of dependencies. The dependencies associated with these selective features are outlined as follows:

* GL engine: [OpenGL v3.3](https://www.khronos.org/opengl/) or [GLES v3.0](https://www.khronos.org/opengles/)
* WG engine: [webgpu-native](https://github.com/gfx-rs/wgpu-native)
* External PNG support: [libpng](https://github.com/glennrp/libpng)
* External JPG support: [turbojpeg](https://github.com/libjpeg-turbo/libjpeg-turbo)
* External WebP support: [libwebp](https://developers.google.com/speed/webp/download)
* Examples: [SDL2](https://www.libsdl.org/)

[Back to contents](#contents)
<br />
<br />
## Contributors
ThorVG stands as a purely open-source initiatives. We are grateful to the individuals, organizations, and companies that have contributed to the development of the ThorVG project. The dedicated efforts of the individuals and entities listed below have enabled ThorVG to reach its current state.

* [Individuals](https://github.com/thorvg/thorvg/blob/main/AUTHORS)
* [LottieFiles](https://lottiefiles.com/) by Design Barn Inc.
* Samsung Electronics Co., Ltd

[Back to contents](#contents)
<br />
<br />
## Communication
For real-time conversations and discussions, please join us on [Discord](https://discord.gg/n25xj6J6HM)

[Back to contents](#contents)
