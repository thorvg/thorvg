subdir('sw_engine')
subdir('gl_engine')

source_file = [
   'tvgCommon.h',
   'tvgRenderCommon.h',
   'tvgShapePath.h',
   'tvgShapeImpl.h',
   'tvgCanvasImpl.h',
   'tvgEngine.cpp',
   'tvgCanvas.cpp',
   'tvgSwCanvas.cpp',
   'tvgGlCanvas.cpp',
   'tvgScene.cpp',
   'tvgShape.cpp'
]

src_dep = declare_dependency(
   include_directories : include_directories('.'),
   sources : source_file
)

