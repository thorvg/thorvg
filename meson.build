project('thorvg',
        'cpp',
        default_options : ['buildtype=debugoptimized', 'werror=false', 'optimization=s'],
        version : '0.1.0',
        license : 'MIT')

config_h = configuration_data()

if get_option('engines').contains('sw') == true
    config_h.set10('THORVG_SW_RASTER_SUPPORT', true)
endif

if get_option('engines').contains('gl') == true
    config_h.set10('THORVG_GL_RASTER_SUPPORT', true)
endif

if get_option('loaders').contains('svg') == true
    config_h.set10('THORVG_SVG_LOADER_SUPPORT', true)
endif

if get_option('vectors').contains('avx') == true
    config_h.set10('THORVG_AVX_VECTOR_SUPPORT', true)
endif

if get_option('bindings').contains('capi') == true
    config_h.set10('THORVG_CAPI_BINDING_SUPPORT', true)
endif

configure_file(
    output: 'config.h',
    configuration: config_h
)

headers = [include_directories('inc'), include_directories('.')]

subdir('inc')
subdir('src')

if get_option('example') == true
    subdir('example')
endif

summary = '''

Summary:
    thorvg version :        @0@
    Build type      :       @1@
    Prefix          :       @2@
'''.format(
        meson.project_version(),
        get_option('buildtype'),
        get_option('prefix'),
    )

message(summary)
