$ErrorActionPreference = "Stop"

$externalDirs = @("eigen", "freetype", "glfw", "harfbuzz", "json", "vma", "webp")

foreach ($dir in $externalDirs) {
    Push-Location $dir
    & ".\build.ps1"
    Pop-Location
}
