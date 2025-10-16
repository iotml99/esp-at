# ESP-IDF Environment Setup Script
# This script sets up the ESP-IDF environment for ESP-AT development
# Run this script in every new PowerShell session before building

# Get the current directory (should be the esp-at project root)
$ProjectRoot = $PWD

# Set IDF_TOOLS_PATH to use local .idf_tools directory
$Env:IDF_TOOLS_PATH = "$ProjectRoot\.idf_tools"

Write-Host "Setting IDF_TOOLS_PATH to: $Env:IDF_TOOLS_PATH" -ForegroundColor Green

# Check if esp-idf directory exists
if (Test-Path "$ProjectRoot\esp-idf\export.ps1") {
    Write-Host "Activating ESP-IDF environment..." -ForegroundColor Yellow
    
    # Run the ESP-IDF export script
    & "$ProjectRoot\esp-idf\export.ps1"
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "ESP-IDF environment activated successfully!" -ForegroundColor Green
        Write-Host "You can now run: python build.py build" -ForegroundColor Cyan
    } else {
        Write-Host "Failed to activate ESP-IDF environment. Exit code: $LASTEXITCODE" -ForegroundColor Red
        Write-Host "Make sure ESP-IDF is properly installed by running: .\esp-idf\install.ps1" -ForegroundColor Yellow
    }
} else {
    Write-Host "Error: esp-idf\export.ps1 not found!" -ForegroundColor Red
    Write-Host "Make sure you're in the esp-at project directory and ESP-IDF is initialized." -ForegroundColor Yellow
    Write-Host "Run the following commands to set up ESP-IDF:" -ForegroundColor Yellow
    Write-Host "  git submodule update --init --recursive" -ForegroundColor White
    Write-Host "  .\esp-idf\install.ps1" -ForegroundColor White
}