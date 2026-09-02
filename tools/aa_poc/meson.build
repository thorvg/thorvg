aa_poc_dep = []
aa_poc_args = compiler_flags

if host_machine.system() == 'darwin'
    aa_poc_dep += dependency('appleframeworks', modules: ['OpenGL'])
else
    aa_poc_dep += dependency('egl')
    if gl_variant == 'OpenGL ES'
        aa_poc_args += ['-DAA_POC_GLES=1']
        aa_poc_gl_dep = dependency('glesv2', required: false)
        if not aa_poc_gl_dep.found()
            aa_poc_gl_dep = cc.find_library('GLESv2')
        endif
        aa_poc_dep += aa_poc_gl_dep
    else
        aa_poc_dep += dependency('gl')
    endif
endif

aa_poc_product_scene_sources = files(
    'aa_product_scenes.cpp',
)

aa_poc_common_sources = files(
    'aa_poc_cli.cpp',
    'aa_poc_comparison_scene.cpp',
    'aa_poc_gl.cpp',
    '../svg2png/lodepng.cpp',
) + aa_poc_product_scene_sources

aa_poc_common_includes = [include_directories('../svg2png')]

aa_poc_render_includes = aa_poc_common_includes + [
    include_directories('../..'),
    include_directories('../../inc'),
    include_directories('../../src/common'),
    include_directories('../../src/renderer'),
    include_directories('../../src/renderer/gpu_engine'),
    include_directories('../../src/renderer/gpu_engine/gl'),
]

aa_poc_flat_mask_scene_sources = files('aa_poc_flat_mask_scene.cpp')
aa_poc_curve_mask_scene_sources = files('aa_poc_curve_mask_scene.cpp')
aa_poc_product_renderer_sources = files('aa_product_renderer.cpp')

executable('aa_flat_direct_poc',
    files(
        'aa_flat_direct_poc.cpp',
        '../../src/common/tvgMath.cpp',
    ) + aa_poc_common_sources,
    include_directories: aa_poc_render_includes,
    cpp_args: aa_poc_args,
    dependencies: aa_poc_dep,
    link_with: thorvg_lib,
    install: false,
)

executable('aa_curve_direct_poc',
    files(
        'aa_curve_direct_poc.cpp',
        '../../src/common/tvgMath.cpp',
    ) + aa_poc_common_sources,
    include_directories: aa_poc_render_includes,
    cpp_args: aa_poc_args,
    dependencies: aa_poc_dep,
    link_with: thorvg_lib,
    install: false,
)

aa_reference_sources = files(
    'aa_reference_poc.cpp',
    '../../src/common/tvgMath.cpp',
) + aa_poc_common_sources + aa_poc_flat_mask_scene_sources + aa_poc_curve_mask_scene_sources

executable('aa_msaa4_poc',
    files('aa_msaa4_poc.cpp') + aa_reference_sources,
    include_directories: aa_poc_render_includes,
    cpp_args: aa_poc_args,
    dependencies: aa_poc_dep,
    link_with: thorvg_lib,
    install: false,
)

executable('aa_ssaa8_poc',
    files('aa_ssaa8_poc.cpp') + aa_reference_sources,
    include_directories: aa_poc_render_includes,
    cpp_args: aa_poc_args,
    dependencies: aa_poc_dep,
    link_with: thorvg_lib,
    install: false,
)

executable('aa_flat_mask_poc',
    files('aa_flat_mask_poc.cpp') + aa_poc_common_sources + aa_poc_flat_mask_scene_sources + aa_poc_product_renderer_sources,
    include_directories: aa_poc_render_includes,
    cpp_args: aa_poc_args,
    dependencies: aa_poc_dep,
    link_with: thorvg_lib,
    install: false,
)

executable('aa_curve_mask_poc',
    files('aa_curve_mask_poc.cpp') + aa_poc_common_sources + aa_poc_curve_mask_scene_sources + aa_poc_product_renderer_sources,
    include_directories: aa_poc_render_includes,
    cpp_args: aa_poc_args,
    dependencies: aa_poc_dep,
    link_with: thorvg_lib,
    install: false,
)

executable('aa_product_poc',
    files('aa_product_poc.cpp') + aa_poc_product_renderer_sources + aa_poc_common_sources,
    include_directories: aa_poc_render_includes,
    cpp_args: aa_poc_args,
    dependencies: aa_poc_dep,
    link_with: thorvg_lib,
    install: false,
)

executable('aa_size_diagnostic',
    files('aa_size_diagnostic.cpp') + aa_poc_product_renderer_sources + aa_poc_common_sources,
    include_directories: aa_poc_render_includes,
    cpp_args: aa_poc_args,
    dependencies: aa_poc_dep,
    link_with: thorvg_lib,
    install: false,
)

executable('aa_aabb_diagnostic',
    files('aa_aabb_diagnostic.cpp') + aa_poc_product_renderer_sources + aa_poc_common_sources,
    include_directories: aa_poc_render_includes,
    cpp_args: aa_poc_args,
    dependencies: aa_poc_dep,
    link_with: thorvg_lib,
    install: false,
)

if wg_engine and host_machine.system() != 'emscripten'
    aa_poc_wgpu_dep = [dependency('wgpu_native')]
    if host_machine.system() == 'darwin'
        aa_poc_wgpu_dep += dependency('appleframeworks', modules: ['Cocoa', 'IOKit'])
    endif

    executable('thorvg_webgpu_quality',
        files(
            'thorvg_webgpu_quality.cpp',
            'aa_poc_comparison_scene.cpp',
            '../svg2png/lodepng.cpp',
        ) + aa_poc_product_scene_sources,
        include_directories: aa_poc_render_includes,
        cpp_args: aa_poc_args,
        dependencies: aa_poc_wgpu_dep,
        link_with: thorvg_lib,
        install: false,
    )
endif
