subsaver_dep = []

if all_savers or get_option('savers').contains('tvg') == true
    subdir('tvg')
endif

if all_savers or get_option('savers').contains('gif') == true
    subdir('gif')
endif

saver_dep = declare_dependency(
   dependencies: subsaver_dep,
   include_directories : include_directories('.'),
)
