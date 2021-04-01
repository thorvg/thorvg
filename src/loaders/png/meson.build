source_file = [
   'tvgPngLoader.h',
   'tvgPngLoader.cpp',
]

png_dep = meson.get_compiler('cpp').find_library('libpng')

subloader_dep += [declare_dependency(
    include_directories : include_directories('.'),
    dependencies : png_dep,
    sources : source_file
)]
