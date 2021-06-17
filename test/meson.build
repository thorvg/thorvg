test_file = [
    'testMain.cpp',
    'testInitializer.cpp',
    'testScene.cpp',
    'testSwCanvas.cpp',
    'testSwCanvasBase.cpp',
]

tests = executable('tvgUnitTests',
    test_file,
    include_directories : headers,
    link_with : thorvg_lib)

test('Unit Tests', tests)

if get_option('bindings').contains('capi') == true
    subdir('capi')
endif
