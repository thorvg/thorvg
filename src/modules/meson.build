subloader_dep = []
subsaver_dep = []

subdir('gif')
subdir('jpg')
subdir('lottie')
subdir('media')
subdir('png')
subdir('raw')
subdir('sfnt')
subdir('svg')
subdir('webp')

loader_dep = declare_dependency(
    dependencies: subloader_dep,
    include_directories : include_directories('.'),
)

saver_dep = declare_dependency(
    dependencies: subsaver_dep,
    include_directories : include_directories('.'),
)
