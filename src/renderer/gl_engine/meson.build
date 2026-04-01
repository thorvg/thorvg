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
   'tvgGlSolidBatch.h',
   'tvgGl.cpp',
   'tvgGlEffect.cpp',
   'tvgGlGeometry.cpp',
   'tvgGlGpuBuffer.cpp',
   'tvgGlProgram.cpp',
   'tvgGlRenderer.cpp',
   'tvgGlSolidBatch.cpp',
   'tvgGlRenderPass.cpp',
   'tvgGlRenderTarget.cpp',
   'tvgGlRenderTask.cpp',
   'tvgGlShader.cpp',
   'tvgGlShaderSrc.cpp',
   'tvgGlTessellator.cpp',
   'tvgGlTessellator.h',
]

#force to use gles
if cc.get_id() == 'emscripten' or target_machine.system() == 'ios' or target_machine.system() == 'android' or target_machine.system() == 'ohos'
    gl_variant = 'OpenGL ES'
    message('Force to use OpenGL ES variant, cc.get_id() = ' + cc.get_id() + ', target_machine.system() = ' + target_machine.system())
endif

if gl_variant == 'OpenGL ES'
    compiler_flags += ['-DTHORVG_GL_TARGET_GLES=1']
else
    compiler_flags += ['-DTHORVG_GL_TARGET_GL=1']
endif

message('target_machine.system() = ' + target_machine.system())
if target_machine.system() == 'ios'
   message('Detected iOS platform, linking OpenGLES.framework')
   gles_dep = dependency(
      'OpenGLES',
      required: true
   )
   engine_dep += gles_dep
elif target_machine.system() == 'darwin'
   message('Detected macOS platform, linking OpenGL.framework')
   gles_dep = dependency(
      'OpenGL',
      required: true
   )
   engine_dep += gles_dep
endif

engine_dep += [declare_dependency(
    include_directories : include_directories('.'),
    sources             : source_file,
)]
