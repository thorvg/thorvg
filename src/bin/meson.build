if (lib_type == 'static')
    compiler_flags += ['-DTVG_STATIC']
endif

if all_tools or get_option('tools').contains('svg2png') == true
   subdir('svg2png')
endif

if all_tools or get_option('tools').contains('svg2tvg') == true
   subdir('svg2tvg')
endif

