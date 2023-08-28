svg2tvg_src  = files('svg2tvg.cpp')

executable('svg2tvg',
           svg2tvg_src,
           include_directories : headers,
           cpp_args : compiler_flags,
           install : true,
           link_with : thorvg_lib)
