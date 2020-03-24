compiler_flags = ['-DTIZENVG_BUILD']

subdir('lib')
subdir('examples')

tizenvg_lib_dep = [ src_dep ]

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
