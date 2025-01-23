source_file = [
    'tvgWebpLoader.h',
    'tvgWebpLoader.cpp',
]

dynamic_source_file = [
    'tvgWebpDynamicLoader.cpp',
]

webp_dep = dependency('libwebp', required: false)
loader_deps = [webp_dep]
dynamic_webp_dep = []

if get_option('module')
    ext_webp_loader = shared_module('thorvg-webp-loader',
        dynamic_source_file,
        include_directories : include_directories('.'),
        dependencies : loader_deps,
        install : true,
        cpp_args : compiler_flags,
        gnu_symbol_visibility : 'hidden',
    )

    ext_webp_dep = declare_dependency(
        include_directories : include_directories('.'),
        sources : source_file
    )
else
ext_webp_dep = declare_dependency(
    include_directories : include_directories('.'),
    dependencies : loader_deps,
    sources : source_file + dynamic_source_file
)
endif



subloader_dep += [ext_webp_dep]
