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

gles_dep = meson.get_compiler('cpp').find_library('GLESv2')

external_dep = [gles_dep]

engine_dep += [declare_dependency(
    dependencies        : external_dep,
    include_directories : include_directories('.'),
    sources             : source_file,
)]
