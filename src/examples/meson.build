compiler_flags = ['-DTVG_BUILD']

examples_dep = dependency('elementary', required : true)

source_file = [
    'Accessor.cpp',
    'AnimateMasking.cpp',
    'Arc.cpp',
    'Async.cpp',
    'Blending.cpp',
    'ClipPath.cpp',
    'CustomTransform.cpp',
    'DirectUpdate.cpp',
    'Duplicate.cpp',
    'FillRule.cpp',
    'GradientMasking.cpp',
    'GradientStroke.cpp',
    'GradientTransform.cpp',
    'ImageScaleDown.cpp',
    'ImageScaleUp.cpp',
    'InvMasking.cpp',
    'LinearGradient.cpp',
    'LumaMasking.cpp',
    'Masking.cpp',
    'MultiCanvas.cpp',
    'MultiShapes.cpp',
    'Opacity.cpp',
    'PathCopy.cpp',
    'Path.cpp',
    'Performance.cpp',
    'PictureJpg.cpp',
    'PicturePng.cpp',
    'PictureRaw.cpp',
    'PictureTvg.cpp',
    'PictureWebp.cpp',
    'RadialGradient.cpp',
    'Scene.cpp',
    'SceneClipper.cpp',
    'SceneTransform.cpp',
    'Shape.cpp',
    'Stacking.cpp',
    'Stress.cpp',
    'Stroke.cpp',
    'StrokeLine.cpp',
    'Svg.cpp',
    'Svg2.cpp',
    'Texmap.cpp',
    'Transform.cpp',
    'Tvg.cpp',
    'TvgSaver.cpp',
    'Update.cpp',
]

foreach current_file : source_file
    name = current_file.split('.')[0]
    executable(name, current_file,
        include_directories : headers,
        link_with : thorvg_lib,
        cpp_args : compiler_flags,
        dependencies : examples_dep)
endforeach


if get_option('bindings').contains('capi') == true
    capi_source_file = [
        'Capi.cpp'
    ]

    foreach current_file : capi_source_file
        name = current_file.split('.')[0]
        executable(name, current_file,
            include_directories : headers,
            link_with : thorvg_lib,
            cpp_args : compiler_flags,
            dependencies : examples_dep)
    endforeach
endif

execute_all_src = join_paths(meson.source_root(), 'src/examples/all.sh')
execute_all_dst = join_paths(meson.build_root(), 'src/examples/all.sh')
run_command('cp', execute_all_src, execute_all_dst)
