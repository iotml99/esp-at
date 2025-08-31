# BNWPS Testing Guide

## WPS Command Implementation Summary

The BNWPS library implements WiFi Protected Setup (WPS) functionality for the ESP32 AT command framework with the following features:

### WPS Command Overview
- `AT+BNWPS=<t>` : Start WPS operation for t seconds (1-300), or cancel with t=0
- `AT+BNWPS?` : Query current WPS status (0=idle, 1=active)
- `AT+BNWPS=0` : Cancel ongoing WPS operation

### WPS Features
1. **Push Button Configuration (PBC)**: Uses WPS_TYPE_PBC mode for router button-based setup
2. **Timeout Management**: Configurable timeout from 1-300 seconds with automatic cleanup
3. **Status Tracking**: Real-time status reporting (idle/active/success/failed/timeout)
4. **AT+CWJAP Response Format**: Uses standard ESP-AT WiFi join response format
5. **Error Handling**: Comprehensive error reporting with appropriate error codes

## Implementation Details

### Core WPS Flow:
1. **Initialization**: WPS subsystem initialized with WiFi event handlers
2. **Start Operation**: `esp_wifi_wps_enable()` → `esp_wifi_wps_start()` with timeout
3. **Router Interaction**: ESP32 waits for router WPS button press
4. **Credential Exchange**: WPS credentials received via WiFi events
5. **Connection**: Automatic WiFi connection with received credentials
6. **Response**: Standard AT+CWJAP format response sent to host

### Status Management:
- **BNWPS_STATUS_IDLE**: No WPS operation active
- **BNWPS_STATUS_ACTIVE**: WPS waiting for router connection
- **BNWPS_STATUS_SUCCESS**: WPS completed successfully, WiFi connected
- **BNWPS_STATUS_FAILED**: WPS operation failed
- **BNWPS_STATUS_TIMEOUT**: WPS operation timed out

## Test Examples

### Test 1: Basic WPS Operation
```bash
# Check initial status (should be 0 = idle)
AT+BNWPS?

# Start WPS for 60 seconds
AT+BNWPS=60

# Check status during operation (should be 1 = active)
AT+BNWPS?

# Press WPS button on router within 60 seconds
# Expected successful response:
# +CWJAP:"MyNetwork","aa:bb:cc:dd:ee:ff",6,-45,0,0,0,0,0
# OK

# Check final status (should be 0 = idle)
AT+BNWPS?
```

### Test 2: WPS Cancellation
```bash
# Start WPS operation
AT+BNWPS=120

# Check that it's active
AT+BNWPS?
# Expected: +BNWPS:1

# Cancel the operation
AT+BNWPS=0
# Expected: +BNWPS:0
# OK

# Verify it's cancelled
AT+BNWPS?
# Expected: +BNWPS:0
```

### Test 3: WPS Timeout
```bash
# Start WPS with short timeout
AT+BNWPS=10

# Don't press router button, let it timeout
# Expected after 10 seconds:
# +CWJAP:2
# ERROR

# Check status after timeout
AT+BNWPS?
# Expected: +BNWPS:0
```

### Test 4: Parameter Validation
```bash
# Test invalid timeout values
AT+BNWPS=0
# Expected: +BNWPS:0, OK (cancel is valid)

AT+BNWPS=301
# Expected: ERROR (exceeds maximum)

AT+BNWPS=-1
# Expected: ERROR (negative value)

AT+BNWPS=abc
# Expected: ERROR (non-numeric)
```

### Test 5: WPS Help and Information
```bash
# Get command help
AT+BNWPS=?

# Expected comprehensive help output showing:
# - Command syntax
# - Parameter ranges
# - Usage examples
# - Response formats
```

## Expected Response Formats

### Successful WPS Connection:
```
AT+BNWPS=60
OK

(After WPS button press and successful connection)
+CWJAP:"MyNetwork","aa:bb:cc:dd:ee:ff",6,-45,0,0,0,0,0
OK
```

### WPS Failure (Connection Failed):
```
AT+BNWPS=60
OK

(After WPS failure)
+CWJAP:1
ERROR
```

### WPS Timeout:
```
AT+BNWPS=10
OK

(After 10 seconds without router response)
+CWJAP:2
ERROR
```

### Status Query Responses:
```
AT+BNWPS?
+BNWPS:0    (idle)
OK

AT+BNWPS?
+BNWPS:1    (active)
OK
```

### Cancel Operation:
```
AT+BNWPS=0
+BNWPS:0
OK
```

## Response Field Meanings

### AT+CWJAP Response Fields:
1. **ssid**: Network name in quotes
2. **bssid**: Router MAC address in "xx:xx:xx:xx:xx:xx" format
3. **channel**: WiFi channel number (1-14)
4. **rssi**: Signal strength in dBm (negative value)
5. **pci_en**: PCI enable status (typically 0)
6. **reconn_interval**: Reconnection interval (typically 0)
7. **listen_interval**: Listen interval (typically 0) 
8. **scan_mode**: Scan mode (typically 0)
9. **pmf**: Protected Management Frame status (typically 0)

### Error Codes:
- **1**: Connection failed
- **2**: Timeout occurred
- **3**: Wrong password (not applicable for WPS)
- **4**: AP not found (not applicable for WPS)

## Hardware Requirements

### Router Requirements:
- WPS-enabled WiFi router
- WPS push button functionality
- Compatible WPS implementation (most modern routers)

### ESP32 Requirements:
- WiFi capability enabled
- Sufficient memory for WPS operation
- Event loop and timer functionality

## Troubleshooting

### Common Issues:

#### WPS Not Starting:
```
AT+BNWPS=60
ERROR

Possible causes:
- WiFi not initialized
- WPS subsystem initialization failed
- Invalid parameter
```

#### WPS Timeout:
```
+CWJAP:2
ERROR

Possible causes:
- Router WPS button not pressed
- Router WPS disabled
- WiFi interference
- Router too far away
```

#### Connection Failed After WPS:
```
+CWJAP:1
ERROR

Possible causes:
- Router WPS succeeded but connection failed
- Network authentication issues
- Router configuration problems
```

### Debug Steps:
1. **Check WiFi Status**: Ensure WiFi subsystem is operational
2. **Router Verification**: Confirm router WPS is enabled and functional
3. **Signal Strength**: Ensure router is within range
4. **Timing**: Press WPS button on router within timeout period
5. **Multiple Attempts**: Try WPS operation multiple times if initial attempts fail

## Performance Considerations

### Timing:
- **WPS Start Time**: < 1 second to enter WPS mode
- **Button Press Window**: Must press router button within timeout period
- **Connection Time**: 5-30 seconds after button press (depends on router)
- **Cleanup Time**: < 1 second for operation cleanup

### Memory Usage:
- **Static Memory**: ~2KB for WPS context and buffers
- **Dynamic Memory**: ~1KB during active WPS operation
- **Event Handlers**: Minimal overhead for WiFi event processing

### Power Consumption:
- **Active WPS**: Higher power due to continuous WiFi scanning
- **Idle State**: Normal low-power WiFi operation
- **Timeout Management**: Automatic cleanup reduces power consumption

## Integration Notes

### Event System:
- Uses ESP-IDF WiFi event system for WPS state management
- Integrates with existing AT command event handling
- Timer-based timeout and countdown management

### Thread Safety:
- All WPS operations are thread-safe
- Uses FreeRTOS timers for timeout management
- Proper synchronization with WiFi event system

### Error Recovery:
- Automatic cleanup on timeout or failure
- Graceful handling of WiFi disconnection
- Proper resource deallocation

---

*Last Updated: August 31, 2025*
*WPS Implementation Version: 1.0*
*Compatible with ESP-AT Framework*
