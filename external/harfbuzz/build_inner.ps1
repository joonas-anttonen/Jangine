$scriptDir = [System.IO.Path]::GetDirectoryName($MyInvocation.MyCommand.Definition)
$gitRepo = "https://github.com/harfbuzz/harfbuzz.git"
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

# Patch the weird 'and' operator in hb-ot-var-avar-table.hh
(Get-Content "src/hb-ot-var-avar-table.hh") -replace ' and map', ' && map' | Set-Content "src/hb-ot-var-avar-table.hh"

& cmake -S . -B build -G Ninja `
    -DCMAKE_BUILD_TYPE=Release `
    -DFREETYPE_INCLUDE_DIRS="../../freetype/include/freetype2" `
    -DFREETYPE_LIBRARY="../../freetype/lib/freetype.lib" `
    -DBUILD_SHARED_LIBS=OFF `
    -DHB_BUILD_UTILS=OFF `
    -DHB_BUILD_SUBSET=OFF `
    -DHB_HAVE_FREETYPE=ON `
    -DHB_HAVE_GLIB=OFF `
    -DHB_HAVE_GOBJECT=OFF `
    -DHB_HAVE_UNISCRIBE=OFF `
    -DHB_HAVE_GDI=OFF `
    -DHB_HAVE_DIRECTWRITE=OFF `
    -DHB_HAVE_INTROSPECTION=OFF
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
Remove-Item -Recurse -Force $srcDir

Write-Host "HarfBuzz OK" -ForegroundColor Green