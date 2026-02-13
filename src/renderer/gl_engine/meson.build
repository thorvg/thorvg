source_file = [
   'tvgGl.h',
   'tvgGlCommon.h',
   'tvgGlEffect.h',
   'tvgGlGpuBuffer.h',
   'tvgGlProgram.h',
   'tvgGlRenderer.h',
   'tvgGlRenderPass.h',
   'tvgGlRenderTarget.h',
   'tvgGlRenderTask.h',
   'tvgGlShader.h',
   'tvgGlShaderSrc.h',
   'tvgGl.cpp',
   'tvgGlEffect.cpp',
   'tvgGlGeometry.cpp',
   'tvgGlGpuBuffer.cpp',
   'tvgGlProgram.cpp',
   'tvgGlRenderer.cpp',
   'tvgGlRenderPass.cpp',
   'tvgGlRenderTarget.cpp',
   'tvgGlRenderTask.cpp',
   'tvgGlShader.cpp',
   'tvgGlShaderSrc.cpp',
   'tvgGlTessellator.cpp',
   'tvgGlTessellator.h',
]

#force to use gles
if cc.get_id() == 'emscripten'
    gl_variant = 'OpenGL ES'
endif

if gl_variant == 'OpenGL ES'
    compiler_flags += ['-DTHORVG_GL_TARGET_GLES=1']
else
    compiler_flags += ['-DTHORVG_GL_TARGET_GL=1']
endif

if (get_option('threads'))
    omp_dep = dependency('openmp', required: false)
    if (omp_dep.found())
        gl_variant_profile += '-DTHORVG_GL_OPENMP_SUPPORT=1'
    endif
endif

engine_dep += [declare_dependency(
    include_directories : include_directories('.'),
    dependencies        : omp_dep,
    sources             : source_file,
)]
