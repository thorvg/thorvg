source_file = [
   'tvgGifSaver.h',
   'tvgGifSaver.cpp',
   'tvgGifEncoder.h',
   'tvgGifEncoder.cpp',
]

gif_dep_found = false
gif_dep = dependency('gif', required: false)

if not gif_dep.found()
    gif_dep = cc.find_library('gif', required: false)
endif

gif_dep_found = gif_dep.found()

if gif_dep_found 
    if not cc.has_function('GifQuantizeBuffer', dependencies: gif_dep)
        gif_dep_found = false
    else
        subsaver_dep += [declare_dependency(
            include_directories : include_directories('.'),
            dependencies : gif_dep,
            sources : source_file
        )]
    endif
endif
