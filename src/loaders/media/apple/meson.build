compiler_flags += ['-DTHORVG_APPLE_MEDIA_SUPPORT=1']

source_file = [
   'tvgAvfMediaLoader.h'
]

media_dep += [declare_dependency(
    dependencies : dependency('appleframeworks', modules : ['AVFoundation']),
    include_directories : include_directories('.'),
    sources : source_file
)]
