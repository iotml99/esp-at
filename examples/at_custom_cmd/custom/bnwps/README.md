# BNWPS - Custom AT+BNWPS Command Implementation

## Overview

BNWPS implements a custom AT command group `AT+BNWPS` for WPS (Wi-Fi Protected Setup) control on ESP32 devices running ESP-AT firmware. This implementation provides thread-safe, non-blocking WPS functionality with proper timeout handling and status reporting.

## Features

- **Start WPS PBC**: `AT+BNWPS=<t>` - Start WPS Push Button Configuration for t seconds (1-120)
- **Query WPS State**: `AT+BNWPS?` - Check if WPS session is active (1=active, 0=idle)
- **Cancel WPS**: `AT+BNWPS=0` - Cancel any active WPS session
- **Thread-safe**: Non-blocking implementation using FreeRTOS tasks and events
- **Comprehensive error handling**: Detailed error codes for all failure scenarios
- **Configurable**: Build-time configuration options via Kconfig

## Command Syntax

### Start WPS Session
```
AT+BNWPS=<t>
```
- `<t>`: Duration in seconds (1-120, configurable maximum)
- **Response on accept**: `+BNWPS:1` followed by `OK`
- **Response on success**: `+CWJAP:"<ssid>","<bssid>",<ch>,<rssi>,<pci>,<reconn>,<listen>,<scan>,<pmf>` followed by `OK`
- **Response on failure**: `+CWJAP:<error_code>` followed by `ERROR`

### Query WPS State
```
AT+BNWPS?
```
- **Response**: `+BNWPS:1` (active) or `+BNWPS:0` (idle) followed by `OK`

### Cancel WPS Session
```
AT+BNWPS=0
```
- **Response**: `+BNWPS:0` followed by `OK`

## Error Codes

The following error codes are returned as `+CWJAP:<code>` on failure:

| Code | Description |
|------|-------------|
| 1 | General failure |
| 2 | Timeout |
| 3 | WPS failed (protocol error) |
| 4 | Invalid arguments |
| 5 | Not initialized / Wi-Fi off |
| 6 | Busy / operation in progress |
| 7 | Canceled by user |
| 8 | Authentication failed |
| 9 | Feature not supported |

## Example Sessions

### Successful WPS Connection
```
AT+BNWPS=60
+BNWPS:1
OK
... (user pushes WPS button on router)
+CWJAP:"MyWiFi","aa:bb:cc:dd:ee:ff",6,-51,1,0,0,0,1
OK
```

### WPS Timeout
```
AT+BNWPS=10
+BNWPS:1
OK
... (no WPS button pressed)
+CWJAP:2
ERROR
```

### Query WPS State
```
AT+BNWPS?
+BNWPS:0
OK
```

### Cancel Active WPS
```
AT+BNWPS=30
+BNWPS:1
OK
AT+BNWPS=0
+BNWPS:0
OK
```

### Busy Error (WPS already active)
```
AT+BNWPS=60
+BNWPS:1
OK
AT+BNWPS=30
+CWJAP:6
ERROR
```

## Configuration Options

### Kconfig Settings

Enable in `menuconfig` or `sdkconfig`:

```kconfig
CONFIG_BNWPS_ENABLE=y                    # Enable BNWPS commands
CONFIG_BNWPS_MAX_DURATION=120            # Maximum session duration (seconds)
CONFIG_BNWPS_ALLOW_RECONNECT=n           # Allow WPS when already connected
```

### Build Configuration

1. **Enable BNWPS**: Set `CONFIG_BNWPS_ENABLE=y`
2. **Set max duration**: Adjust `CONFIG_BNWPS_MAX_DURATION` (default: 120 seconds)
3. **Reconnect policy**: Set `CONFIG_BNWPS_ALLOW_RECONNECT=y` to allow disconnecting from current AP for WPS

## Implementation Architecture

### Components

1. **at_cmd_bnwps.c/h**: AT command parser and handlers
2. **bnwps_sm.c/h**: WPS state machine and event handling
3. **Kconfig**: Build-time configuration options

### State Machine

```
IDLE → ACTIVE (timer running) → CONNECTED (success) → IDLE
  ↑                          → FAILED (error)    → IDLE
  ↑                          → CANCELED (user)   → IDLE
  └────────────────────────────────────────────────┘
```

### Thread Safety

- **Mutex protection**: All state changes protected by mutex
- **Event-driven**: Uses FreeRTOS event groups for inter-task communication
- **Non-blocking**: AT command handlers return immediately, events processed asynchronously

### Error Handling

- **Comprehensive mapping**: ESP-IDF WiFi/WPS errors mapped to AT error codes
- **Timeout protection**: One-shot timer ensures sessions don't hang indefinitely
- **State validation**: All state transitions validated for consistency

## Integration

### File Structure
```
examples/at_custom_cmd/custom/bnwps/
├── at_cmd_bnwps.h              # AT command interface
├── at_cmd_bnwps.c              # AT command implementation
├── bnwps_sm.h                  # State machine interface
├── bnwps_sm.c                  # State machine implementation
├── Kconfig                     # Configuration options
├── CMakeLists.txt              # Build configuration
└── README.md                   # This file
```

### Adding to Custom Commands

The BNWPS commands are integrated into the main custom command table in `at_custom_cmd.c`:

```c
#ifdef CONFIG_BNWPS_ENABLE
#include "bnwps/at_cmd_bnwps.h"
#endif

static const esp_at_cmd_struct at_custom_cmd[] = {
    // ... other commands ...
#ifdef CONFIG_BNWPS_ENABLE
    {"+BNWPS", at_bnwps_cmd_test, at_bnwps_cmd_query, at_bnwps_cmd_setup, NULL},
#endif
};
```

## Dependencies

- **ESP-IDF**: Version 5.x with Wi-Fi and WPS support
- **ESP-AT**: Custom AT framework
- **FreeRTOS**: For tasks, timers, and synchronization
- **Components**: `esp_wifi`, `esp_wps`, `esp_event`, `esp_netif`

## Building

1. Ensure ESP-IDF and ESP-AT are properly configured
2. Enable `CONFIG_BNWPS_ENABLE=y` in your sdkconfig
3. Build normally with `idf.py build` or your build system

## Testing

### Unit Tests

Create test cases for:
- Valid parameter ranges
- Error conditions (invalid args, busy state)
- State transitions
- Timeout handling
- Cancel functionality

### Integration Tests

Test with actual hardware:
- WPS button on router
- Multiple rapid commands
- Long-running sessions
- Network switching scenarios

### Example Test Sequence

```bash
# Start WPS session
AT+BNWPS=60
# Expect: +BNWPS:1, OK

# Query state during session
AT+BNWPS?
# Expect: +BNWPS:1, OK

# Try to start another (should fail)
AT+BNWPS=30
# Expect: +CWJAP:6, ERROR

# Cancel session
AT+BNWPS=0
# Expect: +BNWPS:0, OK

# Query state after cancel
AT+BNWPS?
# Expect: +BNWPS:0, OK
```

## Known Limitations

1. **Single session**: Only one WPS session active at a time
2. **PBC only**: Currently supports Push Button Configuration only (not PIN)
3. **IPv4 only**: Designed for IPv4 networks (standard for WPS)
4. **No persistence**: WPS credentials follow standard ESP-IDF Wi-Fi storage behavior

## Troubleshooting

### Common Issues

1. **`+CWJAP:5` (Not initialized)**
   - Ensure Wi-Fi is initialized and started
   - Check ESP-IDF Wi-Fi configuration

2. **`+CWJAP:6` (Busy)**
   - Another WPS session is active
   - Use `AT+BNWPS=0` to cancel first

3. **`+CWJAP:2` (Timeout)**
   - WPS button not pressed on router within timeout
   - Router doesn't support WPS PBC
   - Increase timeout duration

4. **`+CWJAP:3` (WPS failed)**
   - WPS protocol error
   - Router configuration issue
   - Try resetting router WPS

### Debug Logs

Enable debug logging:
```c
esp_log_level_set("BNWPS", ESP_LOG_DEBUG);
esp_log_level_set("BNWPS_SM", ESP_LOG_DEBUG);
```

## Contributing

When modifying BNWPS:

1. **Maintain thread safety**: All state access must be mutex-protected
2. **Follow error code mapping**: Use consistent `+CWJAP:<code>` format
3. **Update tests**: Add test cases for new functionality
4. **Document changes**: Update this README for new features

## License

SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
SPDX-License-Identifier: Apache-2.0
