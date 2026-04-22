test_compiler_flags = compiler_flags

if lib_type == 'static'
    test_compiler_flags += ['-DTVG_STATIC']
endif

test_dep = []
if host_machine.system() == 'darwin'
    test_dep += declare_dependency(link_args: ['-framework', 'Cocoa', '-framework', 'IOKit'])
endif

sdl2_dep = disabler()
if gl_engine and not get_option('extra').contains('opengl_es') and ['linux', 'darwin'].contains(host_machine.system())
    sdl2_dep = dependency('sdl2', required: false)
endif

if sdl2_dep.found()
    gl_test_dep = disabler()

    if host_machine.system() == 'darwin'
        gl_test_dep = declare_dependency(link_args: ['-framework', 'OpenGL'])
    else
        gl_test_dep = dependency('gl', required: false)
    endif

    if gl_test_dep.found()
        test_dep += gl_test_dep
        test_compiler_flags += ['-DTHORVG_GL_TEST_SUPPORT']
    endif
endif

if wg_engine
    wgpu_test_dep = dependency('wgpu_native', required: false)
    if wgpu_test_dep.found()
        test_dep += wgpu_test_dep
        test_compiler_flags += ['-DTHORVG_WG_TEST_SUPPORT']
    endif
endif

if sdl2_dep.found() and test_compiler_flags.contains('-DTHORVG_GL_TEST_SUPPORT')
    test_dep += sdl2_dep
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

if gl_engine and test_compiler_flags.contains('-DTHORVG_GL_TEST_SUPPORT')
    test_file += ['testGlWindow.cpp', 'testGlCanvas.cpp', 'testGlEngine.cpp']
endif

if wg_engine and test_compiler_flags.contains('-DTHORVG_WG_TEST_SUPPORT')
    test_file += ['testWgWindow.cpp', 'testWgCanvas.cpp', 'testWgEngine.cpp']
endif

tests = executable('tvgUnitTests',
    test_file,
    include_directories : headers,
    link_with : thorvg_lib,
    cpp_args : test_compiler_flags,
    dependencies : test_dep)

test('Unit Tests', tests, args : ['--success'])
