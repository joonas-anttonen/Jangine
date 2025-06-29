$ErrorActionPreference = "Stop"

$scriptDir = [System.IO.Path]::GetDirectoryName($MyInvocation.MyCommand.Definition)
$gitRepo = "https://chromium.googlesource.com/webm/libwebp"
$srcDir = "src"
$includeDir = "include"
$libDir = "lib"

# Clean up any previous run
if (Test-Path $srcDir) { Remove-Item -Recurse -Force $srcDir }
if (Test-Path $includeDir) { Remove-Item -Recurse -Force $includeDir }
if (Test-Path $libDir) { Remove-Item -Recurse -Force $libDir }

# Clone
git clone --depth 1 $gitRepo $srcDir

Push-Location $srcDir

& cmake -S . `
    -B build `
    -G "Ninja" `
    -DCMAKE_BUILD_TYPE=Release `
    -DWEBP_BUILD_ANIM_UTILS=OFF `
    -DWEBP_BUILD_CWEBP=OFF `
    -DWEBP_BUILD_DWEBP=OFF `
    -DWEBP_BUILD_GIF2WEBP=OFF `
    -DWEBP_BUILD_IMG2WEBP=OFF `
    -DWEBP_BUILD_VWEBP=OFF `
    -DWEBP_BUILD_WEBPINFO=OFF `
    -DWEBP_BUILD_LIBWEBPMUX=ON `
    -DWEBP_BUILD_WEBPMUX=OFF `
    -DWEBP_BUILD_EXTRAS=OFF `
    -DWEBP_BUILD_WEBP_JS=OFF `
    -DWEBP_BUILD_FUZZTEST=OFF | Out-Null
& cmake --build build --config Release
& cmake --install build --config Release --prefix build/install

Pop-Location

# Ensure directories exist
New-Item -ItemType Directory -Path $includeDir -Force
New-Item -ItemType Directory -Path $libDir -Force

# Copy include files
Copy-Item -Path "$srcDir/build/install/include/*" -Destination "$includeDir" -Recurse

# Copy lib files
Copy-Item -Path "$srcDir/build/install/lib/*.lib" -Destination "$libDir" -Recurse

# Copy LICENSE file (any extension)
$license = Get-ChildItem -Path $srcDir -Filter "LICENSE*" -File | Select-Object -First 1
if ($license) {
    Copy-Item $license.FullName $scriptDir
}

# Clean up source and build directories
#Remove-Item -Recurse -Force $srcDir

Write-Host "WebP OK" -ForegroundColor Green