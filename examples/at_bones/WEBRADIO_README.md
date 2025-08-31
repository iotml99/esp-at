# Web Radio Streaming Commands

This module implements web radio streaming functionality for the ESP32 AT command framework.

## Commands

### AT+BNWEB_RADIO - Web Radio Control

#### Test Command
```
AT+BNWEB_RADIO=?
```
**Response:** `+BNWEB_RADIO:(0,1)`

Shows the supported parameter values for the web radio command.

#### Query Command
```
AT+BNWEB_RADIO?
```
**Response when inactive:** `+BNWEB_RADIO:0`
**Response when active:** `+BNWEB_RADIO:1,<bytes_streamed>,<duration_ms>`

- `bytes_streamed`: Total number of audio bytes streamed to UART
- `duration_ms`: Streaming duration in milliseconds

#### Setup Command

**Start streaming:**
```
AT+BNWEB_RADIO=1,"<url>"
```

**Stop streaming:**
```
AT+BNWEB_RADIO=0
```

### Parameters

- `enable`: 
  - `0` = Stop streaming
  - `1` = Start streaming
- `url`: Web radio stream URL (HTTP/HTTPS supported)

## Features

### Audio Format Support
- **MP3 streams** - Direct binary MP3 frame streaming
- **AAC streams** - Direct binary AAC frame streaming  
- **Other formats** - Any binary audio format supported by the stream

### Protocol Support
- **HTTP streams** - Standard web radio streams
- **HTTPS streams** - Secure streaming with certificate support
- **ICY Protocol** - Internet radio metadata protocol
- **Redirects** - Automatic redirect following (max 5 redirects)

### Streaming Characteristics
- **Continuous streaming** - Runs indefinitely until stopped
- **Real-time output** - Audio data streamed directly to UART as received
- **No buffering** - Pure pass-through streaming for minimal latency
- **No format markers** - Raw binary audio data only (no +LEN, +POST markers)

## Usage Examples

### Start BBC Radio Stream
```
AT+BNWEB_RADIO=1,"http://stream.live.vc.bbcmedia.co.uk/bbc_radio_one"
```

### Start Secure HTTPS Stream
```
AT+BNWEB_RADIO=1,"https://edge-bauerall-01-gos2.sharp-stream.com/net2national.mp3"
```

### Check Streaming Status
```
AT+BNWEB_RADIO?
+BNWEB_RADIO:1,1048576,30000
OK
```
*Shows: streaming active, 1MB streamed, 30 seconds duration*

### Stop Streaming
```
AT+BNWEB_RADIO=0
OK
```

## Technical Details

### UART Output
- **Pure binary data** - Audio frames output directly to UART
- **No framing** - No AT command markers or length indicators
- **Continuous flow** - Data flows as received from stream
- **Real-time** - Minimal buffering for live audio

### Memory Usage
- **8KB streaming buffer** - Optimized for ESP32 memory constraints
- **Minimal overhead** - Direct pass-through architecture
- **Dynamic allocation** - Memory allocated only during streaming

### Performance
- **Low latency** - Direct UART streaming without buffering
- **High throughput** - Up to UART baud rate limits
- **Concurrent operation** - Can run alongside other AT commands (queries)

### Error Handling
- **Graceful shutdown** - Proper cleanup on stop or error
- **Connection recovery** - Automatic retry on network issues
- **Certificate validation** - HTTPS streams use certificate manager

## Integration with Certificate Manager

HTTPS streams automatically integrate with the certificate manager:

1. **Partition certificates** - Uses custom certificates if available
2. **Hardcoded bundle** - Falls back to embedded CA bundle
3. **Permissive mode** - Final fallback for broad compatibility

## Thread Safety

- **Mutex protection** - All operations are thread-safe
- **Graceful termination** - Stop commands cleanly terminate streaming
- **Resource cleanup** - Automatic cleanup on task termination

## Limitations

- **Single stream** - Only one web radio stream at a time
- **No recording** - Audio data is not stored, only passed through
- **No metadata** - ICY metadata is filtered out (audio only)
- **Memory constraints** - Limited by ESP32 available memory

## Error Responses

- `ERROR` - Invalid parameters, network error, or initialization failure
- Check logs for detailed error information during development

## Development Notes

### Adding Custom Audio Processing
The `webradio_write_callback` function can be modified to add:
- Audio format detection
- Volume control
- Audio filtering
- Format conversion

### Performance Tuning
Adjust these parameters in `bnwebradio.c`:
- `CURLOPT_BUFFERSIZE` - Network buffer size (default: 8192)
- Task stack size - Currently 8KB, increase for complex processing
- Task priority - Currently 5, adjust based on system requirements
