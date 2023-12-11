project('thorvg',
        'cpp',
        default_options : ['buildtype=debugoptimized', 'b_sanitize=none', 'werror=false', 'optimization=s', 'cpp_std=c++14', 'strip=true'],
        version : '0.11.99',
        license : 'MIT')

config_h = configuration_data()

src_dir = '/'.join(meson.current_source_dir().split('\\'))
add_project_arguments('-DEXAMPLE_DIR="@0@/src/examples/resources"'.format(src_dir),
                      '-DTEST_DIR="@0@/test/resources"'.format(src_dir),
                      language : 'cpp')

config_h.set_quoted('THORVG_VERSION_STRING', meson.project_version())

#Engines
if get_option('engines').contains('sw') == true
    config_h.set10('THORVG_SW_RASTER_SUPPORT', true)
endif

if get_option('engines').contains('gl_beta') == true
    config_h.set10('THORVG_GL_RASTER_SUPPORT', true)
endif

if get_option('engines').contains('wg_beta') == true
    config_h.set10('THORVG_WG_RASTER_SUPPORT', true)
endif

#Loaders
all_loaders = false

if get_option('loaders').contains('all') == true
    all_loaders = true
endif

if all_loaders or get_option('loaders').contains('svg') == true
    config_h.set10('THORVG_SVG_LOADER_SUPPORT', true)
endif

if all_loaders or get_option('loaders').contains('tvg') == true
    config_h.set10('THORVG_TVG_LOADER_SUPPORT', true)
endif

if all_loaders or get_option('loaders').contains('png') == true
   config_h.set10('THORVG_PNG_LOADER_SUPPORT', true)
endif

if all_loaders or get_option('loaders').contains('jpg') == true
    config_h.set10('THORVG_JPG_LOADER_SUPPORT', true)
endif

if all_loaders or get_option('loaders').contains('lottie') == true
    config_h.set10('THORVG_LOTTIE_LOADER_SUPPORT', true)
endif

if get_option('loaders').contains('webp_beta') == true
    config_h.set10('THORVG_WEBP_LOADER_SUPPORT', true)
endif



#Savers
all_savers = false

if get_option('savers').contains('all') == true
    all_savers = true
endif

if all_savers or get_option('savers').contains('tvg') == true
    config_h.set10('THORVG_TVG_SAVER_SUPPORT', true)
endif

if all_savers or get_option('savers').contains('gif') == true
    config_h.set10('THORVG_GIF_SAVER_SUPPORT', true)
endif

#Vectorization
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

#Bindings
if get_option('bindings').contains('capi') == true
    config_h.set10('THORVG_CAPI_BINDING_SUPPORT', true)
endif

if get_option('bindings').contains('wasm_beta') == true
    config_h.set10('THORVG_WASM_BINDING_SUPPORT', true)
endif

#Log
if get_option('log') == true
    config_h.set10('THORVG_LOG_ENABLED', true)
endif

#Tools
all_tools = false

if get_option('tools').contains('all') == true
    all_tools = true
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
    Raster Engine (WG):    @6@
    Loader (TVG):          @7@
    Loader (SVG):          @8@
    Loader (PNG):          @9@
    Loader (JPG):          @10@
    Loader (WEBP_BETA):    @11@
    Loader (LOTTIE):       @12@
    Saver (TVG):           @13@
    Saver (GIF):           @14@
    Binding (CAPI):        @15@
    Binding (WASM_BETA):   @16@
    Log Message:           @17@
    Tests:                 @18@
    Examples:              @19@
    Tool (Svg2Tvg):        @20@
    Tool (Svg2Png):        @21@
    Tool (Lottie2Gif):     @22@

'''.format(
        meson.project_version(),
        get_option('buildtype'),
        get_option('prefix'),
        simd_type,
        get_option('engines').contains('sw'),
        get_option('engines').contains('gl_beta'),
        get_option('engines').contains('wg_beta'),
        all_loaders or get_option('loaders').contains('tvg'),
        all_loaders or get_option('loaders').contains('svg'),
        all_loaders or get_option('loaders').contains('png'),
        all_loaders or get_option('loaders').contains('jpg'),
        get_option('loaders').contains('webp_beta'),
        all_loaders or get_option('loaders').contains('lottie'),
        all_savers or get_option('savers').contains('tvg'),
        all_savers or get_option('savers').contains('gif'),
        get_option('bindings').contains('capi'),
        get_option('bindings').contains('wasm_beta'),
        get_option('log'),
        get_option('tests'),
        get_option('examples'),
        all_tools or get_option('tools').contains('svg2tvg'),
        all_tools or get_option('tools').contains('svg2png'),
        all_tools or get_option('tools').contains('lottie2gif'),
    )

message(summary)
