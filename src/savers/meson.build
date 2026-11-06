subsaver_dep = []

if gif_saver
    subdir('gif')
endif

if svg_saver
    subdir('svg')
endif

saver_dep = declare_dependency(
   dependencies: subsaver_dep,
   include_directories : include_directories('.'),
)
