media_inc = ['.']
system = host_machine.system()

source_file = [
   'tvgMediaLoader.h',
   'tvgVideo.cpp'
]

media_dep = []
if system == 'darwin' or system == 'ios'
    subdir('apple')
elif system == 'emscripten'
    subdir('web')
elif system == 'linux'
    subdir('linux')
else
    # Use the fallback factory when no platform-specific media backend is available.
    source_file += ['tvgMediaLoader.cpp']
endif

subloader_dep += [declare_dependency(
    dependencies : media_dep,
    include_directories : include_directories(media_inc),
    sources : source_file
)]

install_headers('thorvg_media.h', subdir: 'thorvg-' + vmaj)
headers += include_directories('.')
