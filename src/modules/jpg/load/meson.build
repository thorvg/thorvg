if get_option('static')
    subdir('internal')
else
    subdir('external')
    if not jpg_dep.found()
        subdir('internal')
    endif
endif
