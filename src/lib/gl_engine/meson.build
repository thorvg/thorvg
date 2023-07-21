source_file = [
   'tvgGlCommand.cpp',
   'tvgGlCommand.h',
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
    defines += [ '-DTVG_TARGET_GLES' ]
elif host_machine.system() == 'linux'
    external_dep += [ meson.get_compiler('cpp').find_library('GL', required: true) ]
    defines += [ '-DTVG_TARGET_GL' ]
else
    external_dep += [ dependency('opengl', required:true) ]
    defines += [ '-DTVG_TARGET_GL' ]
endif

# shaders
subdir('shader')

engine_dep += [declare_dependency(
    dependencies        : [external_dep, gl_shader_dep],
    compile_args        : defines,
    include_directories : include_directories('.'),
    sources             : source_file,
)]
