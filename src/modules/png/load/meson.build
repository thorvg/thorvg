if get_option('static')
    subdir('internal')
else
    subdir('external')
    if not png_dep.found()
        subdir('internal')
    endif
endif
