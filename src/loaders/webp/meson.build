webp_deb = []

subdir('dec')
subdir('dsp')
subdir('utils')
subdir('webp')

source_file = [
    'tvgWebpLoader.h',
    'tvgWebpLoader.cpp',
]

dynamic_source_file = [
    'tvgWebpDynamicLoader.cpp',
]

if get_option('module')
    webp_loader = shared_module('thorvg-webp-loader',
        dynamic_source_file,
        include_directories : include_directories('.'),
        dependencies : webp_deb,
        install : true,
        cpp_args : compiler_flags,
        gnu_symbol_visibility : 'hidden',
    )

    webp_dep = declare_dependency(
        include_directories : include_directories('.'),
        sources : source_file
    )
else
webp_dep = declare_dependency(
    include_directories : include_directories('.'),
    dependencies : webp_deb,
    sources : source_file + dynamic_source_file
)
endif

subloader_dep += [webp_dep]
