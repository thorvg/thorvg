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

if gl_engine
    test_egl_dep = dependency('egl', required: false)

    if gl_variant == 'OpenGL ES'
        test_gl_dep = dependency('glesv2', required: false)
        if not test_gl_dep.found()
            test_gl_dep = cc.find_library('GLESv2', required: false)
        endif
    else
        test_gl_dep = dependency('gl', required: false)
    endif

    if test_egl_dep.found() and test_gl_dep.found()
        test_dep += [test_egl_dep, test_gl_dep]
        test_compiler_flags += ['-DTHORVG_GL_TEST_SUPPORT']
        test_file += ['testGlCanvas.cpp', 'testGlEngine.cpp']
    endif
endif

tests = executable('tvgUnitTests',
    test_file,
    include_directories : headers,
    link_with : thorvg_lib,
    cpp_args : test_compiler_flags,
    dependencies : test_dep)

test('Unit Tests', tests, args : ['--success'])
