submodule_dep = []

if svg_loader
    subdir('svg')
endif

if ttf_loader or otf_loader
    subdir('sfnt')
endif

if lottie_loader
    subdir('lottie')
endif

if png_loader
    subdir('png')
endif

if jpg_loader
    subdir('jpg')
endif

if webp_loader
    subdir('webp')
endif

if media_loader
    subdir('media')
endif

subdir('raw')

if gif_saver
    subdir('gif')
endif

module_dep = declare_dependency(
    dependencies: submodule_dep,
    include_directories : include_directories('.'),
)
