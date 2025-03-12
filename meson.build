project('thorvg',
        'cpp',
        default_options : ['buildtype=debugoptimized', 'b_sanitize=none', 'werror=false', 'optimization=3', 'cpp_std=c++14', 'strip=true'],
        version : '0.15.11',
        license : 'MIT')

config_h = configuration_data()

src_dir = '/'.join(meson.current_source_dir().split('\\'))
add_project_arguments('-DEXAMPLE_DIR="@0@/examples/resources"'.format(src_dir),
                      '-DTEST_DIR="@0@/test/resources"'.format(src_dir),
                      language : 'cpp')

config_h.set_quoted('THORVG_VERSION_STRING', meson.project_version())

#Multi-Tasking
if get_option('threads')
  config_h.set10('THORVG_THREAD_SUPPORT', true)
endif

#Engines
all_engines = get_option('engines').contains('all')

sw_engine = false
if all_engines or get_option('engines').contains('sw')
    sw_engine = true
    config_h.set10('THORVG_SW_RASTER_SUPPORT', true)
endif

gl_engine = false
if all_engines or get_option('engines').contains('gl')
    gl_engine = true
    config_h.set10('THORVG_GL_RASTER_SUPPORT', true)
endif

wg_engine = false
if get_option('engines').contains('wg_beta')
    wg_engine = true
    config_h.set10('THORVG_WG_RASTER_SUPPORT', true)
endif

#Tools
all_tools = get_option('tools').contains('all')
svg2tvg = all_tools or get_option('tools').contains('svg2tvg')
lottie2gif = all_tools or get_option('tools').contains('lottie2gif')
svg2png = all_tools or get_option('tools').contains('svg2png')

#Loaders
all_loaders = get_option('loaders').contains('all')
svg_loader = all_loaders or get_option('loaders').contains('svg') or svg2tvg or svg2png
tvg_loader = all_loaders or get_option('loaders').contains('tvg')
png_loader = all_loaders or get_option('loaders').contains('png')
jpg_loader = all_loaders or get_option('loaders').contains('jpg')
lottie_loader = all_loaders or get_option('loaders').contains('lottie') or lottie2gif
ttf_loader = all_loaders or get_option('loaders').contains('ttf')
webp_loader = all_loaders or get_option('loaders').contains('webp')

#Savers
all_savers = get_option('savers').contains('all')
tvg_saver = all_savers or get_option('savers').contains('tvg') or svg2tvg
gif_saver = all_savers or get_option('savers').contains('gif') or lottie2gif

#Loaders/savers/tools config
if svg_loader
    config_h.set10('THORVG_SVG_LOADER_SUPPORT', true)
endif

if tvg_loader
    config_h.set10('THORVG_TVG_LOADER_SUPPORT', true)
endif

if png_loader
   config_h.set10('THORVG_PNG_LOADER_SUPPORT', true)
endif

if jpg_loader
    config_h.set10('THORVG_JPG_LOADER_SUPPORT', true)
endif

if lottie_loader
    config_h.set10('THORVG_LOTTIE_LOADER_SUPPORT', true)
endif

if ttf_loader
    config_h.set10('THORVG_TTF_LOADER_SUPPORT', true)
endif

if webp_loader
    config_h.set10('THORVG_WEBP_LOADER_SUPPORT', true)
endif

if tvg_saver
    config_h.set10('THORVG_TVG_SAVER_SUPPORT', true)
endif

if gif_saver
    config_h.set10('THORVG_GIF_SAVER_SUPPORT', true)
endif

#Vectorization
simd_type = 'none'

if get_option('simd')
  if host_machine.cpu_family().startswith('x86')
    config_h.set10('THORVG_AVX_VECTOR_SUPPORT', true)
    simd_type = 'avx'
  elif host_machine.cpu_family().startswith('arm')
    config_h.set10('THORVG_NEON_VECTOR_SUPPORT', true)
    simd_type = 'neon-arm'
  elif host_machine.cpu().startswith('aarch')
    config_h.set10('THORVG_NEON_VECTOR_SUPPORT', true)
    simd_type = 'neon-aarch'
  endif
endif

#Bindings
if get_option('bindings').contains('capi')
    config_h.set10('THORVG_CAPI_BINDING_SUPPORT', true)
endif

if get_option('bindings').contains('wasm_beta')
    config_h.set10('THORVG_WASM_BINDING_SUPPORT', true)
endif

#Log
if get_option('log')
    config_h.set10('THORVG_LOG_ENABLED', true)
endif

#File IO
if get_option('file') == true
    config_h.set10('THORVG_FILE_IO_SUPPORT', true)
endif


#Extra
lottie_expressions = lottie_loader and get_option('extra').contains('lottie_expressions')

if lottie_expressions
    config_h.set10('THORVG_LOTTIE_EXPRESSIONS_SUPPORT', true)
endif

#Miscellaneous
config_h.set10('WIN32_LEAN_AND_MEAN', true)

configure_file(
    output: 'config.h',
    configuration: config_h
)

headers = [include_directories('inc'), include_directories('.')]

#OpenGL profile: OpenGLES(true) or OpenGL(false), confirmed by gl_engine
target_opengles = false

subdir('inc')
subdir('src')
subdir('tools')

if get_option('examples')
    subdir('examples')
endif

if get_option('tests')
   subdir('test')
endif

summary = '''

Summary:
    ThorVG version:             @0@
    Build Type:                 @1@
    Prefix:                     @2@
    Multi-Tasking:              @3@
    SIMD Instruction:           @4@
    Raster Engine (SW):         @5@
    Raster Engine (GL):         @6@
    Raster Engine (WG_BETA):    @7@
    Loader (TVG):               @8@
    Loader (SVG):               @9@
    Loader (TTF):               @10@
    Loader (LOTTIE):            @11@
    Loader (PNG):               @12@
    Loader (JPG):               @13@
    Loader (WEBP):              @14@
    Saver (TVG):                @15@
    Saver (GIF):                @16@
    Binding (CAPI):             @17@
    Binding (WASM_BETA):        @18@
    Log Message:                @19@
    Tests:                      @20@
    Examples:                   @21@
    Tool (Svg2Tvg):             @22@
    Tool (Svg2Png):             @23@
    Tool (Lottie2Gif):          @24@
    Extra (Lottie Expressions): @25@

'''.format(
        meson.project_version(),
        get_option('buildtype'),
        get_option('prefix'),
        get_option('threads'),
        simd_type,
        sw_engine,
        gl_engine,
        wg_engine,
        tvg_loader,
        svg_loader,
        ttf_loader,
        lottie_loader,
        png_loader,
        jpg_loader,
        webp_loader,
        tvg_saver,
        gif_saver,
        get_option('bindings').contains('capi'),
        get_option('bindings').contains('wasm_beta'),
        get_option('log'),
        get_option('tests'),
        get_option('examples'),
        svg2tvg,
        svg2png,
        lottie2gif,
        lottie_expressions
    )

message(summary)
