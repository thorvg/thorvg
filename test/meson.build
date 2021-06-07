test_file = [
    'testMain.cpp',
    'testInitializer.cpp',
]

if get_option('bindings').contains('capi') == true
	capi_test_file = [
	    'capi/testInitializer.cpp',
	    'capi/testSwCanvas.cpp',
	    'capi/testPaint.cpp',
	    'capi/testShape.cpp',
	]
	test_file += capi_test_file
endif

tests = executable('tvgUnitTests',
    test_file,
    include_directories : headers,
    link_with : thorvg_lib)

test('Unit Tests', tests)