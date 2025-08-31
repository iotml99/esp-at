# Quick Range Download Test Commands

## Setup
```bash
AT+BNSD_MOUNT
AT+BNSD_SPACE?
```

## Test 1: Small Range Download to File (HTTPBin)
```bash
# Download first 1KB of a 4KB test file
AT+BNCURL="GET","http://httpbin.org/range/4096","-dd","@test_range.bin","-r","0-1023"

# Download second 1KB (should append)
AT+BNCURL="GET","http://httpbin.org/range/4096","-dd","@test_range.bin","-r","1024-2047"

# Download third 1KB (should append)
AT+BNCURL="GET","http://httpbin.org/range/4096","-dd","@test_range.bin","-r","2048-3071"

# Download final 1KB (should append)
AT+BNCURL="GET","http://httpbin.org/range/4096","-dd","@test_range.bin","-r","3072-4095"

# Check final file size (should be 4096 bytes)
AT+BNSD_SPACE?
```

## Test 2: Range Download to UART (No File)
```bash
# Download first 1KB directly to UART
AT+BNCURL="GET","http://httpbin.org/range/4096","-r","0-1023"

# Download second 1KB to UART  
AT+BNCURL="GET","http://httpbin.org/range/4096","-r","1024-2047"

# Download final portion to UART
AT+BNCURL="GET","http://httpbin.org/range/4096","-r","2048-4095"
```

## Test 3: Range Download with Verbose Output
```bash
AT+BNCURL="GET","http://httpbin.org/range/8192","-dd","@test_verbose.bin","-r","0-2047","-v"
AT+BNCURL="GET","http://httpbin.org/range/8192","-r","0-2047","-v"
```

## Test 4: Invalid Range Tests
```bash
# Should fail: invalid range format
AT+BNCURL="GET","http://httpbin.org/range/4096","-dd","@test.bin","-r","invalid"

# Should fail: end < start
AT+BNCURL="GET","http://httpbin.org/range/4096","-dd","@test.bin","-r","1000-500"

# Should fail: invalid range format (UART)
AT+BNCURL="GET","http://httpbin.org/range/4096","-r","invalid"
```

## Expected Output Format

### Range Download to File:
```
+RANGE_INFO:existing_size=0
+LEN:1024,
+POST:512,<data>
+POST:512,<data>
+RANGE_FINAL:file_size=1024
SEND OK
```

### Range Download to UART:
```
+RANGE_INFO:uart_stream=true
+LEN:1024,
+POST:512,<data>
+POST:512,<data>
+RANGE_FINAL:uart_bytes=1024
SEND OK
```

### Subsequent Range to File:
```
+RANGE_INFO:existing_size=1024
+LEN:1024,
+POST:512,<data>
+POST:512,<data>
+RANGE_FINAL:file_size=2048
SEND OK
```
