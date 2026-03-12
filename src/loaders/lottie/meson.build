if lottie_exp
    subdir('jerryscript')
endif

source_file = [
   'tvgLottieBuilder.h',
   'tvgLottieData.h',
   'tvgLottieExpressions.h',
   'tvgLottieInterpolator.h',
   'tvgLottieLoader.h',
   'tvgLottieModel.h',
   'tvgLottieModifier.h',
   'tvgLottieParser.h',
   'tvgLottieParserHandler.h',
   'tvgLottieProperty.h',
   'tvgLottieRenderPooler.h',
   'tvgLottieFlatBufferParser.h',
   'tvgLottieAnimation.cpp',
   'tvgLottieBuilder.cpp',
   'tvgLottieExpressions.cpp',
   'tvgLottieInterpolator.cpp',
   'tvgLottieLoader.cpp',
   'tvgLottieModel.cpp',
   'tvgLottieModifier.cpp',
   'tvgLottieParserHandler.cpp',
   'tvgLottieParser.cpp',
   'tvgLottieFlatBufferParser.cpp'
]

subloader_dep += [declare_dependency(
    include_directories : include_directories('.'),
    sources : source_file
)]

install_headers('thorvg_lottie.h', subdir: 'thorvg-' + vmaj)
headers += include_directories('.')
