if lottie_expressions
    subdir('jerryscript')
endif

source_file = [
   'tvgLottieAnimation.cpp',
   'tvgLottieBuilder.h',
   'tvgLottieInterpolator.h',
   'tvgLottieLoader.h',
   'tvgLottieModel.h',
   'tvgLottieParser.h',
   'tvgLottieParserHandler.h',
   'tvgLottieProperty.h',
   'tvgLottieBuilder.cpp',
   'tvgLottieInterpolator.cpp',
   'tvgLottieLoader.cpp',
   'tvgLottieModel.cpp',
   'tvgLottieParserHandler.cpp',
   'tvgLottieParser.cpp'
]

subloader_dep += [declare_dependency(
    include_directories : include_directories('.'),
    sources : source_file
)]

install_headers('thorvg_lottie.h')
headers += include_directories('.')