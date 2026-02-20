subsaver_dep = []

if gif_saver
    if get_option('static')
        subdir('gif')
    else 
        subdir('external_gif')
        if not gif_dep_found
            subdir('gif')
        endif
    endif
endif

saver_dep = declare_dependency(
   dependencies: subsaver_dep,
   include_directories : include_directories('.'),
)
