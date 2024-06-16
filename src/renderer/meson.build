engine_dep = []

if get_option('engines').contains('sw') == true
    subdir('sw_engine')
endif

if get_option('engines').contains('gl_beta') == true
    subdir('gl_engine')
endif

if get_option('engines').contains('wg_beta') == true
    subdir('wg_engine')
endif

source_file = [
   'tvgAnimation.h',
   'tvgCanvas.h',
   'tvgCommon.h',
   'tvgBinaryDesc.h',
   'tvgFill.h',
   'tvgFrameModule.h',
   'tvgLoader.h',
   'tvgLoadModule.h',
   'tvgPicture.h',
   'tvgRender.h',
   'tvgIteratorAccessor.h',
   'tvgSaveModule.h',
   'tvgScene.h',
   'tvgShape.h',
   'tvgTaskScheduler.h',
   'tvgText.h',
   'tvgAccessor.cpp',
   'tvgAnimation.cpp',
   'tvgCanvas.cpp',
   'tvgFill.cpp',
   'tvgGlCanvas.cpp',
   'tvgInitializer.cpp',
   'tvgLoader.cpp',
   'tvgPaint.cpp',
   'tvgPicture.cpp',
   'tvgRender.cpp',
   'tvgSaver.cpp',
   'tvgScene.cpp',
   'tvgShape.cpp',
   'tvgSwCanvas.cpp',
   'tvgTaskScheduler.cpp',
   'tvgText.cpp',
   'tvgWgCanvas.cpp'
]

common_dep = declare_dependency(
   dependencies        : engine_dep,
   include_directories : include_directories('.'),
   sources             : source_file
)
