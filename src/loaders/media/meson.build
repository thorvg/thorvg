media_inc = ['.']
system = host_machine.system()

media_dep = []

source_file = [
   'tvgMediaLoader.h',
   'tvgMediaLoader.cpp',
   'tvgVideo.cpp'
]

subloader_dep += [declare_dependency(
    dependencies : media_dep,
    include_directories : include_directories(media_inc),
    sources : source_file
)]

install_headers('thorvg_media.h', subdir: 'thorvg-' + vmaj)
headers += include_directories('.')