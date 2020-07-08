subloader_dep = []

if get_option('loaders').contains('svg') == true
    subdir('svg_loader')
    message('Enable SVG Loader')
endif

loader_dep = declare_dependency(
   dependencies: subloader_dep,
   include_directories : include_directories('.'),
)
