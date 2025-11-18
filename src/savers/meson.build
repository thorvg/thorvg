subsaver_dep = []

if gif_saver
    subdir('gif')
endif

if png_saver
    if get_option('static')
        subdir('png')
    else
        subdir('external_png')
        if not png_dep.found()
            subdir('png')
        endif
    endif
endif

saver_dep = declare_dependency(
   dependencies: subsaver_dep,
   include_directories : include_directories('.'),
)
