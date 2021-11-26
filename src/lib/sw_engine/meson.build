source_file = [
   'tvgSwCommon.h',
   'tvgSwRasterC.h',
   'tvgSwRasterAvx.h',
   'tvgSwRasterNeon.h',
   'tvgSwRasterTexmap.h',
   'tvgSwRasterTexmapInternal.h',
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

engine_dep += [declare_dependency(
    include_directories : include_directories('.'),
    sources : source_file
)]
