compiler_flags = ['-DTVG_BUILD']

subdir('lib')
subdir('loaders')
subdir('examples')

thread_dep = meson.get_compiler('cpp').find_library('pthread')
thorvg_lib_dep = [common_dep, loader_dep, thread_dep]

thorvg_lib = library(
	'thorvg',
	include_directories    : headers,
	version                : meson.project_version(),
	dependencies           : thorvg_lib_dep,
	install                : true,
	cpp_args               : compiler_flags,
	gnu_symbol_visibility  : 'hidden',
)

thorvg_dep = declare_dependency(
	include_directories: headers,
	link_with : thorvg_lib
)

pkg_mod = import('pkgconfig')

pkg_mod.generate(
	libraries    : thorvg_lib,
	version      : meson.project_version(),
	name         : 'libthorvg',
	filebase     : 'thorvg',
	description  : 'A Thor library for rendering vector graphics'
)
