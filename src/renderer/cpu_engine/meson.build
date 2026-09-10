source_file = [
   'tvgSwCommon.h',
   'tvgSwRasterC.h',
   'tvgSwRasterAvx.h',
   'tvgSwRasterNeon.h',
   'tvgSwRasterTexmap.h',
   'tvgSwCpuInfo.h',
   'tvgSwBlendOp.cpp',
   'tvgSwCpuInfo.cpp',
   'tvgSwFill.cpp',
   'tvgSwImage.cpp',
   'tvgSwMemPool.cpp',
   'tvgSwPostEffect.cpp',
   'tvgSwRenderer.h',
   'tvgSwRaster.cpp',
   'tvgSwRenderer.cpp',
   'tvgSwRle.cpp',
   'tvgSwShape.cpp',
   'tvgSwStroke.cpp',
   'tvgSwUtil.cpp'
]

omp_dep = []

if openmp
    omp_dep = dependency('openmp')
endif

simd_libs = []

if simd_type == 'avx'
    if cc.get_id() == 'msvc'
        avx_flags = ['/arch:AVX']
    elif cc.get_id() == 'clang-cl'
        avx_flags = ['/clang:-mavx']
    else
        avx_flags = ['-mavx']
    endif

    simd_libs += static_library('tvg_sw_avx', 'tvgSwRasterAvx.cpp',
        include_directories   : [headers, include_directories('.', '..', '../../common')],
        dependencies          : omp_dep,
        cpp_args              : compiler_flags + avx_flags,
        gnu_symbol_visibility : 'hidden',
        override_options      : override_options
    )
endif

engine_dep += [declare_dependency(
    include_directories : include_directories('.'),
    dependencies        : omp_dep,
    link_whole          : simd_libs,
    sources             : source_file
)]
