test_file = [
   'capiAnimation.cpp',
   'capiInitializer.cpp',
   'capiFill.cpp',
   'capiLinearGradient.cpp',
   'capiLottie.cpp',
   'capiMain.cpp',
   'capiPaint.cpp',
   'capiPicture.cpp',
   'capiRadialGradient.cpp',
   'capiSavers.cpp',
   'capiScene.cpp',
   'capiShape.cpp',
   'capiSwCanvas.cpp',
   'capiText.cpp'
]

tests = executable('capiUnitTests',
    test_file,
    include_directories : headers,
    link_with : thorvg_lib,
    cpp_args : compiler_flags)

test('Capi Unit Tests', tests)
