if lib_type == 'static'
    compiler_flags += ['-DTVG_STATIC']
endif

test_file = [
    'testAccessor.cpp',
    'testAnimation.cpp',
    'testFill.cpp',
    'testInitializer.cpp',
    'testLottie.cpp',
    'testMain.cpp',
    'testPaint.cpp',
    'testPicture.cpp',
    'testSavers.cpp',
    'testScene.cpp',
    'testShape.cpp',
    'testSwCanvas.cpp',
    'testSwEngine.cpp',
    'testText.cpp'
]

tests = executable('tvgUnitTests',
    test_file,
    include_directories : headers,
    link_with : thorvg_lib,
    cpp_args : compiler_flags)

test('Unit Tests', tests, args : ['--success'])

if get_option('bindings').contains('capi')
    subdir('capi')
endif
