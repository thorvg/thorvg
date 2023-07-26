if (lib_type == 'static')
    compiler_flags += ['-DTVG_STATIC']
endif

examples_dep = [ dependency('sdl2', required : true) ]

if (get_option('engines').contains('gl_beta') == true)
    examples_dep += [ dependency('opengl', required : true) ]
    compiler_flags += [ '-DTVG_EXAMPLE_GL' ]
endif


source_file = [
    'Accessor.cpp',
    'AnimateMasking.cpp',
    'Animation.cpp',
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
    'InvLumaMasking.cpp',
    'InvMasking.cpp',
    'LinearGradient.cpp',
    'Lottie.cpp',
    'LumaMasking.cpp',
    'Masking.cpp',
    'MaskingMethods.cpp',
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
    'Retaining.cpp',
    'Scene.cpp',
    'SceneBlending.cpp',
    'SceneClipper.cpp',
    'SceneTransform.cpp',
    'Shape.cpp',
    'Stress.cpp',
    'Stroke.cpp',
    'StrokeLine.cpp',
    'StrokeMiterlimit.cpp',
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
        cpp_args : [compiler_flags, '-std=c++17' ],
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
run_command('cp', execute_all_src, execute_all_dst, check: true)
