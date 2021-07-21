subsaver_dep = []

if get_option('savers').contains('tvg') == true
    subdir('tvg')
endif

saver_dep = declare_dependency(
   dependencies: subsaver_dep,
   include_directories : include_directories('.'),
)
