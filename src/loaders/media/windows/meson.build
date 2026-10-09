source_file += files(
    'tvgMfMediaLoader.h',
    'tvgMfMediaLoader.cpp'
)

media_dep += [declare_dependency(
    # Target Windows 8 to expose the APIs required by the media backend.
    # https://learn.microsoft.com/en-us/windows/win32/winprog/using-the-windows-headers
    compile_args : ['-D_WIN32_WINNT=0x0602'],
    dependencies : [
        cc.find_library('mfplat'),
        cc.find_library('mfuuid'),
        cc.find_library('d3d11'),
        cc.find_library('ole32'),
        cc.find_library('oleaut32'),
        cc.find_library('shlwapi')
    ]
)]
