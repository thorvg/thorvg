Name:           thorvg
Version:        0.15.12
Release:        %{autorelease}
Summary:        Lightweight vector-based scenes and animation drawing library

License:        MIT
URL:            https://www.thorvg.org/
Source:         https://github.com/thorvg/thorvg/archive/v%{version}/thorvg-%{version}.tar.gz
# Give binaries names that are not generic to prevent conflicts
# https://github.com/thorvg/thorvg/issues/3367
Patch:          binarynameconflicts.patch

BuildRequires:  gcc
BuildRequires:  gcc-c++
BuildRequires:  mesa-libGL-devel
BuildRequires:  meson
BuildRequires:  ninja-build
BuildRequires:  pkgconfig(libturbojpeg)
BuildRequires:  pkgconfig(libpng)
BuildRequires:  pkgconfig(libwebp)
BuildRequires:  pkgconfig(sdl2)
# Documentation
BuildRequires:  doxygen

# Modifications do not allow for use of packaged rapidjson
# https://github.com/thorvg/thorvg/issues/3358
Provides: bundled(rapidjson)

%description
ThorVG is an open-source graphics library designed for creating vector-based
scenes and animations. It combines immense power with remarkable lightweight
efficiency, as Thor embodies a dual meaning—symbolizing both thunderous
strength and lightning-fast agility. Embracing the philosophy of simpler is
better, the ThorVG project provides intuitive, user-friendly interfaces while
maintaining a compact footprint and minimal overhead.

The following list shows primitives that are supported by ThorVG:
- Lines & Shapes: rectangles, circles, and paths with coordinate control
- Filling: solid colors, linear & radial gradients, and path clipping
- Stroking: stroke width, joins, caps, dash patterns, and trimming
- Scene Management: retainable scene graph and object transformations
- Composition: various blending and masking
- Text: unicode characters with horizontal text layout using scalable fonts (TTF)
- Images: SVG, JPG, PNG, WebP, and raw bitmaps
- Effects: blur, drop shadow, fill, tint, tritone and color replacement
- Animations: Lottie

%package devel
Summary: Development headres and libraries
Requires: %{name}%{?_isa} = %{version}-%{release}

%description devel
Development headers and libraries for ThorVG.

%package doc
Summary: Documentation
BuildArch:  noarch

%description doc
HTML documentation for ThorVG.

%prep
%autosetup -n thorvg-%{version} -p1

%build
%meson -Dengines=all \
       -Dexamples=true \
       -Dloaders=all \
       -Dsavers=all \
       -Dbindings=capi \
       -Dtools=all \
       -Dstrip=false \
       -Dtests=true
%meson_build
# documentation
pushd docs
doxygen
popd

%install
%meson_install

%check
%meson_test

%files
%license LICENSE
%doc README.md
%doc AUTHORS
%doc CONTRIBUTING.md
%{_libdir}/libthorvg.so.0
%{_libdir}/libthorvg.so.0.*
%{_bindir}/tvg-svg2png
%{_bindir}/tvg-svg2tvg
%{_bindir}/tvg-lottie2gif

%files devel
%{_includedir}/thorvg.h
%{_includedir}/thorvg_capi.h
%{_includedir}/thorvg_lottie.h
%{_libdir}/libthorvg.so
%{_libdir}/pkgconfig/thorvg.pc

%files doc
%license LICENSE
%doc docs/html

%changelog
%autochangelog
