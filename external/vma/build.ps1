$ErrorActionPreference = "Stop"

$vmaRepo = "https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git"
$vmaDir = "vma_src"
$includeDir = "include"

# Clean up any previous run
if (Test-Path $vmaDir) { Remove-Item -Recurse -Force $vmaDir }
if (Test-Path $includeDir) { Remove-Item -Recurse -Force $includeDir }

# Clone VMA
git clone $vmaRepo $vmaDir

# Copy VMA header
New-Item -ItemType Directory -Path $includeDir | Out-Null
Copy-Item "$vmaDir\include\vk_mem_alloc.h" "$includeDir\"

# Clean up source directory
Remove-Item -Recurse -Force $vmaDir

Write-Host "Vulkan Memory Allocator header copied to 'external/vma/include'"