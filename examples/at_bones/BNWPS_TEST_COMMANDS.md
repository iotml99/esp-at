# Quick WPS Test Commands

## Basic WPS Operation Test
```bash
# Check initial status
AT+BNWPS?

# Start WPS for 60 seconds
AT+BNWPS=60

# Press WPS button on router now!
# Expected successful response:
# +CWJAP:"RouterName","xx:xx:xx:xx:xx:xx",6,-45,0,0,0,0,0
# OK
```

## WPS Cancel Test
```bash
# Start WPS
AT+BNWPS=120

# Check it's active
AT+BNWPS?
# Expected: +BNWPS:1

# Cancel it
AT+BNWPS=0
# Expected: +BNWPS:0, OK
```

## WPS Timeout Test
```bash
# Start with short timeout
AT+BNWPS=5

# Don't press router button - let it timeout
# Expected after 5 seconds: +CWJAP:2, ERROR
```

## Parameter Validation Tests
```bash
# Valid cancel
AT+BNWPS=0

# Invalid - too large
AT+BNWPS=301

# Invalid - negative
AT+BNWPS=-1

# Get help
AT+BNWPS=?
```

## Expected Output Examples

### Successful WPS:
```
AT+BNWPS=60
OK
+CWJAP:"MyNetwork","aa:bb:cc:dd:ee:ff",6,-45,0,0,0,0,0
OK
```

### WPS Timeout:
```
AT+BNWPS=10
OK
+CWJAP:2
ERROR
```

### WPS Cancel:
```
AT+BNWPS=0
+BNWPS:0
OK
```

### Status Query:
```
AT+BNWPS?
+BNWPS:0
OK
```
