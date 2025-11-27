source_file = [
   'tvgGifLoader.h',
   'tvgGifLoader.cpp',
]

gif_dep = dependency('gif', required: false)

if gif_dep.found()
    subloader_dep += [declare_dependency(
        include_directories : include_directories('.'),
        dependencies : gif_dep,
        sources : source_file
    )]
endif
