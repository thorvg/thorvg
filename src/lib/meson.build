engine_dep = []

if get_option('engines').contains('sw') == true
    subdir('sw_engine')
endif

if get_option('engines').contains('gl') == true
    subdir('gl_engine')
endif

source_file = [
   'tvgCanvasImpl.h',
   'tvgCommon.h',
   'tvgBezier.h',
   'tvgBinaryDesc.h',
   'tvgFill.h',
   'tvgLoader.h',
   'tvgLoadModule.h',
   'tvgLzw.h',
   'tvgPictureImpl.h',
   'tvgRender.h',
   'tvgSaveModule.h',
   'tvgSceneImpl.h',
   'tvgShapeImpl.h',
   'tvgTaskScheduler.h',
   'tvgBezier.cpp',
   'tvgCanvas.cpp',
   'tvgFill.cpp',
   'tvgGlCanvas.cpp',
   'tvgInitializer.cpp',
   'tvgLinearGradient.cpp',
   'tvgLzw.cpp',
   'tvgLoader.cpp',
   'tvgPaint.cpp',
   'tvgPicture.cpp',
   'tvgRadialGradient.cpp',
   'tvgRender.cpp',
   'tvgSaver.cpp',
   'tvgScene.cpp',
   'tvgShape.cpp',
   'tvgSwCanvas.cpp',
   'tvgTaskScheduler.cpp',
]

common_dep = declare_dependency(
   dependencies        : engine_dep,
   include_directories : include_directories('.'),
   sources             : source_file
)
