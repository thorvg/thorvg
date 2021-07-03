subloader_dep = []

if get_option('loaders').contains('svg') == true
    subdir('svg')
endif

if get_option('loaders').contains('png') == true
    subdir('png')
endif

if get_option('loaders').contains('tvg') == true
    subdir('tvg')
endif

subdir('raw')

loader_dep = declare_dependency(
   dependencies: subloader_dep,
   include_directories : include_directories('.'),
)
