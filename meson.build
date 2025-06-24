project(
    'Jangine', 
    'cpp', 
    version: '1.0.0',
    default_options: [
        'cpp_std=c++20',
        'buildtype=release',
        'warning_level=3',
        'werror=true',
])

vulkan = dependency('vulkan', required: true)
eigen_includes = include_directories('external/eigen/include')
glfw_includes = include_directories('external/glfw/include')
glfw_lib = '../external/glfw/lib/glfw3.lib'
json_includes = include_directories('external/json/include')
vma_includes = include_directories('external/vma/include')

jangine = library(
    'Jangine', 
    'Jangine/Core.cpp',
    'Jangine/Logging/Log.cpp',
    'Jangine/Logging/Logger.cpp',
    'Jangine/Gfx/Core.cpp',
    'Jangine/Gfx/Presenter.cpp',
    'Jangine/Gfx/Core2D.cpp',
    'Jangine/Gui/Core.cpp',
    include_directories: [
        include_directories('Jangine/'),
        eigen_includes,
        glfw_includes,
        json_includes,
        vma_includes
    ],
    dependencies: [vulkan],
    cpp_args: [
        '-DJANGINE_BUILD'
    ],
    link_args: [glfw_lib],
)

executable(
    'Jangine.Test', 
    'Jangine.Test/Jangine.Test.cpp',
    link_with: jangine)