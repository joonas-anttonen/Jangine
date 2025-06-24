$ErrorActionPreference = "Stop"

$glfwRepo = "https://github.com/glfw/glfw.git"
$glfwDir = "glfw_src"
$tag = "3.4" # <-- Replace with your desired tag
$includeDir = "include"
$libDir = "lib"

# Clean up any previous run
if (Test-Path $glfwDir) { Remove-Item -Recurse -Force $glfwDir }
if (Test-Path $includeDir) { Remove-Item -Recurse -Force $includeDir }
if (Test-Path $libDir) { Remove-Item -Recurse -Force $libDir }

# Clone GLFW
git clone --branch $tag --depth 1 $glfwRepo $glfwDir

Push-Location $glfwDir

# Create build directory
mkdir build

# Configure and build with CMake (Ninja generator)
& cmake -S . -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release `
      -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL `
      -DGLFW_BUILD_EXAMPLES=OFF `
      -DGLFW_BUILD_TESTS=OFF `
      -DGLFW_BUILD_DOCS=OFF
& cmake --build build --config Release --verbose

# Copy include directory
Copy-Item -Recurse "include\GLFW" "..\include"

# Copy .lib files to lib directory
New-Item -ItemType Directory -Path "..\lib" | Out-Null
$libFiles = Get-ChildItem -Path "build" -Filter "*.lib" -Recurse
foreach ($file in $libFiles) {
    Copy-Item $file.FullName "..\lib\"
}

Pop-Location # out of glfw_src

# Clean up source and build directories
Remove-Item -Recurse -Force $glfwDir

Write-Host "GLFW built and artifacts copied to 'external/glfw/include' and 'external/glfw/lib'"