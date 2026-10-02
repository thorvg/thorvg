if get_option('static')
    subdir('internal')
else
    subdir('external')
    if not webp_dep.found()
        subdir('internal')
    endif
endif
