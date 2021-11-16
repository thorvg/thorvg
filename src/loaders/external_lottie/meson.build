source_file = [
   'tvgLottieLoader.h',
   'tvgLottieLoader.cpp',
]

#Temporary
rlottie_dep = dependency('rlottie', required: false)

subloader_dep += [declare_dependency(
    include_directories : include_directories('.'),
    dependencies : rlottie_dep,
    sources : source_file
)]

