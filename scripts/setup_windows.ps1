<# PowerShell script to install development tools on Windows using Chocolatey if available #>
param()

if (-not (Get-Command choco -ErrorAction SilentlyContinue)) {
    Write-Host "Chocolatey not found. Please install Visual Studio with C/C++ workload and CMake, or install Chocolatey and rerun this script." -ForegroundColor Yellow
    exit 1
}

choco install -y visualstudio2019buildtools cmake libxml2
Write-Host "Windows dependencies installed (via Chocolatey). Make sure to open x64 Native Tools Command Prompt for builds." -ForegroundColor Green
