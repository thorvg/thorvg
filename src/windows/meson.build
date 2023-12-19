engine_dep = []

source_file = [
   'tvgWindowImpl.h',
   'tvgWindow.cpp',
]

glew_dep = dependency('glew', required: true)
glfw3_dep = dependency('glfw3', required: true)

engine_dep += glew_dep
engine_dep += glfw3_dep

windows_dep = declare_dependency(
   dependencies        : engine_dep,
   include_directories : include_directories('.'),
   sources             : source_file
)
