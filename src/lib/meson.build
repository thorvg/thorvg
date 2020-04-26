subdir('sw_engine')
subdir('gl_engine')

source_file = [
   'tvgCommon.h',
   'tvgRenderCommon.h',
   'tvgEngine.cpp',
   'tvgShapePath.h',
   'tvgCanvas.cpp',
   'tvgSwCanvas.cpp',
   'tvgGlCanvas.cpp',
   'tvgSceneNode.cpp',
   'tvgShapeNode.cpp'
]

src_dep = declare_dependency(
   include_directories : include_directories('.'),
   sources : source_file
)

