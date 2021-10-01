subloader_dep = []

if get_option('loaders').contains('tvg') == true
    subdir('tvg')
endif

if get_option('loaders').contains('svg') == true
    subdir('svg')
endif

if get_option('loaders').contains('png') == true
    png_dep = meson.get_compiler('cpp').find_library('libpng')
    if png_dep.found()
        message('libpng is found.')
        subdir('dynamic_png')
    else
        subdir('png')
    endif
endif

if get_option('loaders').contains('jpg') == true
    jpg_dep = dependency('libturbojpeg', required: false)
    if not jpg_dep.found()
        jpg_dep = cc.find_library('turbojpeg', required: false)
    endif
    if jpg_dep.found()
        message('libjpeg-turbo is found.')
        subdir('dynamic_jpg')
    else
        subdir('jpg')
    endif
endif

subdir('raw')

loader_dep = declare_dependency(
   dependencies: subloader_dep,
   include_directories : include_directories('.'),
)
