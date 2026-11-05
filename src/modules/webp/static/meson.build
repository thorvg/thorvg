webp_deb = []

subdir('dec')
subdir('dsp')
subdir('utils')
subdir('webp')

source_file = [
   'tvgWebpLoader.h',
   'tvgWebpLoader.cpp',
]

submodule_dep += [declare_dependency(
    include_directories : include_directories('.'),
    dependencies : webp_deb,
    sources : source_file
    )]
