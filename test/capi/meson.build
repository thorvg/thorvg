test_file = [
   'capiInitializer.cpp',
   'capiFill.cpp',
   'capiLinearGradient.cpp',
   'capiMain.cpp',
   'capiPaint.cpp',
   'capiRadialGradient.cpp',
   'capiScene.cpp',
   'capiShape.cpp',
   'capiSwCanvas.cpp',
]

tests = executable('capiUnitTests',
    test_file,
    include_directories : headers,
    link_with : thorvg_lib)

test('Capi Unit Tests', tests)
