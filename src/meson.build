compiler_flags = ['-DTVG_BUILD']

subdir('lib')
subdir('loaders')
subdir('examples')

m_dep = meson.get_compiler('cpp').find_library('m')
thread_dep = meson.get_compiler('cpp').find_library('pthread')
egl_dep = meson.get_compiler('cpp').find_library('EGL')
gles_dep = meson.get_compiler('cpp').find_library('GLESv2')

tizenvg_lib_dep = [ src_dep, swengine_dep, glengine_dep, m_dep, egl_dep, gles_dep, svgloader_dep, thread_dep]


tizenvg_lib = library(
	'tizenvg',
	include_directories    : headers,
	version                : meson.project_version(),
	dependencies           : tizenvg_lib_dep,
	install                : true,
	cpp_args               : compiler_flags,
	gnu_symbol_visibility  : 'hidden',
)

tizenvg_dep = declare_dependency(
	include_directories: headers,
	link_with : tizenvg_lib
)

pkg_mod = import('pkgconfig')

pkg_mod.generate(
	libraries    : tizenvg_lib,
	version      : meson.project_version(),
	name         : 'libtizenvg',
	filebase     : 'tizenvg',
	description  : 'A Tizen library for rendering vector graphics'
)
