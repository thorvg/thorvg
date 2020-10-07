examples_dep = dependency('elementary', required : true)

source_file = [
    'Arc.cpp',
    'Async.cpp',
    'Blending.cpp',
    'Boundary.cpp',
    'Capi.cpp',
    'CustomTransform.cpp',
    'DirectUpdate.cpp',
    'Duplicate.cpp',
    'GradientTransform.cpp',
    'LinearGradient.cpp',
    'MultiCanvas.cpp',
    'MultiShapes.cpp',
    'PathCopy.cpp',
    'Path.cpp',
    'RadialGradient.cpp',
    'Scene.cpp',
    'SceneTransform.cpp',
    'Shape.cpp',
    'Stress.cpp',
    'Stroke.cpp',
    'StrokeLine.cpp',
    'Svg2.cpp',
    'Svg.cpp',
    'Transform.cpp',
    'Update.cpp',
    'ClipPath.cpp',
]

foreach current_file : source_file
    name = current_file.split('.')[0]
    executable(name, current_file,
           include_directories : headers,
           link_with : thorvg_lib,
           dependencies : examples_dep)
endforeach
