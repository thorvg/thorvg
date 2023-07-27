
pymod = import('python')

py3 = pymod.find_installation('python3', required: true)

shader_list = files(
    'color.frag',
    'color.vert',
    'gradient.vert',
    'linear_gradient.frag',
    'image.frag',
    'image.vert',
    'radial_gradient.frag',
    'stencil.frag',
    'stencil.vert',
)

gl_shader_source = custom_target(
        'gl_shader_source',
        output: 'gl_shader_source.h',
        input: [ 'shader_packer.py' ],
        command: [
            py3, '@INPUT@', '@OUTPUT@', shader_list
        ],
)

gl_shader_dep = declare_dependency(
    sources: [ gl_shader_source ],
)
