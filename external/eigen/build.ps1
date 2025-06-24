$ErrorActionPreference = "Stop"

$eigenRepo = "https://gitlab.com/libeigen/eigen.git"
$eigenDir = "eigen_src"
$includeDir = "include"

# Clean up any previous run
if (Test-Path $eigenDir) { Remove-Item -Recurse -Force $eigenDir }
if (Test-Path $includeDir) { Remove-Item -Recurse -Force $includeDir }

# Clone Eigen
git clone $eigenRepo $eigenDir

# Copy Eigen headers
New-Item -ItemType Directory -Path $includeDir | Out-Null
Copy-Item -Recurse "$eigenDir\Eigen" "$includeDir\"

# Clean up source directory
Remove-Item -Recurse -Force $eigenDir

Write-Host "Eigen headers copied to 'external/eigen/include'"