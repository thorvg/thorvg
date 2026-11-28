if png_loader
    if get_option('static')
        subdir('static')
    else
        subdir('external')
        if not png_dep.found()
            subdir('static')
        endif
    endif
endif

if png_saver
    subdir('save')
endif
