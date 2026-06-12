gst_dep = dependency('gstreamer-1.0', required: false)

if gst_dep.found()
    compiler_flags += ['-DTHORVG_LINUX_MEDIA_SUPPORT=1']

    source_file = [
        'tvgGstMediaLoader.h'
    ]

    media_dep += [declare_dependency(
        dependencies : gst_dep,
        include_directories : include_directories('.'),
        sources : source_file
    )]
endif
