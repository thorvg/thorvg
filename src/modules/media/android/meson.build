if cc.get_define('__ANDROID_API__').to_int() < 24
    error('The Android media loader requires API 24 or later')
endif

media_inc += ['android']

source_file += files(
   'tvgAndroidMediaDecoder.h',
   'tvgAndroidMediaDecoder.cpp',
   'tvgAndroidMediaAudioSink.h',
   'tvgAndroidMediaAudioSink.cpp',
   'tvgAndroidMediaLoader.h',
   'tvgAndroidMediaLoader.cpp'
)

media_dep += [
    cc.find_library('mediandk'),
    cc.find_library('OpenSLES')
]
