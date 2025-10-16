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

### 2. Install ESP-IDF Tools

The ESP-AT project includes ESP-IDF as a submodule. Install the required tools:

```powershell
.\esp-idf\install.ps1
```

This will:
- Install all ESP-IDF compilation tools (gcc, cmake, ninja, etc.)
- Set up Python virtual environment with required packages
- Install target-specific tools for ESP32, ESP32-C2, ESP32-C3, ESP32-C5, ESP32-C6, and ESP32-S2

### 3. Activate ESP-IDF Environment

**Important**: You must run this command in every new PowerShell session before building:

```powershell
.\esp-idf\export.ps1
```

This sets up:
- `IDF_PATH` environment variable
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

### Option 1: Using idf.py
```powershell
idf.py flash
```

### Option 2: Using esptool directly
```powershell
python -m esptool --chip esp32c6 -b 460800 --before default_reset --after hard_reset write_flash --flash_mode dio --flash_size 4MB --flash_freq 80m 0x0 build\bootloader\bootloader.bin 0x8000 build\partition_table\partition-table.bin 0xe000 build\ota_data_initial.bin 0x1e000 build\at_customize.bin 0x1f000 build\customized_partitions\mfg_nvs.bin 0x40000 build\esp-at.bin
```

### Option 3: Flash factory image
```powershell
python -m esptool --chip esp32c6 write_flash 0x0 build\factory\factory_ESP32C6-4MB.bin
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

**Solution**: Use the build.py script instead of calling idf.py directly:
```powershell
python build.py build
```

#### 3. MQTT Compilation Errors (Function Redefinition)

**Problem**: Outdated patches causing duplicate function definitions.

**Solution**: This has been fixed in the current build. If you encounter similar issues:
1. Check `patches/mqtt.patch`
2. Ensure no duplicate function definitions in `esp-idf/components/mqtt/esp-mqtt/mqtt_client.c`

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

### 1. Setup (One-time)
```powershell
git clone https://github.com/iotml99/esp-at.git
cd esp-at
.\esp-idf\install.ps1
```

### 2. Daily Development
```powershell
# Start new session
.\esp-idf\export.ps1

# Make changes to code
# ...

# Build and test
python build.py build
idf.py flash monitor
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