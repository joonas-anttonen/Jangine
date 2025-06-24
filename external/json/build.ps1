$ErrorActionPreference = "Stop"

$scriptDir = [System.IO.Path]::GetDirectoryName($MyInvocation.MyCommand.Definition)
$gitRepo = "https://github.com/nlohmann/json.git"
$srcDir = "src"
$includeDir = "include"

# Clean up any previous run
if (Test-Path $srcDir) { Remove-Item -Recurse -Force $srcDir }
if (Test-Path $includeDir) { Remove-Item -Recurse -Force $includeDir }

# Clone
git clone --depth 1 $gitRepo $srcDir

# Copy only the nlohmann directory from single_include to include
New-Item -ItemType Directory -Path $includeDir | Out-Null
Copy-Item -Recurse "$srcDir\single_include\nlohmann" "$includeDir\"

# Copy LICENSE file (any extension)
$license = Get-ChildItem -Path $srcDir -Filter "LICENSE*" -File | Select-Object -First 1
if ($license) {
    Copy-Item $license.FullName $scriptDir
}

# Clean up source directory
Remove-Item -Recurse -Force $srcDir

Write-Host "nlohmann/json OK" -ForegroundColor Green