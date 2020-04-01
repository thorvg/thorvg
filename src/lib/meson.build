subdir('sw_engine')
subdir('gl_engine')

source_file = [
   'tvgCommon.h',
   'tvgEngine.cpp',
   'tvgCanvasBase.h',
   'tvgShapePath.h',
   'tvgSwCanvas.cpp',
   'tvgGlCanvas.cpp',
   'tvgSceneNode.cpp',
   'tvgShapeNode.cpp'
]

src_dep = declare_dependency(
   include_directories : include_directories('.'),
   sources : source_file
)

