source_file = [
   'tvgGlCommon.h',
   'tvgGlGeometry.h',
   'tvgGlGpuBuffer.h',
   'tvgGlProgram.h',
   'tvgGlRenderer.h',
   'tvgGlShader.h',
   'tvgGlShaderSrc.h',
   'tvgGlGeometry.cpp',
   'tvgGlGpuBuffer.cpp',
   'tvgGlProgram.cpp',
   'tvgGlRenderer.cpp',
   'tvgGlShader.cpp',
   'tvgGlShaderSrc.cpp',
]

egl_dep = meson.get_compiler('cpp').find_library('EGL')
gles_dep = meson.get_compiler('cpp').find_library('GLESv2')
external_dep = [egl_dep, gles_dep]

glengine_dep = declare_dependency(
   dependencies        : external_dep,
   include_directories : include_directories('.'),
   sources             : source_file,
)
