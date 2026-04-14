subloader_dep += [declare_dependency(
    compile_args : [
        '-DJERRY_CPOINTER_32_BIT=' + (lottie_jerry_system_allocator ? '1' : '0'),
        '-DJERRY_SYSTEM_ALLOCATOR=' + (lottie_jerry_system_allocator ? '1' : '0'),
    ]
)]

subdir('jerry-core')
