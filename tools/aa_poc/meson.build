aa_poc_dep = []
aa_poc_args = compiler_flags

if host_machine.system() == 'darwin'
    aa_poc_dep += dependency('appleframeworks', modules: ['OpenGL'])
else
    aa_poc_dep += dependency('egl')
    if gl_variant == 'OpenGL ES'
        aa_poc_args += ['-DAA_POC_GLES=1']
        aa_poc_gl_dep = dependency('glesv2', required: false)
        if not aa_poc_gl_dep.found()
            aa_poc_gl_dep = cc.find_library('GLESv2')
        endif
        aa_poc_dep += aa_poc_gl_dep
    else
        aa_poc_dep += dependency('gl')
    endif
endif

executable('aa_poc',
    files(
        'aa_poc.cpp',
        '../svg2png/lodepng.cpp',
        '../../src/common/tvgMath.cpp',
    ),
    include_directories: [
        include_directories('../..'),
        include_directories('../../inc'),
        include_directories('../../src/common'),
        include_directories('../../src/renderer'),
        include_directories('../svg2png'),
    ],
    cpp_args: aa_poc_args,
    dependencies: aa_poc_dep,
    install: false,
)
