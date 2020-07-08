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
   'tvgLoader.h',
   'tvgLoaderMgr.h',
   'tvgRender.h',
   'tvgSceneImpl.h',
   'tvgShapePath.h',
   'tvgShapeImpl.h',
   'tvgBezier.cpp',
   'tvgCanvas.cpp',
   'tvgFill.cpp',
   'tvgGlCanvas.cpp',
   'tvgInitializer.cpp',
   'tvgLinearGradient.cpp',
   'tvgLoaderMgr.cpp',
   'tvgRadialGradient.cpp',
   'tvgRender.cpp',
   'tvgScene.cpp',
   'tvgShape.cpp',
   'tvgSwCanvas.cpp',
]

common_dep = declare_dependency(
   dependencies        : engine_dep,
   include_directories : include_directories('.'),
   sources             : source_file
)
