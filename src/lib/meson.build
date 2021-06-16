engine_dep = []

if get_option('engines').contains('sw') == true
    subdir('sw_engine')
    message('Enable SW Raster Engine')
endif

if get_option('engines').contains('gl') == true
    subdir('gl_engine')
    message('Enable GL Raster Engine')
endif

source_file = [
   'tvgCanvasImpl.h',
   'tvgCommon.h',
   'tvgBezier.h',
   'tvgBinaryDesc.h',
   'tvgFill.h',
   'tvgLoader.h',
   'tvgLoaderMgr.h',
   'tvgPictureImpl.h',
   'tvgRender.h',
   'tvgSceneImpl.h',
   'tvgShapeImpl.h',
   'tvgTaskScheduler.h',
   'tvgBezier.cpp',
   'tvgCanvas.cpp',
   'tvgFill.cpp',
   'tvgGlCanvas.cpp',
   'tvgInitializer.cpp',
   'tvgLinearGradient.cpp',
   'tvgLoaderMgr.cpp',
   'tvgPaint.cpp',
   'tvgPicture.cpp',
   'tvgRadialGradient.cpp',
   'tvgRender.cpp',
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
