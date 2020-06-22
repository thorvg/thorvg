subdir('sw_engine')
subdir('gl_engine')

source_file = [
   'tvgCanvasImpl.h',
   'tvgCommon.h',
   'tvgLoader.h',
   'tvgLoaderMgr.h',
   'tvgRender.h',
   'tvgSceneImpl.h',
   'tvgShapePath.h',
   'tvgShapeImpl.h',
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

src_dep = declare_dependency(
   include_directories : include_directories('.'),
   sources : source_file
)

