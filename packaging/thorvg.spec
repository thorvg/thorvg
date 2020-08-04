Name:       thorvg
Summary:    Thor Vector Graphics Library
Version:    0.0.1
Release:    1
Group:      Graphics System/Rendering Engine
License:    Apache-2.0
URL:        https://github.com/samsung/thorvg
Source0:    %{name}-%{version}.tar.gz

BuildRequires:  pkgconfig
BuildRequires:  pkgconfig(glesv2)

BuildRequires:  meson
BuildRequires:  ninja
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig

%description
Thor Vector Graphics Library


%package devel
Summary:    Thor Vector Graphics Library (devel)
Group:      Development/Libraries
Requires:   %{name} = %{version}-%{release}


%description devel
Thor Vector Graphics Library (devel)


%prep
%setup -q


%build

export DESTDIR=%{buildroot}

meson setup \
      --prefix /usr \
      --libdir %{_libdir} \
      builddir 2>&1
ninja \
      -C builddir \
      -j %(echo "`/usr/bin/getconf _NPROCESSORS_ONLN`")

%install

export DESTDIR=%{buildroot}

ninja -C builddir install

%files
%defattr(-,root,root,-)
%{_libdir}/libthorvg.so.*
%manifest packaging/thorvg.manifest

%files devel
%defattr(-,root,root,-)
%{_includedir}/*.h
%{_libdir}/libthorvg.so

%{_libdir}/pkgconfig/thorvg.pc
