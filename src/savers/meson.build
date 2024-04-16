subsaver_dep = []

if tvg_saver
    subdir('tvg')
endif

if gif_saver
    subdir('gif')
endif

saver_dep = declare_dependency(
   dependencies: subsaver_dep,
   include_directories : include_directories('.'),
)
