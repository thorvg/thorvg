source_file = [
   'tvgSwCommon.h',
   'tvgSwRasterC.h',
   'tvgSwRasterNeon.h',
   'tvgSwFill.cpp',
   'tvgSwImage.cpp',
   'tvgSwMath.cpp',
   'tvgSwRenderer.h',
   'tvgSwRaster.cpp',
   'tvgSwRenderer.cpp',
   'tvgSwMemPool.cpp',
   'tvgSwRle.cpp',
   'tvgSwShape.cpp',
   'tvgSwStroke.cpp',
]

avx_source_file = [
    'tvgSwRasterAvx.cpp',
]

avx_compiler_flags = compiler_flags + ['-mavx']
avx_headers +=  [include_directories('.')]

avx_raster_lib = library(
    'tvgSwRasterAvx',
    avx_source_file,
    include_directories  : avx_headers,
    cpp_args             : avx_compiler_flags,
    install              : true
)


engine_dep += [declare_dependency(
    link_with            : avx_raster_lib,
    include_directories  : include_directories('.'),
    sources              : source_file
)]
