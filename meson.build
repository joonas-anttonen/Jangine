project(
    'Jangine',
    'cpp',
    version: '1.0.0',
    default_options: [
        'cpp_std=c++20',
        'buildtype=release',
        'warning_level=3',
        'werror=true',
    ],
)

#
# External dependencies
#
vulkan = dependency('vulkan', required: true)
eigen = declare_dependency(include_directories: include_directories('external/eigen/include'))
freetype = declare_dependency(
    include_directories: include_directories('external/freetype/include/freetype2'),
    link_args: ['../external/freetype/lib/freetype.lib'],
)
glfw = declare_dependency(
    include_directories: include_directories('external/glfw/include'),
    link_args: ['../external/glfw/lib/glfw3.lib'],
)
harfbuzz = declare_dependency(
    include_directories: include_directories('external/harfbuzz/include'),
    link_args: ['../external/harfbuzz/lib/harfbuzz.lib'],
)
json = declare_dependency(include_directories: include_directories('external/json/include'))
tinyxml2 = declare_dependency(
    include_directories: include_directories('external/tinyxml-2/include'),
    link_args: ['../external/tinyxml-2/lib/tinyxml2.lib'],
)
vma = declare_dependency(include_directories: include_directories('external/vma/include'))
webp = declare_dependency(
    include_directories: include_directories('external/webp/include'),
    link_args: ['../external/webp/lib/libsharpyuv.lib', '../external/webp/lib/libwebp.lib', '../external/webp/lib/libwebpdecoder.lib',
                 '../external/webp/lib/libwebpdemux.lib', '../external/webp/lib/libwebpmux.lib'],
)

#
# Build the font compiler executable
#
font_compiler = executable(
    'Jangine.Gfx.FontCompiler',
    'Jangine/Tools/FontCompiler.cpp',
    include_directories: [include_directories('Jangine/')],
    dependencies: [vulkan, eigen, json, freetype, harfbuzz],
    cpp_args: ['-DJANGINE_FONT_COMPILER'],
)

#
# Use the font compiler to generate font headers
#
built_in_fonts = custom_target(
    'built-in-fonts',
    output: 'BuiltInFonts.hpp',
    input: [
        'Jangine/Gfx/Text/ProggyTiny.ttf',
    ],
    command: [font_compiler, '@OUTPUT@', '--namespace=Jangine::Gfx::Text', '@INPUT@'],
    build_by_default: true,
)

#
# Build the shader compiler executable
#
shader_compiler = executable(
    'Jangine.Gfx.ShaderCompiler',
    ['Jangine/Tools/ShaderCompiler.cpp', 'Jangine/Gfx/ShaderProgram.cpp'],
    include_directories: [include_directories('Jangine/')],
    dependencies: [vulkan, eigen, json],
    cpp_args: ['-DJANGINE_SHADER_COMPILER'],
)

#
# Use the shader compiler to generate built-in shader headers
#
built_in_shaders = custom_target(
    'built-in-shaders',
    output: 'BuiltInShaders.hpp',
    input: [
        'Jangine/Gfx/Shaders/built-in-2d.hlsl',
        'Jangine/Gfx/Shaders/built-in-2d-composition.hlsl',
        'Jangine/Gfx/Shaders/built-in-blur.hlsl',
        'Jangine/Gfx/Shaders/built-in-mesh.hlsl',
        'Jangine/Gfx/Shaders/built-in-oit-composition.hlsl',
        'Jangine/Gfx/Shaders/built-in-shape-disc.hlsl',
        'Jangine/Gfx/Shaders/built-in-shape-line.hlsl',
    ],
    command: [shader_compiler, '@OUTPUT@', '--namespace=Jangine::Gfx', '@INPUT@'],
    build_by_default: true,
)

#
# Copy dxcompiler from Vulkan SDK to the build directory
# Custom script since Meson does not support configuring based on the environment
#
system = host_machine.system()
if system == 'windows'
    script_path = join_paths(
        meson.project_source_root(),
        'external',
        'build_step_copy_dxcompiler.ps1',
    )
    custom_target(
        'copy-dxcompiler',
        output: 'dxcompiler.dll',
        command: ['powershell', '-File', script_path],
        build_by_default: true,
    )
else
    error('Unsupported platform for dxcompiler copy')
endif

#
# Main library
#
jangine = library(
    'Jangine',
    [
        'Jangine/Core.cpp',
        'Jangine/Logging/Log.cpp',
        'Jangine/Logging/Logger.cpp',
        'Jangine/Gfx/Core.cpp',
        'Jangine/Gfx/Presenter.cpp',
        'Jangine/Gfx/PixelBuffer.cpp',
        'Jangine/Gfx/ShaderProgram.cpp',
        'Jangine/Gfx/Core2D.cpp',
        'Jangine/Gfx/Core3D.cpp',
        'Jangine/Gfx/Core3D.Scene.cpp',
        'Jangine/Gfx/Camera.cpp',
        'Jangine/Gfx/Text/Font.cpp',
        'Jangine/Gfx/Text/FontCollection.cpp',
        'Jangine/Gfx/Text/Shaper.cpp',
        'Jangine/IO/Gltf.cpp',
        'Jangine/IO/Urdf.cpp',
        'Jangine/IO/WebP.cpp',
        'Jangine/Gui/Core.cpp',
        built_in_fonts,
        built_in_shaders,
    ],
    include_directories: [include_directories('Jangine/')],
    dependencies: [vulkan, eigen, freetype, glfw, harfbuzz, json, tinyxml2, vma, webp],
    cpp_args: ['-DJANGINE_BUILD'],
)

#
# Test executable
#
executable('Jangine.Test', 'Jangine.Test/Jangine.Test.cpp', dependencies: [eigen], link_with: jangine)