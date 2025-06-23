project('thorvg',
        'cpp',
        default_options : ['buildtype=debugoptimized', 'b_sanitize=none', 'werror=false', 'optimization=3', 'cpp_std=c++14', 'strip=true'],
        version : '1.0.0',
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
if all_engines or get_option('engines').contains('wg')
    wg_engine = true
    config_h.set10('THORVG_WG_RASTER_SUPPORT', true)
endif

#Tools
all_tools = get_option('tools').contains('all')
lottie2gif = all_tools or get_option('tools').contains('lottie2gif')
svg2png = all_tools or get_option('tools').contains('svg2png')

#Loaders
all_loaders = get_option('loaders').contains('all')
svg_loader = all_loaders or get_option('loaders').contains('svg') or svg2png
png_loader = all_loaders or get_option('loaders').contains('png')
jpg_loader = all_loaders or get_option('loaders').contains('jpg')
lottie_loader = all_loaders or get_option('loaders').contains('lottie') or lottie2gif
ttf_loader = all_loaders or get_option('loaders').contains('ttf')
webp_loader = all_loaders or get_option('loaders').contains('webp')

#Savers
all_savers = get_option('savers').contains('all')
gif_saver = all_savers or get_option('savers').contains('gif') or lottie2gif

#logging
logging = get_option('log')

#Loaders/savers/tools config
if svg_loader
    config_h.set10('THORVG_SVG_LOADER_SUPPORT', true)
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
if logging
    config_h.set10('THORVG_LOG_ENABLED', true)
endif

#Extra
file_io = get_option('extra').contains('file_io')

if file_io
    config_h.set10('THORVG_FILE_IO_SUPPORT', true)
endif

partial_render = get_option('extra').contains('partial_render')

if partial_render
    config_h.set10('THORVG_PARTIAL_RENDER_SUPPORT', true)
endif

lottie_expressions = lottie_loader and get_option('extra').contains('lottie_expressions')

if lottie_expressions
    config_h.set10('THORVG_LOTTIE_EXPRESSIONS_SUPPORT', true)
endif

gl_variant = ''

if gl_engine
    if get_option('extra').contains('opengl_es')
        gl_variant = 'OpenGL ES'
    else
        gl_variant = 'OpenGL'
    endif
else
    gl_variant = 'None'
endif

#Miscellaneous
config_h.set10('WIN32_LEAN_AND_MEAN', true)

configure_file(
    output: 'config.h',
    configuration: config_h
)

headers = [include_directories('inc'), include_directories('.')]

subdir('inc')
subdir('src')
subdir('tools')

if get_option('examples')
    subdir('examples')
endif

if get_option('tests')
   subdir('test')
endif

summary(
  {
    'Build Type': get_option('buildtype'),
    'Prefix': get_option('prefix'),
    'Multi-Tasking': get_option('threads'),
    'SIMD Instruction': simd_type,
    'Log Message': get_option('log'),
    'Tests': get_option('tests'),
    'Examples': get_option('examples'),
  },
  bool_yn: true,
)

summary(
  {
    'SW': sw_engine,
    'GL': gl_engine,
    'WG': wg_engine,
  },
  section: 'Raster Engine',
  bool_yn: true,
)

summary(
  {
    'SVG': svg_loader,
    'TTF': ttf_loader,
    'LOTTIE': lottie_loader,
    'PNG': png_loader,
    'JPG': jpg_loader,
    'WEBP': webp_loader,
  },
  section: 'Loader',
  bool_yn: true,
)

summary(
  {
    'GIF': gif_saver,
  },
  section: 'Saver',
  bool_yn: true,
)

summary(
  {
    'CAPI': get_option('bindings').contains('capi'),
    'WASM_BETA': get_option('bindings').contains('wasm_beta'),
  },
  section: 'Binding',
  bool_yn: true,
)

summary(
  {
    'Svg2Png': svg2png,
    'Lottie2Gif': lottie2gif,
  },
  section: 'Tool',
  bool_yn: true,
)

summary(
  {
    'File I/O': file_io,
    'Partial Rendering': partial_render,
    'Lottie Expressions': lottie_expressions,
    'OpenGL Variant': gl_variant,
  },
  section: 'Extra',
  bool_yn: true,
)
