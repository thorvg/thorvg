subloader_dep = []

if get_option('loaders').contains('svg') == true
    subdir('svg')
    message('Enable SVG Loader')
endif

if get_option('loaders').contains('raw') == true
    subdir('raw')
    message('Enable RAW(ARGB) Loader')
endif

loader_dep = declare_dependency(
   dependencies: subloader_dep,
   include_directories : include_directories('.'),
)
