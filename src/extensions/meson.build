extension_dep = []

if get_option('extensions').contains('lottie') == true
    subdir('lottie')
endif

extension_dep = declare_dependency(
    dependencies: subloader_dep,
    include_directories : include_directories('.'),
)
