
sdl_dep = dependency('sdl2', required: true)

executable('sdl_hello_world', 'hello_world.cc',
        include_directories : headers,
        link_with : thorvg_lib,
        dependencies : sdl_dep)