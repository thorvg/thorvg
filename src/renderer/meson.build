engine_dep = []

if sw_engine
    subdir('sw_engine')
endif

if gl_engine
    subdir('gl_engine')
endif

if wg_engine
    subdir('wg_engine')
endif

source_file = [
   'tvgAnimation.h',
   'tvgCanvas.h',
   'tvgCommon.h',
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
   'tvgTrimPath.h',
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
   'tvgTrimPath.cpp',
   'tvgWgCanvas.cpp'
]

common_dep = declare_dependency(
   dependencies        : engine_dep,
   include_directories : include_directories('.'),
   sources             : source_file
)
