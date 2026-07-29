add_languages('objcpp', native : false, required : true)

source_file += files(
   'tvgAvfMediaLoader.h',
   'tvgAvfMediaLoader.mm'
)

media_dep += [dependency(
    'appleframeworks',
    modules : ['AVFoundation', 'CoreVideo', 'CoreMedia', 'Foundation', 'CoreFoundation']
)]
