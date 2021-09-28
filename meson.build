project('thorvg',
        'cpp',
        default_options : ['buildtype=debugoptimized', 'b_sanitize=none', 'werror=false', 'optimization=s', 'cpp_std=c++14'],
        version : '0.5.99',
        license : 'MIT')

config_h = configuration_data()

add_project_arguments('-DEXAMPLE_DIR="@0@/src/examples/images"'.format(meson.current_source_dir()),
                      '-DTEST_DIR="@0@/test/images"'.format(meson.current_source_dir()),
                      language : 'cpp')

config_h.set_quoted('THORVG_VERSION_STRING', meson.project_version())

if get_option('engines').contains('sw') == true
    config_h.set10('THORVG_SW_RASTER_SUPPORT', true)
endif

if get_option('engines').contains('gl') == true
    config_h.set10('THORVG_GL_RASTER_SUPPORT', true)
endif

if get_option('loaders').contains('svg') == true
    config_h.set10('THORVG_SVG_LOADER_SUPPORT', true)
endif

if get_option('loaders').contains('tvg') == true
    config_h.set10('THORVG_TVG_LOADER_SUPPORT', true)
endif

if get_option('loaders').contains('png') == true
    config_h.set10('THORVG_PNG_LOADER_SUPPORT', true)
endif

if get_option('loaders').contains('jpg') == true
    config_h.set10('THORVG_JPG_LOADER_SUPPORT', true)
endif

if get_option('savers').contains('tvg') == true
    config_h.set10('THORVG_TVG_SAVER_SUPPORT', true)
endif

simd_type = 'none'

if get_option('vector') == true
  if host_machine.cpu_family() == 'x86' or host_machine.cpu_family() == 'x86_64'
    config_h.set10('THORVG_AVX_VECTOR_SUPPORT', true)
    simd_type = 'avx'
  elif host_machine.cpu_family() == 'arm'
    config_h.set10('THORVG_NEON_VECTOR_SUPPORT', true)
    simd_type = 'neon'
  endif
endif

if get_option('bindings').contains('capi') == true
    config_h.set10('THORVG_CAPI_BINDING_SUPPORT', true)
endif

if get_option('log') == true
    config_h.set10('THORVG_LOG_ENABLED', true)
endif

configure_file(
    output: 'config.h',
    configuration: config_h
)

headers = [include_directories('inc'), include_directories('.')]

subdir('inc')
subdir('src')

if get_option('tests') == true
   subdir('test')
endif

summary = '''

Summary:
    ThorVG version:        @0@
    Build Type:            @1@
    Prefix:                @2@
    SIMD Instruction:      @3@
    Raster Engine (SW):    @4@
    Raster Engine (GL):    @5@
    Loader (TVG):          @6@
    Loader (SVG):          @7@
    Loader (PNG):          @8@
    Loader (JPG):          @9@
    Saver (TVG):           @10@
    CAPI Binding:          @11@
    Log Message:           @12@
    Tests:                 @13@
    Examples:              @14@
    Tool (Svg2Tvg):        @15@
    Tool (Svg2Png):        @16@

'''.format(
        meson.project_version(),
        get_option('buildtype'),
        get_option('prefix'),
        simd_type,
        get_option('engines').contains('sw'),
        get_option('engines').contains('gl'),
        get_option('loaders').contains('tvg'),
        get_option('loaders').contains('svg'),
        get_option('loaders').contains('png'),
        get_option('loaders').contains('jpg'),
        get_option('savers').contains('tvg'),
        get_option('bindings').contains('capi'),
        get_option('log'),
        get_option('tests'),
        get_option('examples'),
        get_option('tools').contains('svg2tvg'),
        get_option('tools').contains('svg2png'),
    )

message(summary)
