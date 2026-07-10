source_file = [
   'tvgGifLoader.h',
   'tvgGifLoader.cpp',
]

gif_dep = dependency('libgif', required: false)

if not gif_dep.found()
    gif_dep = cc.find_library('gif', required: false)
endif

if gif_dep.found()
    subloader_dep += [declare_dependency(
        include_directories : include_directories('.'),
        dependencies : gif_dep,
        sources : source_file
    )]
endif
