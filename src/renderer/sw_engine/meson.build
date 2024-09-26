source_file = [
   'tvgSwCommon.h',
   'tvgSwRasterC.h',
   'tvgSwRasterAvx.h',
   'tvgSwRasterNeon.h',
   'tvgSwRasterTexmap.h',
   'tvgSwFill.cpp',
   'tvgSwImage.cpp',
   'tvgSwMath.cpp',
   'tvgSwMemPool.cpp',
   'tvgSwPostEffect.cpp',
   'tvgSwRenderer.h',
   'tvgSwRaster.cpp',
   'tvgSwRenderer.cpp',
   'tvgSwRle.cpp',
   'tvgSwShape.cpp',
   'tvgSwStroke.cpp',
]

omp_dep = []
sw_compiler_args = []

if (get_option('threads'))
    omp_dep = dependency('openmp', required: false)
    if (omp_dep.found())
        sw_compiler_args = '-DTHORVG_SW_OPENMP_SUPPORT=1'
    endif
endif

engine_dep += [declare_dependency(
    compile_args        : sw_compiler_args,
    include_directories : include_directories('.'),
    dependencies        : omp_dep,
    sources             : source_file
)]