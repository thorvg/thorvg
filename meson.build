project('thorvg',
        'cpp',
        default_options : ['buildtype=debugoptimized', 'b_sanitize=none', 'werror=false', 'optimization=s'],
        version : '0.4.99',
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

if get_option('loaders').contains('tvg_beta') == true
    config_h.set10('THORVG_TVG_LOADER_SUPPORT', true)
endif

if get_option('loaders').contains('png') == true
    config_h.set10('THORVG_PNG_LOADER_SUPPORT', true)
endif

if get_option('loaders').contains('jpg') == true
    config_h.set10('THORVG_JPG_LOADER_SUPPORT', true)
endif

if get_option('savers').contains('tvg_beta') == true
    config_h.set10('THORVG_TVG_SAVER_SUPPORT', true)
endif

if get_option('vectors').contains('avx') == true
    config_h.set10('THORVG_AVX_VECTOR_SUPPORT', true)
endif

if get_option('vectors').contains('neon') == true
    config_h.set10('THORVG_NEON_VECTOR_SUPPORT', true)
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
    Raster Engine (SW):    @3@
    Raster Engine (GL):    @4@
    AVX SIMD Instruction:  @5@
    Neon SIMD Instruction: @6@
    Loader (TVG):          @7@
    Loader (SVG):          @8@
    Loader (PNG):          @9@
    Loader (JPG):          @10@
    Saver (TVG):           @11@
    CAPI Binding:          @12@
    Log Message:           @13@
    Tests:                 @14@
    Examples:              @15@
    Tool (Svg2Tvg):        @16@
    Tool (Svg2Png):        @17@

'''.format(
        meson.project_version(),
        get_option('buildtype'),
        get_option('prefix'),
        get_option('engines').contains('sw'),
        get_option('engines').contains('gl'),
        get_option('vectors').contains('avx'),
        get_option('vectors').contains('neon'),
        get_option('loaders').contains('tvg_beta'),
        get_option('loaders').contains('svg'),
        get_option('loaders').contains('png'),
        get_option('loaders').contains('jpg'),
        get_option('savers').contains('tvg_beta'),
        get_option('bindings').contains('capi'),
        get_option('log'),
        get_option('tests'),
        get_option('examples'),
        get_option('tools').contains('svg2tvg'),
        get_option('tools').contains('svg2png'),
    )

message(summary)
