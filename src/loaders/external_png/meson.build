source_file = [
   'tvgPngLoader.h',
   'tvgPngLoader.cpp',
]

png_dep = meson.get_compiler('cpp').find_library('libpng')
zlib_dep = meson.get_compiler('cpp').find_library('zlibstatic')

if png_dep.found()
    subloader_dep += [declare_dependency(
        include_directories : include_directories('.'),
        dependencies : [png_dep, zlib_dep],
        sources : source_file
    )]
endif
