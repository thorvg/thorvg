source_file = [
   'tvgGlCommon.h',
   'tvgGlGeometry.h',
   'tvgGlGpuBuffer.h',
   'tvgGlList.h',
   'tvgGlProgram.h',
   'tvgGlRenderer.h',
   'tvgGlRenderPass.h',
   'tvgGlRenderTask.h',
   'tvgGlShader.h',
   'tvgGlShaderSrc.h',
   'tvgGlGeometry.cpp',
   'tvgGlGpuBuffer.cpp',
   'tvgGlProgram.cpp',
   'tvgGlRenderer.cpp',
   'tvgGlRenderPass.cpp',
   'tvgGlRenderTask.cpp',
   'tvgGlShader.cpp',
   'tvgGlShaderSrc.cpp',
   'tvgGlTessellator.cpp',
   'tvgGlTessellator.h',
]

if host_machine.system() == 'darwin'
    gl_dep = declare_dependency(link_args: ['-framework', 'OpenGL'])
elif host_machine.system() == 'windows'
    gl_dep = cc.find_library(
        'GLESv2',
        has_headers: 'GLES3/gl3.h'
    )
    if gl_dep.found()
        target_opengles = true
    else
        error('GLES 3.0 library not found. Please make sure Angle library is installed, or disable OpenGL support.')
    endif
else
    #find a opengl library with fallbacks
    gl_dep = dependency('GL', required: false)

    if not gl_dep.found()
        gl_dep = meson.get_compiler('cpp').find_library('GL', required: false)
    endif

    if not gl_dep.found()
        gl_dep = dependency('GLESv3', required: false)
        target_opengles = true
    endif

    if not gl_dep.found()
        gl_dep = meson.get_compiler('cpp').find_library('GLESv3')
        target_opengles = true
    endif
endif

if target_opengles
    gl_target_profile = '-DTHORVG_GL_TARGET_GLES=1'
else
    gl_target_profile = '-DTHORVG_GL_TARGET_GL=1'
endif

engine_dep += [declare_dependency(
    compile_args        : gl_target_profile,
    dependencies        : gl_dep,
    include_directories : include_directories('.'),
    sources             : source_file,
)]
