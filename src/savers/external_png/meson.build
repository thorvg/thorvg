source_file = [
  'tvgPngSaver.cpp', 
  'tvgPngSaver.h'
]

png_dep = dependency('libpng', required: false)

if png_dep.found()
    subsaver_dep += [declare_dependency(
        include_directories : include_directories('.'),
        dependencies : png_dep,
        sources : source_file
    )]
endif