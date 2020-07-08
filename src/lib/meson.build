subdir('sw_engine')
subdir('gl_engine')

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

engine_dep = [swengine_dep, glengine_dep]

common_dep = declare_dependency(
   dependencies        : engine_dep,
   include_directories : include_directories('.'),
   sources             : source_file
)
