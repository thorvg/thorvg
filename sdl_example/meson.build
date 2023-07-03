
sdl_dep = dependency('sdl2', required: true)

executable('sdl_hello_world', 'hello_world.cc',
        include_directories : headers,
        link_with : thorvg_lib,
        dependencies : sdl_dep)

if get_option('engines').contains('gl_beta') == true
    gl_dep = dependency('opengl')

    executable('gl_example',
        'gl_example.cc',
        include_directories : headers,
        link_with: thorvg_lib,
        dependencies: [sdl_dep, gl_dep],
    )
endif