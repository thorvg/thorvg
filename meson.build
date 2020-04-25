project('tizenvg',
        'cpp',
        default_options : ['buildtype=debug', 'werror=false', 'cpp_std=c++14'],
        version : '0.1.0',
        license : 'Apache-2.0')

config_h = configuration_data()

configure_file(
  output: 'config.h',
  configuration: config_h
)

headers = [include_directories('inc')]

subdir('inc')
subdir('src')

summary = '''

Summary:
    tizenvg version :        @0@
    Build type      :        @1@
    Prefix          :        @2@
'''.format(
        meson.project_version(),
        get_option('buildtype'),
        get_option('prefix'),
    )

message(summary)
