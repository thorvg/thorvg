examples_compiler_flags = compiler_flags

if lib_type == 'static'
    examples_compiler_flags += ['-DTVG_STATIC']
endif

if target_opengles
    examples_compiler_flags += '-DTHORVG_GL_TARGET_GLES=1'
endif

examples_dep = [dependency('sdl2')]

if all_engines or get_option('engines').contains('wg')
    examples_dep += dependency('wgpu_native')
    if host_machine.system() == 'darwin'
        add_languages('objcpp')
        examples_dep += declare_dependency(link_args: ['-lobjc', '-framework', 'Cocoa', '-framework', 'CoreVideo', '-framework', 'IOKit', '-framework', 'QuartzCore'])
        examples_compiler_flags += ['-x', 'objective-c++']
    endif
endif

source_file = [
    'Accessor.cpp',
    'AnimateMasking.cpp',
    'Animation.cpp',
    'Blending.cpp',
    'BoundingBox.cpp',
    'Clipping.cpp',
    'CustomTransform.cpp',
    'DataLoad.cpp',
    'DirectUpdate.cpp',
    'Duplicate.cpp',
    'EffectDropShadow.cpp',
    'SceneEffects.cpp',
    'FillRule.cpp',
    'FillSpread.cpp',
    'GifSaver.cpp',
    'GradientMasking.cpp',
    'GradientStroke.cpp',
    'GradientTransform.cpp',
    'ImageRotation.cpp',
    'ImageScaleDown.cpp',
    'ImageScaleUp.cpp',
    'LinearGradient.cpp',
    'Lottie.cpp',
    'LumaMasking.cpp',
    'Masking.cpp',
    'MaskingMethods.cpp',
    'MultiCanvas.cpp',
    'Opacity.cpp',
    'Path.cpp',
    'PictureJpg.cpp',
    'PicturePng.cpp',
    'PictureRaw.cpp',
    'PictureSvg.cpp',
    'PictureWebp.cpp',
    'RadialGradient.cpp',
    'Retaining.cpp',
    'Scene.cpp',
    'SceneBlending.cpp',
    'SceneTransform.cpp',
    'Shapes.cpp',
    'Stroke.cpp',
    'StrokeLine.cpp',
    'StrokeMiterlimit.cpp',
    'Svg.cpp',
    'Text.cpp',
    'Transform.cpp',
    'TrimPath.cpp',
    'Update.cpp',
    'Viewport.cpp'
]


if lottie_loader
  source_file += ['LottieExtension.cpp', 'LottieTweening.cpp']
endif

if lottie_expressions
  source_file += 'LottieExpressions.cpp'
endif

foreach current_file : source_file
    name = current_file.split('.')[0]
    executable(name, current_file,
        include_directories : headers,
        link_with : thorvg_lib,
        cpp_args : examples_compiler_flags,
        dependencies : examples_dep)
endforeach


if get_option('bindings').contains('capi')
    capi_source_file = [
        'Capi.cpp'
    ]

    foreach current_file : capi_source_file
        name = current_file.split('.')[0]
        executable(name, current_file,
            include_directories : headers,
            link_with : thorvg_lib,
            cpp_args : examples_compiler_flags,
            dependencies : examples_dep)
    endforeach
endif

execute_all_src = join_paths(meson.project_source_root(), 'examples/all.sh')
execute_all_dst = join_paths(meson.project_build_root(), 'examples/all.sh')
run_command('cp', execute_all_src, execute_all_dst, check: true)
