compiler_flags += ['-DTHORVG_ANDROID_MEDIA_SUPPORT=1']

source_file = [
   'tvgAndroidMediaLoader.h',
   'tvgAndroidMediaLoader.cpp'
]

media_dep += [declare_dependency(
    dependencies : [cc.find_library('mediandk'), cc.find_library('OpenSLES')],
    include_directories : include_directories('.'),
    sources : source_file
)]
