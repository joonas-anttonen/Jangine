$ErrorActionPreference = "Stop"

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsInstallPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars64 = Join-Path $vsInstallPath "VC\Auxiliary\Build\vcvars64.bat"
if (!(Test-Path $vcvars64)) {
    throw "Could not find vcvars64.bat. Please ensure Visual Studio is installed."
}
$script = "build_inner.ps1"
$cmd = "`"$vcvars64`" && powershell -File `"$script`""
& cmd /c $cmd
