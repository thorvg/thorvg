source_file = [
   'tvgPngLoader.h',
   'tvgPngLoader.cpp',
]

png_dep = dependency('libpng', required: false)

if not png_dep.found()
    png_dep = cc.find_library('libpng', required: false)
endif

if png_dep.found()
    subloader_dep += [declare_dependency(
        include_directories : include_directories('.'),
        dependencies : png_dep,
        sources : source_file
    )]
endif
