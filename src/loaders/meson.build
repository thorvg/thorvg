subloader_dep = []

if all_loaders or get_option('loaders').contains('svg') == true
    subdir('svg')
endif

if all_loaders or get_option('loaders').contains('png') == true
    subdir('png')
endif

if all_loaders or get_option('loaders').contains('tvg') == true
    subdir('tvg')
endif

if all_loaders or get_option('loaders').contains('jpg') == true
    subdir('jpg')
endif

subdir('raw')

loader_dep = declare_dependency(
   dependencies: subloader_dep,
   include_directories : include_directories('.'),
)
