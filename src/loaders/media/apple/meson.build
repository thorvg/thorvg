compiler_flags += ['-DTHORVG_APPLE_MEDIA_SUPPORT=1']
add_languages('objcpp', native : false, required : true)

source_file = [
   'tvgAvfMediaLoader.h',
   'tvgAvfMediaLoader.mm'
]

media_dep += [declare_dependency(
    dependencies : dependency('appleframeworks', modules : ['AVFoundation', 'CoreVideo', 'CoreMedia', 'Foundation', 'CoreFoundation']),
    include_directories : include_directories('.'),
    sources : source_file
)]
