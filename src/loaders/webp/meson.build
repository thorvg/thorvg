if get_option('static')
    subdir('static')
else
    subdir('external')
    if not webp_dep.found()
        subdir('static')
    endif
endif
