source_file = [
    'tvgWgBindGroups.h',
    'tvgWgCommon.h',
    'tvgWgCompositor.h',
    'tvgWgGeometry.h',
    'tvgWgPipelines.h',
    'tvgWgRenderData.h',
    'tvgWgRenderer.h',
    'tvgWgRenderTarget.h',
    'tvgWgShaderSrc.h',
    'tvgWgShaderTypes.h',
    'tvgWgBindGroups.cpp',
    'tvgWgCommon.cpp',
    'tvgWgCompositor.cpp',
    'tvgWgGeometry.cpp',
    'tvgWgPipelines.cpp',
    'tvgWgRenderData.cpp',
    'tvgWgRenderer.cpp',
    'tvgWgRenderTarget.cpp',
    'tvgWgShaderSrc.cpp',
    'tvgWgShaderTypes.cpp'
]

# dependencies for webgpu for windows are described in the corresponding cross file
wgpu_dep = dependency('wgpu_native', required : host_machine.system() not in ['emscripten', 'windows'])

engine_dep += [declare_dependency(
    dependencies        : wgpu_dep,
    include_directories : include_directories('.'),
    sources : source_file
)]
