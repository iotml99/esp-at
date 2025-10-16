# ESP-AT Install Script
# This script sets up the ESP-IDF environment and installs ESP-AT dependencies
# 
# Usage:
#   .\esp-at-build.ps1           # Just setup environment
#   .\esp-at-build.ps1 -Install  # Setup environment and install dependencies
#   .\esp-at-build.ps1 -i        # Setup environment and install dependencies (short form)

param(
    [switch]$Install,
    [switch]$i
)

# Handle short form parameters
if ($i) { $Install = $true }

# Get the current directory (should be the esp-at project root)
$ProjectRoot = $PWD

# Set IDF_TOOLS_PATH to use local .idf_tools directory
$Env:IDF_TOOLS_PATH = "$ProjectRoot\.idf_tools"

Write-Host "ESP-AT Install Script" -ForegroundColor Cyan
Write-Host "=====================" -ForegroundColor Cyan
Write-Host ""

if ($Install) {
    Write-Host "Mode: Environment Setup + Install Dependencies" -ForegroundColor Yellow
} else {
    Write-Host "Mode: Environment Setup Only" -ForegroundColor Yellow
}

Write-Host "Setting IDF_TOOLS_PATH to: $Env:IDF_TOOLS_PATH" -ForegroundColor Green

# Check if esp-idf directory exists
if (Test-Path "$ProjectRoot\esp-idf\export.ps1") {
    Write-Host "Activating ESP-IDF environment..." -ForegroundColor Yellow
    
    # Run the ESP-IDF export script
    & "$ProjectRoot\esp-idf\export.ps1"
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "ESP-IDF environment activated successfully!" -ForegroundColor Green
        Write-Host "" # Empty line for better readability
        
        # Install dependencies if requested
        if ($Install) {
            Write-Host "Installing ESP-AT dependencies..." -ForegroundColor Cyan
            Write-Host "Running: python build.py install" -ForegroundColor White
            
            python build.py install
            
            if ($LASTEXITCODE -eq 0) {
                Write-Host "" # Empty line for better readability
                Write-Host "========================================" -ForegroundColor Green
                Write-Host "ESP-AT DEPENDENCIES INSTALLED SUCCESSFULLY!" -ForegroundColor Green
                Write-Host "========================================" -ForegroundColor Green
                Write-Host ""
                Write-Host "Environment is now ready for development." -ForegroundColor Cyan
                Write-Host ""
                Write-Host "To build the project, run:" -ForegroundColor Yellow
                Write-Host "  python build.py build" -ForegroundColor White
                Write-Host ""
                Write-Host "Installation completed at: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')" -ForegroundColor Gray
            } else {
                Write-Host "" # Empty line for better readability
                Write-Host "========================================" -ForegroundColor Red
                Write-Host "DEPENDENCY INSTALLATION FAILED!" -ForegroundColor Red
                Write-Host "========================================" -ForegroundColor Red
                Write-Host "Installation failed with exit code: $LASTEXITCODE" -ForegroundColor Red
                Write-Host ""
                Write-Host "Common solutions:" -ForegroundColor Yellow
                Write-Host "  1. Check installation logs above for specific errors" -ForegroundColor White
                Write-Host "  2. Ensure internet connection is available" -ForegroundColor White
                Write-Host "  3. Try running .\esp-idf\install.ps1 manually" -ForegroundColor White
                Write-Host "  4. Check disk space and permissions" -ForegroundColor White
                exit $LASTEXITCODE
            }
        } else {
            Write-Host "" # Empty line for better readability
            Write-Host "========================================" -ForegroundColor Green
            Write-Host "ESP-IDF ENVIRONMENT ACTIVATED!" -ForegroundColor Green
            Write-Host "========================================" -ForegroundColor Green
            Write-Host ""
            Write-Host "Environment is ready for development." -ForegroundColor Cyan
            Write-Host ""
            Write-Host "Available commands:" -ForegroundColor Yellow
            Write-Host "  python build.py install  # Install dependencies" -ForegroundColor White
            Write-Host "  python build.py build    # Build the project" -ForegroundColor White
            Write-Host "  python build.py clean    # Clean build artifacts" -ForegroundColor White
            Write-Host "  idf.py build            # Direct ESP-IDF build" -ForegroundColor White
            Write-Host "  idf.py flash            # Flash firmware" -ForegroundColor White
            Write-Host ""
            Write-Host "Environment activated at: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')" -ForegroundColor Gray
        }
    } else {
        Write-Host "Failed to activate ESP-IDF environment. Exit code: $LASTEXITCODE" -ForegroundColor Red
        Write-Host "Make sure ESP-IDF is properly installed by running: .\esp-idf\install.ps1" -ForegroundColor Yellow
        exit $LASTEXITCODE
    }
} else {
    Write-Host "Error: esp-idf\export.ps1 not found!" -ForegroundColor Red
    Write-Host "Make sure you're in the esp-at project directory and ESP-IDF is initialized." -ForegroundColor Yellow
    Write-Host "Run the following commands to set up ESP-IDF:" -ForegroundColor Yellow
    Write-Host "  git submodule update --init --recursive" -ForegroundColor White
    Write-Host "  .\esp-idf\install.ps1" -ForegroundColor White
    exit 1
}