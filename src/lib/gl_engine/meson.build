source_file = [
   'tvgGlCommon.h',
   'tvgGlGeometry.h',
   'tvgGlGpuBuffer.h',
   'tvgGlProgram.h',
   'tvgGlPropertyInterface.h',
   'tvgGlRenderer.h',
   'tvgGlRendererProperties.h',
   'tvgGlRenderTask.h',
   'tvgGlShader.h',
   'tvgGlShaderSrc.h',
   'tvgGlGeometry.cpp',
   'tvgGlGpuBuffer.cpp',
   'tvgGlProgram.cpp',
   'tvgGlPropertyInterface.cpp',
   'tvgGlRenderer.cpp',
   'tvgGlRenderTask.cpp',
   'tvgGlShader.cpp',
   'tvgGlShaderSrc.cpp',
]

external_dep = []
defines = []

if host_machine.system() == 'android'
    external_dep += [ meson.get_compiler('cpp').find_library('GLESv2') ]
    defines += [ '-DOS_ANDROID' ]
else
    external_dep += [ dependency('opengl', required:true) ]
endif

engine_dep += [declare_dependency(
    dependencies        : external_dep,
    include_directories : include_directories('.'),
    sources             : source_file,
)]
