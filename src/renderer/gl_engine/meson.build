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

#find a gl library with fallbacks
gles_dep = meson.get_compiler('cpp').find_library('GLESv3', required: false)

if not gles_dep.found()
  gles_dep = dependency('GLESv3', required: false)
endif

if not gles_dep.found()
  gles_dep = meson.get_compiler('cpp').find_library('GLESv2', required: false)
endif

if not gles_dep.found()
  gles_dep = dependency('GLESv2', required: false)
endif

link_opengl = false

if not gles_dep.found()
  gles_dep = dependency('GL')
  link_opengl = true
endif

if not gles_dep.found()
  gles_dep = meson.get_compiler('cpp').find_library('GL')
  link_opengl = true
endif

if link_opengl
    gl_target_define = '-DTHORVG_GL_TARGET_GL=1'
else
    gl_target_define = '-DTHORVG_GL_TARGET_GLES=1'
endif

engine_dep += [declare_dependency(
    compile_args        : gl_target_define,
    dependencies        : gles_dep,
    include_directories : include_directories('.'),
    sources             : source_file,
)]
