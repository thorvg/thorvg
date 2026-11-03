test_compiler_flags = compiler_flags + ['-UTEST_DIR', '-DTEST_DIR="' + join_paths(meson.current_source_dir(), 'res') + '"']

if lib_type == 'static'
    test_compiler_flags += ['-DTVG_STATIC']
endif

test_dep = []
if host_machine.system() == 'darwin'
    test_dep += declare_dependency(link_args: ['-framework', 'Cocoa', '-framework', 'IOKit'])
endif

test_file = [
    'src/testAccessor.cpp',
    'src/testAnimation.cpp',
    'src/testFill.cpp',
    'src/testInitializer.cpp',
    'src/testLottie.cpp',
    'src/testMain.cpp',
    'src/testPaint.cpp',
    'src/testPicture.cpp',
    'src/testSavers.cpp',
    'src/testScene.cpp',
    'src/testShape.cpp',
    'src/testSwCanvas.cpp',
    'src/testSwEngine.cpp',
    'src/testText.cpp'
]

if gl_engine
    if host_machine.system() == 'darwin'
        test_dep += dependency('appleframeworks', modules: ['OpenGL'])
        test_compiler_flags += ['-DTHORVG_GL_TEST_SUPPORT']
        test_file += ['src/testGlCanvas.cpp', 'src/testGlEngine.cpp']
    else
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
            test_file += ['src/testGlCanvas.cpp', 'src/testGlEngine.cpp']
        endif
    endif
endif

if wg_engine
    test_wgpu_dep = dependency('wgpu_native', required: false)

    if test_wgpu_dep.found()
        test_dep += test_wgpu_dep
        test_compiler_flags += ['-DTHORVG_WG_TEST_SUPPORT']
        test_file += ['src/testWgCanvas.cpp', 'src/testWgEngine.cpp']
    endif
endif

tests = executable('tvgUnitTests',
    test_file,
    include_directories : headers,
    link_with : thorvg_lib,
    cpp_args : test_compiler_flags,
    dependencies : test_dep)

test('Unit Tests', tests)
