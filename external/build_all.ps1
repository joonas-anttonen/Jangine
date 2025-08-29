$ErrorActionPreference = "Stop"

$externalDirs = @("eigen", "freetype", "glfw", "harfbuzz", "json", "tinyxml-2", "vma", "webp")

foreach ($dir in $externalDirs) {
    Push-Location $dir
    & ".\build.ps1"
    Pop-Location
}
