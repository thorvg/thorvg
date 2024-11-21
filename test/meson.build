test_compiler_flags = compiler_flags

if lib_type == 'static'
    test_compiler_flags += ['-DTVG_STATIC']
endif

test_dep = []
if host_machine.system() == 'darwin'
    test_dep += declare_dependency(link_args: ['-framework', 'Cocoa', '-framework', 'IOKit'])
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
    cpp_args : test_compiler_flags,
    dependencies : test_dep)

test('Unit Tests', tests, args : ['--success'])
