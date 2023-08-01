source_file = [
   'tvgWebpLoader.h',
   'tvgWebpLoader.cpp',
]

webp_dep = dependency('libwebp', required: true)

subloader_dep += [declare_dependency(
    include_directories : include_directories('.'),
    dependencies : webp_dep,
    sources : source_file
    )]