compiler_flags += ['-DTHORVG_ANDROID_MEDIA_SUPPORT=1']

source_file = [
   'tvgAndroidMediaDecoder.h',
   'tvgAndroidMediaDecoder.cpp',
   'tvgAndroidMediaLoader.h',
   'tvgAndroidMediaLoader.cpp',
   'tvgAndroidMediaScheduler.h',
   'tvgAndroidMediaScheduler.cpp'
]

media_dep += [declare_dependency(
    dependencies : [
        cc.find_library('mediandk'),
        cc.find_library('OpenSLES')
    ],
    include_directories : include_directories('.'),
    sources : source_file
)]
