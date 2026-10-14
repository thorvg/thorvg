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
    if host_machine.system() == 'darwin'
        test_dep += dependency('appleframeworks', modules: ['OpenGL'])
        test_compiler_flags += ['-DTHORVG_GL_TEST_SUPPORT']
        test_file += ['testGlCanvas.cpp', 'testGlEngine.cpp']
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
            test_file += ['testGlCanvas.cpp', 'testGlEngine.cpp']
        endif
    endif
endif

if wg_engine
    test_wgpu_dep = dependency('wgpu_native', required: false)

    if test_wgpu_dep.found()
        test_dep += test_wgpu_dep
        test_compiler_flags += ['-DTHORVG_WG_TEST_SUPPORT']
        test_file += ['testWgCanvas.cpp', 'testWgEngine.cpp']
    endif
endif

tests = executable('tvgUnitTests',
    test_file,
    include_directories : headers,
    link_with : thorvg_lib,
    cpp_args : test_compiler_flags,
    dependencies : test_dep)

test('Unit Tests', tests)

if gl_engine and cpu_engine and get_option('analytic_aa')
    coverage_includes = headers + [include_directories('../src/common'), include_directories('../src/renderer'),
                                  include_directories('../src/renderer/gpu_engine/gl/coverage')]
    if lib_type == 'static'
        coverage_tests = executable('tvgCoverageTests', 'testGlCoverage.cpp',
            include_directories: coverage_includes,
            link_with: thorvg_lib,
            cpp_args: test_compiler_flags,
            dependencies: test_dep)
    else
        # The coverage adapter is internal; reuse the library objects without exporting it.
        coverage_dependencies = []
        foreach dep : thorvg_lib_dep
            coverage_dependencies += dep.partial_dependency(compile_args: true, link_args: true, links: true)
        endforeach
        coverage_tests = executable('tvgCoverageTests', 'testGlCoverage.cpp',
            include_directories: coverage_includes,
            objects: thorvg_lib.extract_all_objects(recursive: true),
            cpp_args: test_compiler_flags,
            dependencies: coverage_dependencies)
    endif
    test('Coverage Maps', coverage_tests)

    if host_machine.system() == 'windows' and gl_variant == 'OpenGL'
        coverage_storage_includes = coverage_includes + [
            include_directories('../src/renderer/gpu_engine'),
            include_directories('../src/renderer/gpu_engine/gl')]
        coverage_storage_dep = [cc.find_library('opengl32'), cc.find_library('gdi32'), cc.find_library('user32')]
        if lib_type == 'static'
            coverage_storage_tests = executable('tvgCoverageStorageTests', 'testGlCoverageStorage.cpp',
                include_directories: coverage_storage_includes,
                link_with: thorvg_lib,
                cpp_args: test_compiler_flags,
                dependencies: test_dep + coverage_storage_dep)
        else
            coverage_storage_tests = executable('tvgCoverageStorageTests', 'testGlCoverageStorage.cpp',
                include_directories: coverage_storage_includes,
                objects: thorvg_lib.extract_all_objects(recursive: true),
                cpp_args: test_compiler_flags,
                dependencies: coverage_dependencies + coverage_storage_dep)
        endif
        test('Coverage GPU Storage', coverage_storage_tests)
    endif
endif
