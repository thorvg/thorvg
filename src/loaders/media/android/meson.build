media_inc += ['android']

source_file += files(
   'tvgAndroidMediaDecoder.h',
   'tvgAndroidMediaDecoder.cpp',
   'tvgAndroidMediaLoader.h',
   'tvgAndroidMediaLoader.cpp'
)

media_dep += [
    cc.find_library('mediandk'),
    cc.find_library('OpenSLES')
]
