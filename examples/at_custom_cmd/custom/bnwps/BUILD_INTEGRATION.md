# Build Integration Instructions for BNWPS

This document describes how to integrate BNWPS into the ESP-AT build system.

## Step 1: Update Main Custom Command CMakeLists.txt

Add the following to `examples/at_custom_cmd/custom/CMakeLists.txt` (or create if it doesn't exist):

```cmake
# Include BNWPS component
include(bnwps/CMakeLists.txt)

# Set component sources
set(COMPONENT_SRCS 
    "at_custom_cmd.c"
    "sd_card.c"
    ${BNWPS_SRCS}
)

# Set component includes
set(COMPONENT_ADD_INCLUDEDIRS 
    "."
    ${BNWPS_INCLUDES}
)

# Register component
idf_component_register(
    SRCS ${COMPONENT_SRCS}
    INCLUDE_DIRS ${COMPONENT_ADD_INCLUDEDIRS}
    REQUIRES esp_wifi esp_wps esp_event esp_netif freertos esp_at
)
```

## Step 2: Update Project CMakeLists.txt

Ensure the custom component is included in the main project `CMakeLists.txt`:

```cmake
# Add custom component path
set(EXTRA_COMPONENT_DIRS "examples/at_custom_cmd/custom")

# Include ESP-IDF build system
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp-at)
```

## Step 3: Update Kconfig

Add BNWPS configuration to the main Kconfig file. Add this line to `main/Kconfig` or your main Kconfig file:

```kconfig
rsource "examples/at_custom_cmd/custom/bnwps/Kconfig"
```

## Step 4: Update sdkconfig.defaults

Add default BNWPS configuration to your `sdkconfig.defaults`:

```
# BNWPS Configuration
CONFIG_BNWPS_ENABLE=y
CONFIG_BNWPS_MAX_DURATION=120
CONFIG_BNWPS_ALLOW_RECONNECT=n

# Ensure required ESP-IDF components are enabled
CONFIG_ESP_WIFI_ENABLED=y
CONFIG_ESP_WPS_ENABLED=y
CONFIG_ESP_EVENT_ENABLED=y
```

## Step 5: Update Module Configuration

For specific ESP32 variants, update the module configuration files in `module_config/`:

For example, in `module_config/module_esp32c6/sdkconfig.defaults`:

```
# Enable BNWPS for this module
CONFIG_BNWPS_ENABLE=y
CONFIG_BNWPS_MAX_DURATION=120
CONFIG_BNWPS_ALLOW_RECONNECT=n
```

## Step 6: Build Command

Build with the standard ESP-IDF build system:

```bash
# Configure the project
idf.py menuconfig

# Build the project
idf.py build

# Flash to device
idf.py -p /dev/ttyUSB0 flash monitor
```

## Step 7: Verification

After flashing, test the BNWPS commands:

```
AT+BNWPS=?
AT+BNWPS?
AT+BNWPS=60
```

## Alternative Integration: Single File Approach

If you prefer a single-file integration, you can combine all BNWPS code into the main `at_custom_cmd.c` file:

1. Copy the contents of `bnwps_sm.c` and `at_cmd_bnwps.c` into `at_custom_cmd.c`
2. Include the header definitions inline
3. Add the BNWPS command entries to the command table
4. Add Kconfig entries to the main configuration

## Troubleshooting Build Issues

### Missing ESP-WPS Support

If you get build errors about missing WPS functions:

1. Ensure `CONFIG_ESP_WPS_ENABLED=y` in sdkconfig
2. Check that your ESP-IDF version supports WPS (IDF 4.4+)
3. Verify the target chip supports WPS (ESP32, ESP32-S2, ESP32-S3, ESP32-C3)

### AT Framework Issues

If AT command registration fails:

1. Verify ESP-AT core libraries are properly linked
2. Check that custom command array size is correct
3. Ensure proper ESP_AT_CMD_SET_INIT_FN usage

### Memory Issues

If you encounter memory allocation failures:

1. Increase task stack sizes in menuconfig
2. Adjust heap size configuration
3. Consider reducing BNWPS_UART_CHUNK size if needed

### Linker Issues

If you get undefined symbol errors:

1. Verify all required components are in the REQUIRES list
2. Check that ESP-AT libraries include the needed symbols
3. Ensure proper component dependencies in CMakeLists.txt

## Testing the Build

After successful build and flash:

1. Connect to the ESP32 via serial terminal
2. Test basic AT communication: `AT`
3. Check BNWPS availability: `AT+BNWPS=?`
4. Run the integration test script: `python test_bnwps_integration.py --port /dev/ttyUSB0`

## Performance Considerations

- BNWPS uses approximately 4KB of stack space for the main task
- Mutex and event group usage is minimal (<1KB heap)
- Timer overhead is minimal (one-shot timer only during active sessions)
- Total RAM usage: ~6-8KB during active WPS sessions

## Security Considerations

- WPS PBC is inherently less secure than WPA2/WPA3 passwords
- Sessions are time-limited to prevent long-term exposure
- No credential persistence beyond standard ESP-IDF Wi-Fi storage
- Proper state machine prevents multiple simultaneous sessions
