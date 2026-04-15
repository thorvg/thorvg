test_compiler_flags = compiler_flags

if lib_type == 'static'
    test_compiler_flags += ['-DTVG_STATIC']
endif

test_dep = []
if host_machine.system() == 'darwin'
    test_dep += declare_dependency(link_args: ['-framework', 'Cocoa', '-framework', 'IOKit'])
endif

if gl_engine and not get_option('extra').contains('opengl_es') and host_machine.system() == 'linux'
    egl_dep = dependency('egl', required: false)
    gl_test_dep = dependency('gl', required: false)

    if egl_dep.found() and gl_test_dep.found()
        test_dep += [egl_dep, gl_test_dep]
        test_compiler_flags += ['-DTHORVG_GL_TEST_SUPPORT']
    endif
endif

test_file = [
    'engine.cpp',
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

if gl_engine and test_compiler_flags.contains('-DTHORVG_GL_TEST_SUPPORT')
    test_file += ['testGlCanvas.cpp', 'testGlEngine.cpp']
endif

tests = executable('tvgUnitTests',
    test_file,
    include_directories : headers,
    link_with : thorvg_lib,
    cpp_args : test_compiler_flags,
    dependencies : test_dep)

test('Unit Tests', tests, args : ['--success'])
