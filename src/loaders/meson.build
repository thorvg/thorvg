subloader_dep = []

if get_option('loaders').contains('svg') == true
    subdir('svg')
    message('Enable SVG Loader')
endif

if get_option('loaders').contains('png') == true
    subdir('png')
    message('Enable PNG Loader')
endif

subdir('raw')

loader_dep = declare_dependency(
   dependencies: subloader_dep,
   include_directories : include_directories('.'),
)
