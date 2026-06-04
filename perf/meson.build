perf_compiler_flags = compiler_flags

if lib_type == 'static'
    perf_compiler_flags += ['-DTVG_STATIC']
endif

perf_dep = []
if host_machine.system() == 'darwin'
    perf_dep += declare_dependency(link_args: ['-framework', 'Cocoa', '-framework', 'IOKit'])
endif

executable('perfJpg',
    'perfJpg.cpp',
    include_directories : headers,
    link_with : thorvg_lib,
    cpp_args : perf_compiler_flags,
    dependencies : perf_dep)
