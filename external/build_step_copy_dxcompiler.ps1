param (
    [string]$TargetDir
)

$src = Join-Path $env:VULKAN_SDK "Bin\dxcompiler.dll"
if (!(Test-Path $src)) {
    Write-Error "dxcompiler.dll not found at $src"
    exit 1
}
Copy-Item $src -Destination $TargetDir -Force