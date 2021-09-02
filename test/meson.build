test_file = [
    'testFill.cpp',
    'testInitializer.cpp',
    'testMain.cpp',
    'testPaint.cpp',
    'testPicture.cpp',
    'testSavers.cpp',
    'testScene.cpp',
    'testShape.cpp',
    'testSwCanvas.cpp',
    'testSwCanvasBase.cpp',
    'testSwEngine.cpp',
]

tests = executable('tvgUnitTests',
    test_file,
    include_directories : headers,
    link_with : thorvg_lib)

test('Unit Tests', tests, args : ['--success'])

if get_option('bindings').contains('capi') == true
    subdir('capi')
endif
