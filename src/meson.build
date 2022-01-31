compiler_flags = ['-DTVG_BUILD']

cc = meson.get_compiler('cpp')
if (cc.get_id() != 'msvc')
    if simd_type == 'avx'
        compiler_flags += ['-mavx']
    endif
    if simd_type == 'neon'
        compiler_flags += ['-mfpu=neon']
    endif
    if get_option('b_sanitize') == 'none'
        compiler_flags += ['-s', '-fno-exceptions', '-fno-rtti', '-fno-stack-protector', '-fno-math-errno',
                           '-fno-unwind-tables' , '-fno-asynchronous-unwind-tables',
                           '-Woverloaded-virtual', '-Wno-unused-parameter']
    endif
endif


subdir('lib')
subdir('loaders')
subdir('savers')
subdir('bindings')

thorvg_lib_dep = [common_dep, loader_dep, saver_dep, binding_dep]
if host_machine.system() != 'windows'
    thread_dep = meson.get_compiler('cpp').find_library('pthread')
    thorvg_lib_dep += [thread_dep]
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
        include_directories : headers,
        dependencies : [thorvg_lib_dep, thorvg_wasm_dep],
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
    subdir('examples')
endif
