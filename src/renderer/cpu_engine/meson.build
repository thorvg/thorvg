# CPU vectorization is enabled only when simd is true and the target machine supports it.
simd_flags = []

if get_option('simd')
    simd_type = 'unsupported'
    cpp = meson.get_compiler('cpp')
    cpu_family = host_machine.cpu_family()

    # AVX2
    if cpu_family == 'x86' or cpu_family == 'x86_64'
        if cpp.get_id() == 'msvc'
            candidate_flags = ['/arch:AVX2']
        elif cpp.get_id() == 'clang-cl'
            candidate_flags = ['/clang:-mavx2']
        else
            candidate_flags = ['-mavx2']
        endif

        if cpp.compiles('''
            #include <immintrin.h>
            int main() {
                __m128i a = _mm_set1_epi32(1);
                __m128i b = _mm_mullo_epi32(a, a);
                __m256 c = _mm256_set1_ps(1.0f);
                return _mm_cvtsi128_si32(b) + _mm256_movemask_ps(c);
            }
        ''', args: candidate_flags, name: 'AVX raster support')
            simd_flags = candidate_flags
            simd_type = 'avx2'
            config_h.set10('THORVG_AVX_SUPPORT', true)
        endif
    # NEON
    elif cpu_family == 'arm' or cpu_family == 'aarch64'
        candidate_flags = []
        if cpu_family == 'arm'
            if cpp.get_id() == 'clang-cl'
                candidate_flags = ['/clang:-mfpu=neon']
            elif cpp.get_id() != 'msvc'
                candidate_flags = ['-mfpu=neon']
            endif
        endif

        if cpp.compiles('''
            #include <arm_neon.h>
            int main() {
                uint32x4_t a = vdupq_n_u32(1);
                return vgetq_lane_u32(vmulq_u32(a, a), 0);
            }
        ''', args: candidate_flags, name: 'NEON raster support')
            simd_flags = candidate_flags
            simd_type = cpu_family == 'arm' ? 'neon-arm' : 'neon-aarch'
            config_h.set10('THORVG_NEON_SUPPORT', true)
        endif
    endif
endif

source_file = [
   'tvgSwCommon.h',
   'tvgSwRasterCore.h',
   'tvgSwRasterTexmap.h',
   'tvgSwRenderer.h',
   'tvgSwBlendOp.cpp',
   'tvgSwFill.cpp',
   'tvgSwImage.cpp',
   'tvgSwMemPool.cpp',
   'tvgSwPostEffect.cpp',
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

engine_dep += [declare_dependency(
    include_directories : include_directories('.'),
    dependencies        : omp_dep,
    compile_args        : simd_flags,
    sources             : source_file
)]
