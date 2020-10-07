examples_dep = dependency('elementary', required : true)

source_file = [
    'testArc.cpp',
    'testAsync.cpp',
    'testBlending.cpp',
    'testBoundary.cpp',
    'testCapi.cpp',
    'testCustomTransform.cpp',
    'testDirectUpdate.cpp',
    'testDuplicate.cpp',
    'testGradientTransform.cpp',
    'testLinearGradient.cpp',
    'testMultiCanvas.cpp',
    'testMultiShapes.cpp',
    'testPathCopy.cpp',
    'testPath.cpp',
    'testRadialGradient.cpp',
    'testScene.cpp',
    'testSceneTransform.cpp',
    'testShape.cpp',
    'testStress.cpp',
    'testStroke.cpp',
    'testStrokeLine.cpp',
    'testSvg2.cpp',
    'testSvg.cpp',
    'testTransform.cpp',
    'testUpdate.cpp',
    'testClipPath.cpp',
]

foreach current_file : source_file
    name = current_file.split('.')[0]
    executable(name, current_file,
           include_directories : headers,
           link_with : thorvg_lib,
           dependencies : examples_dep)
endforeach
