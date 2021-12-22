rlottie_dep = dependency('rlottie', required: false)

subdir('lib')
subdir('inc')
subdir('loader')

extension_lottie_dep = declare_dependency(
    dependencies: [rlottie_dep, extension_source_dep, extension_loader_dep],
    include_directories : include_directories('.'),
)
