compiler_flags = ['-DTVG_BUILD']

cc = meson.get_compiler('cpp')
if (cc.get_id() != 'msvc')
    if get_option('vectors').contains('avx')
        compiler_flags += ['-mavx']
        message('Enable Advanced Vector Extension')
    endif
    compiler_flags += ['-fno-exceptions', '-fno-rtti',
                       '-fno-unwind-tables' , '-fno-asynchronous-unwind-tables',
                       '-Woverloaded-virtual', '-Wno-unused-parameter']
endif


subdir('lib')
subdir('loaders')
subdir('bindings')

thread_dep = meson.get_compiler('cpp').find_library('pthread')
thorvg_lib_dep = [common_dep, loader_dep, binding_dep, thread_dep]

if get_option('png') == true
    message('Enable libpng based png loading')
    png_dep = meson.get_compiler('cpp').find_library('libpng')
    thorvg_lib_dep += png_dep
endif

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

if (cc.get_id() == 'emscripten')

  subdir('wasm')

  executable('thorvg-wasm',
              [],
              dependencies : [thorvg_dep, thorvg_wasm_dep],
            )
endif

pkg_mod = import('pkgconfig')

pkg_mod.generate(
	libraries    : thorvg_lib,
	version      : meson.project_version(),
	name         : 'libthorvg',
	filebase     : 'thorvg',
	description  : 'A Thor library for rendering vector graphics'
)

subdir('bin')

if get_option('examples') == true
    message('Enable Examples')
    subdir('examples')
endif
