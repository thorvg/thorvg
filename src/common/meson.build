source_file = [
   'tvgArray.h',
   'tvgCommon.h',
   'tvgColor.h',
   'tvgCompressor.h',
   'tvgInlist.h',
   'tvgLock.h',
   'tvgMath.h',
   'tvgStr.h',
   'tvgColor.cpp',
   'tvgCompressor.cpp',
   'tvgMath.cpp',
   'tvgStr.cpp'
]

static_png  = false
if png_saver or png_loader
   if get_option('static')
      static_png = true
   else 
      png_dep = dependency('libpng', required: false)
      static_png  =  not png_dep.found()
   endif
endif

if static_png 
   source_file += [
      'tvgPngCodec.h',
      'tvgPngCodec.cpp',
   ]
endif

utils_dep = declare_dependency(
    include_directories : include_directories('.'),
    sources : source_file)
