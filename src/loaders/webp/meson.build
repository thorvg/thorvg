webp_static_dep = []

subdir('libwebp')

source_file = [
   'tvgWebpLoader.h',
   'tvgWebpLoader.cpp',
]

subloader_dep += [declare_dependency(
    include_directories : include_directories('.'),
    dependencies : webp_static_dep,
    sources : source_file
    )]