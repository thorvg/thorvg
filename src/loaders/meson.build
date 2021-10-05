subloader_dep = []

if all_loaders or get_option('loaders').contains('tvg') == true
    subdir('tvg')
endif

if all_loaders or get_option('loaders').contains('svg') == true
    subdir('svg')
endif

if all_loaders or get_option('loaders').contains('png') == true
    subdir('external_png')
    if not png_dep.found()
        subdir('png')
    endif
endif

if all_loaders or get_option('loaders').contains('jpg') == true
    subdir('external_jpg')
    if not jpg_dep.found()
        subdir('jpg')
    endif
endif

subdir('raw')

loader_dep = declare_dependency(
    dependencies: subloader_dep,
    include_directories : include_directories('.'),
)
