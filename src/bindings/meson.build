subbinding_dep = []

if get_option('bindings').contains('capi') == true
    subdir('capi')
    message('Enable CAPI Bindings')
endif

binding_dep = declare_dependency(
   dependencies: subbinding_dep,
   include_directories : include_directories('.'),
)
