compiler_flags += ['-DTHORVG_WINDOWS_MEDIA_SUPPORT=1']

source_file = [
    'tvgMfMediaLoader.h',
    'tvgMfMediaLoader.cpp'
]

media_dep += [declare_dependency(
    dependencies : [
        cc.find_library('mfplat'),
        cc.find_library('mfuuid'),
        cc.find_library('d3d11'),
        cc.find_library('ole32'),
        cc.find_library('oleaut32')
    ],
    include_directories : include_directories('.'),
    sources : source_file
)]
