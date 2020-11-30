
#TODO:Need to change warning level to 3
override_default = ['warning_level=0', 'werror=false']

gtest_dep  = dependency('gtest')

canvas_test_sources = [
    'testsuite.cpp',
    'test_canvas.cpp',
    ]

canvas_testsuite = executable('canvasTestSuite',
                              canvas_test_sources,
                              include_directories : headers,
                              override_options : override_default,
                              dependencies : [gtest_dep, thorvg_lib_dep],
                              )

test('Canvas Testsuite', canvas_testsuite)

paint_test_sources = [
    'testsuite.cpp',
    'test_paint.cpp',
    ]

paint_testsuite = executable('paintTestSuite',
                              paint_test_sources,
                              include_directories : headers,
                              override_options : override_default,
                              dependencies : [gtest_dep, thorvg_lib_dep],
                              )

test('Paint Testsuite', paint_testsuite)
