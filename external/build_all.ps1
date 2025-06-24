$ErrorActionPreference = "Stop"

$externalDirs = @("glfw", "vma", "eigen")

foreach ($dir in $externalDirs) {
    Push-Location $dir
    & ".\build.ps1"
    Pop-Location
}
