# ESP-AT Build Guide

This document provides step-by-step instructions for building the ESP-AT firmware for ESP32C6 modules on Windows.

## Prerequisites

- Windows 10/11 with PowerShell
- Git for Windows
- Python 3.11.x (automatically managed by ESP-IDF)
- Visual Studio Code (optional, for development)

## Environment Setup

### 1. Clone the Repository

```powershell
git clone https://github.com/iotml99/esp-at.git
cd esp-at
```

### 2. Automated Setup with esp-at-setup.ps1

The ESP-AT project includes an automated setup script that handles both environment setup and dependency installation:

#### First-Time Setup (Required)
```powershell
.\esp-at-setup.ps1 -i
```

**Use the `-i` option for initial setup only.** This single command will:
- Set `IDF_TOOLS_PATH` to use local `.idf_tools` directory
- Install `xlrd` Python package (required for Excel file processing)
- Apply MQTT client patches (removes duplicate function definitions)
- Run `python build.py install` to install ESP-AT dependencies
- Activate ESP-IDF environment with `.\esp-idf\export.ps1`
- Install all ESP-IDF compilation tools (gcc, cmake, ninja, etc.)
- Set up Python virtual environment with required packages
- Install target-specific tools for ESP32, ESP32-C2, ESP32-C3, ESP32-C5, ESP32-C6, and ESP32-S2

#### Daily Development (Environment Activation Only)
```powershell
.\esp-at-setup.ps1
```

**For daily development, use the script without `-i` parameter.** This will only activate the ESP-IDF environment without reinstalling dependencies.

### 3. Daily Development Workflow

**Important**: You must activate the ESP-IDF environment in every new PowerShell session before building:

```powershell
.\esp-at-setup.ps1  # Environment activation (no -i parameter for daily use)
```

**Note**: Do NOT use the `-i` parameter for daily development - it's only needed for first-time setup.

This sets up:
- `IDF_PATH` environment variable
- `IDF_TOOLS_PATH` pointing to local `.idf_tools` directory
- Python virtual environment activation
- Tool paths in `PATH`
- ESP-IDF specific environment variables

## Building the Firmware

### 1. Build Command

Use the ESP-AT build script to compile the firmware:

```powershell
python build.py build
```

The build script will automatically:
- Detect platform (ESP32C6) and module (ESP32C6-4MB)
- Set required environment variables (`ESP_AT_PROJECT_PLATFORM`, `ESP_AT_MODULE_NAME`)
- Include custom AT command components
- Apply necessary patches
- Invoke `idf.py build` with correct parameters

### 2. Build Output

Upon successful completion, you'll find these files in the `build` directory:

#### Core Files:
- `bootloader/bootloader.bin` - Second-stage bootloader
- `esp-at.bin` - Main ESP-AT application
- `partition_table/partition-table.bin` - Partition layout
- `at_customize.bin` - AT command customizations
- `customized_partitions/mfg_nvs.bin` - Manufacturing data

#### Factory Images:
- `factory/factory_ESP32C6-4MB.bin` - Complete 4MB factory image (ready to flash)
- `factory/factory_ESP32C6-4MB_unfilled.bin` - Optimized factory image

### 3. Build Configuration

The build uses configuration from:
- `module_config/module_esp32c6_default/sdkconfig_silence.defaults`
- Generated `sdkconfig` file in project root

## Flashing the Firmware

Use the build.py script for flashing (Recommended):

```powershell
# Build the firmware
python build.py build

# Flash and monitor with merged binary
python build.py flash monitor -p COM3 merge-bin  # Replace COM3 with your port
```

## Troubleshooting

### Common Issues and Solutions

#### 1. "No module named 'esp_idf_monitor'" Error

**Problem**: ESP-IDF environment not properly activated.

**Solution**:
```powershell
.\esp-idf\export.ps1
```

#### 2. "idf.py build failed" with CMake Errors

**Problem**: Environment variables not set correctly.

**Solution**: Ensure environment is properly activated and use the ESP-AT build script:
```powershell
.\esp-at-setup.ps1  # Activate environment (no -i parameter)
python build.py build  # Then build
```

#### 3. MQTT Compilation Errors (Function Redefinition)

**Problem**: Outdated patches causing duplicate function definitions in mqtt_client.c.

**Solution**: Use the automated setup script which handles this automatically:
```powershell
.\esp-at-setup.ps1 -i
```

The script automatically removes duplicate function definitions (lines 1593-1617) from `esp-idf/components/mqtt/esp-mqtt/mqtt_client.c`.

**Manual Fix** (if needed):
1. Check `patches/mqtt.patch`
2. Remove duplicate `mqtt_resend_pubrel` function from mqtt_client.c (lines 1593-1617)
3. Ensure no other duplicate function definitions exist

#### 4. Python Environment Issues

**Problem**: Corrupted Python virtual environment.

**Solution**: Reinstall ESP-IDF tools:
```powershell
.\esp-idf\install.ps1
.\esp-idf\export.ps1
```

## Build Customization

### Target Configuration

The build automatically detects the target configuration:
- **Platform**: ESP32C6
- **Module**: ESP32C6-4MB (4MB flash, default configuration)

To build for different configurations, modify the detection logic in `build.py` or set environment variables manually.

### Custom Components

The build automatically includes:
- `examples/at_bones` - AT command framework extensions
- `examples/at_custom_cmd` - Custom AT command implementations

### Patch System

The project applies several patches during build:
- `blufi-adv.patch` - BluFi advertisement improvements
- `mqtt.patch` - MQTT QoS 2 PUBREL retransmission fix
- Additional patches in `patches/` directory

## Build Performance

### Typical Build Times
- Clean build: ~5-10 minutes (depending on hardware)
- Incremental build: ~30 seconds - 2 minutes

### Build Size Information
- **Bootloader**: ~21KB (36% free space in bootloader partition)
- **Application**: ~2.2MB (32% free space in app partition)
- **Total Flash Usage**: ~2.3MB out of 4MB

## Development Workflow

### 1. Initial Setup (One-time)
```powershell
git clone https://github.com/iotml99/esp-at.git
cd esp-at
$Env:IDF_TOOLS_PATH = "$PWD\.idf_tools"

# First-time setup with dependencies (use -i only once)
.\esp-at-setup.ps1 -i
```

### 2. Daily Development
```powershell
# Start new session - activate environment (NO -i parameter)
.\esp-at-setup.ps1

# Make changes to code
# ...

# Build firmware
python build.py build

# Flash and monitor with merged binary
python build.py flash monitor -p COM3 merge-bin  # Replace COM3 with your port
```

### 3. Clean Build (when needed)
```powershell
python build.py clean
python build.py build
```

## Additional Resources

- [ESP-AT Documentation](https://docs.espressif.com/projects/esp-at/en/latest/)
- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/)
- [ESP32-C6 Technical Reference](https://www.espressif.com/sites/default/files/documentation/esp32-c6_technical_reference_manual_en.pdf)

## Version Information

- **ESP-IDF Version**: v5.5.1
- **ESP-AT Version**: v4.2.0.0-dev
- **Supported Targets**: ESP32, ESP32-C2, ESP32-C3, ESP32-C5, ESP32-C6, ESP32-S2
- **Build Date**: October 16, 2025