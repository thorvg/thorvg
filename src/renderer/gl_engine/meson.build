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
    gl_variant_profile = '-DTHORVG_GL_TARGET_GLES=1'
else
    gl_variant_profile = '-DTHORVG_GL_TARGET_GL=1'
endif

engine_dep += [declare_dependency(
    compile_args        : gl_variant_profile,
    include_directories : include_directories('.'),
    sources             : source_file,
)]
