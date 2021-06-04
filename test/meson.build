test_file = [
    'testMain.cpp',
    'testInitializer.cpp',
    'testSwCanvas.cpp',
    'testSwCanvasBase.cpp',
    'capi/testRadialGradient.cpp'
]

tests = executable('tvgUnitTests',
    test_file,
    include_directories : headers,
    link_with : thorvg_lib)

test('Unit Tests', tests)
