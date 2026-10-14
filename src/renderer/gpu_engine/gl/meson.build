source_file = [
   'tvgGl.h',
   'tvgGlCommon.h',
   'tvgGlEffect.h',
   'tvgGlGpuBuffer.h',
   'tvgGlTextureMgr.h',
   'tvgGlProgram.h',
   'tvgGlRenderer.h',
   'tvgGlRenderPass.h',
   'tvgGlRenderTarget.h',
   'tvgGlRenderTask.h',
   'tvgGlShader.h',
   'tvgGlShaderSrc.h',
   'tvgGlSolidBatch.h',
   'tvgGlStateCache.h',
   'tvgGl.cpp',
   'tvgGlEffect.cpp',
   'tvgGlGeometry.cpp',
   'tvgGlGpuBuffer.cpp',
   'tvgGlTextureMgr.cpp',
   'tvgGlProgram.cpp',
   'tvgGlRenderer.cpp',
   'tvgGlSolidBatch.cpp',
   'tvgGlRenderPass.cpp',
   'tvgGlRenderTarget.cpp',
   'tvgGlRenderTask.cpp',
   'tvgGlShader.cpp',
   'tvgGlShaderSrc.cpp',
   'tvgGlStateCache.cpp',
]

if get_option('analytic_aa')
    source_file += ['tvgGlCoverage.cpp', 'tvgGlCoverage.h',
                    'coverage/tvgCoverage.h', 'coverage/tvgCoverage.cpp']
    # CPU builds already provide these unchanged geometry entry points.
    if not cpu_engine
        source_file += ['../../cpu_engine/tvgSwShape.cpp', '../../cpu_engine/tvgSwStroke.cpp',
                        '../../cpu_engine/tvgSwRle.cpp', '../../cpu_engine/tvgSwUtil.cpp']
    endif
else
    source_file += ['tvgGlTessellator.cpp', 'tvgGlTessellator.h',
                    'tvgGlStencilCoverBatch.cpp', 'tvgGlStencilCoverBatch.h']
endif

#force to use gles
if cc.get_id() == 'emscripten'
    gl_variant = 'OpenGL ES'
endif

if gl_variant == 'OpenGL ES'
    compiler_flags += ['-DTHORVG_GL_TARGET_GLES=1']
else
    compiler_flags += ['-DTHORVG_GL_TARGET_GL=1']
endif

engine_dep += [declare_dependency(
    include_directories : include_directories('.', 'coverage', '../../cpu_engine'),
    sources             : source_file,
)]
