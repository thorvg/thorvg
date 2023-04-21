svg2png_src  = files('svg2png.cpp', 'lodepng.cpp')

executable('svg2png',
           svg2png_src,
           include_directories : headers,
           cpp_args: compiler_flags,
           link_with : thorvg_lib)
