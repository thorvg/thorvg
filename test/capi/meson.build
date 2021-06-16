test_file = [
    'capiMain.cpp',
    'capiFill.cpp',
]

tests = executable('capiUnitTests',
    test_file,
    include_directories : headers,
    link_with : thorvg_lib)

test('Capi Unit Tests', tests)
