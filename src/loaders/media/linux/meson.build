gst_deps = [
    dependency('gstreamer-1.0', required: false),
    dependency('gstreamer-app-1.0', required: false),
    dependency('gstreamer-video-1.0', required: false),
]

gst_found = gst_deps[0].found() and gst_deps[1].found() and gst_deps[2].found()

if gst_found
    compiler_flags += ['-DTHORVG_LINUX_MEDIA_SUPPORT=1']

    source_file = [
        'tvgGstMediaLoader.h',
        'tvgGstMediaLoader.cpp'
    ]

    media_dep += [declare_dependency(
        dependencies : gst_deps,
        include_directories : include_directories('.'),
        sources : source_file
    )]
else
    message('GStreamer (gstreamer-1.0/app/video) not found; media loader disabled.')
endif
