$ErrorActionPreference = "Stop"

$scriptDir = [System.IO.Path]::GetDirectoryName($MyInvocation.MyCommand.Definition)
$gitRepo = "https://github.com/glfw/glfw.git"
$srcDir = "src"
$tag = "3.4"
$includeDir = "include"
$libDir = "lib"

# Clean up any previous run
if (Test-Path $srcDir) { Remove-Item -Recurse -Force $srcDir }
if (Test-Path $includeDir) { Remove-Item -Recurse -Force $includeDir }
if (Test-Path $libDir) { Remove-Item -Recurse -Force $libDir }

# Clone
git clone --branch $tag --depth 1 $gitRepo $srcDir

Push-Location $srcDir

# Create build directory
mkdir build

# Configure and build
& cmake -S . -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release `
      -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL `
      -DGLFW_BUILD_EXAMPLES=OFF `
      -DGLFW_BUILD_TESTS=OFF `
      -DGLFW_BUILD_DOCS=OFF
& cmake --build build --config Release

# Copy include directory
Copy-Item -Recurse "include\GLFW" "..\include"

# Copy .lib files to lib directory
New-Item -ItemType Directory -Path "..\lib" | Out-Null
$libFiles = Get-ChildItem -Path "build" -Filter "*.lib" -Recurse
foreach ($file in $libFiles) {
    Copy-Item $file.FullName "..\lib\"
}

# Copy LICENSE file (any extension)
$license = Get-ChildItem -Path . -Filter "LICENSE*" -File | Select-Object -First 1
if ($license) {
    Copy-Item $license.FullName $scriptDir
}

Pop-Location # out of src

# Clean up source and build directories
Remove-Item -Recurse -Force $srcDir

Write-Host "GLFW OK" -ForegroundColor Green