subloader_dep = []

if all_loaders or get_option('loaders').contains('tvg') == true
    subdir('tvg')
endif

if all_loaders or get_option('loaders').contains('svg') == true
    subdir('svg')
endif

if get_option('loaders').contains('ttf_beta') == true
    subdir('ttf')
endif

if all_loaders or get_option('loaders').contains('lottie') == true
    subdir('lottie')
endif

if all_loaders or get_option('loaders').contains('png') == true
    if get_option('static') == true
        subdir('png')
    else
        subdir('external_png')
        if not png_dep.found()
            subdir('png')
        endif
    endif
endif

if all_loaders or get_option('loaders').contains('jpg') == true
    if get_option('static') == true
        subdir('jpg')
    else
        subdir('external_jpg')
        if not jpg_dep.found()
            subdir('jpg')
        endif
    endif
endif

if get_option('loaders').contains('webp_beta') == true
    if get_option('static') == true
        message('static webp is not available, disable webp_beta loader.')
    else
        subdir('external_webp')
    endif
endif

subdir('raw')

loader_dep = declare_dependency(
    dependencies: subloader_dep,
    include_directories : include_directories('.'),
)
