project('Jangine', 'cpp', version: '1.0.0', default_options: ['cpp_std=c++20'])

vulkan = dependency('vulkan', required: true)

jangine = library(
    'Jangine', 
    'Jangine/Core.cpp',
    'Jangine/Logging/Log.cpp',
    'Jangine/Logging/Logger.cpp',
    'Jangine/Gfx/Core.cpp',
    include_directories: include_directories('Jangine/'),
    dependencies: [vulkan],
    cpp_args: [
        '-DJANGINE_BUILD',
        '-Wall',
        '-Wextra',
        '-Werror'
    ]
)

executable(
    'Jangine.Test', 
    'Jangine.Test/Jangine.Test.cpp',
    link_with: jangine)