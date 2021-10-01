subloader_dep = []

if get_option('loaders').contains('tvg') == true
    subdir('tvg')
endif

if get_option('loaders').contains('svg') == true
    subdir('svg')
endif

subdir('external_png')

if not png_dep.found()
    subdir('png')
endif

subdir('external_jpg')

if not jpg_dep.found()
    subdir('jpg')
endif

subdir('raw')

loader_dep = declare_dependency(
    dependencies: subloader_dep,
    include_directories : include_directories('.'),
)
