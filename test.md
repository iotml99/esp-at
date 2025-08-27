# ESP32 AT+BNCURL Test Cases

## 1. Basic Method Tests

### 1.1 Valid Methods
- `AT+BNCURL="GET","https://httpbin.org/get"`
- `AT+BNCURL="POST","https://httpbin.org/post"`
- `AT+BNCURL="HEAD","https://httpbin.org/get"`

### 1.2 Invalid Methods
- `AT+BNCURL="PUT","https://httpbin.org/put"` (should fail - only GET/POST/HEAD supported)
- `AT+BNCURL="DELETE","https://httpbin.org/delete"` (should fail)
- `AT+BNCURL="PATCH","https://httpbin.org/patch"` (should fail)
- `AT+BNCURL="","https://httpbin.org/get"` (empty method)
- `AT+BNCURL="get","https://httpbin.org/get"` (lowercase - should fail or auto-convert)

## 2. URL Tests

### 2.1 Valid URLs
- `AT+BNCURL="GET","http://httpbin.org/get"` (HTTP)
- `AT+BNCURL="GET","https://httpbin.org/get"` (HTTPS)
- `AT+BNCURL="GET","http://httpbin.org:80/get"` (explicit port)
- `AT+BNCURL="GET","https://httpbin.org:443/get"` (explicit HTTPS port)
- `AT+BNCURL="GET","http://example.com:8080/api/test"` (custom port)
- `AT+BNCURL="GET","https://api.openweathermap.org/data/2.5/weather?lat=42.567001&lon=1.598100&appid=test123&lang=en&units=metric"` (query parameters)

### 2.2 Invalid URLs
- `AT+BNCURL="GET",""` (empty URL)
- `AT+BNCURL="GET","ftp://example.com/file"` (unsupported protocol)
- `AT+BNCURL="GET","httpbin.org/get"` (missing protocol)
- `AT+BNCURL="GET","http://"` (incomplete URL)
- `AT+BNCURL="GET","https://toolongdomainname.verylongsubdomain.extremelylongdomainname.com/very/long/path/that/might/exceed/limits"`

## 3. Header Tests (-H)

### 3.1 Single Headers
- `AT+BNCURL="GET","https://httpbin.org/get","-H","Content-Type: application/json"`
- `AT+BNCURL="GET","https://httpbin.org/get","-H","User-Agent: ESP32-Client/1.0"`
- `AT+BNCURL="GET","https://httpbin.org/get","-H","Authorization: Bearer token123"`

### 3.2 Multiple Headers
- `AT+BNCURL="GET","https://httpbin.org/get","-H","Content-Type: application/json","-H","Accept: application/json"`
- `AT+BNCURL="GET","https://httpbin.org/get","-H","User-Agent: ESP32","-H","X-Custom-Header: test","-H","Accept-Language: en-US"`

### 3.3 Headers with Nested Quotes
- `AT+BNCURL="GET","https://httpbin.org/get","-H","SOAPAction: \"/getQuestions\""`
- `AT+BNCURL="GET","https://httpbin.org/get","-H","X-Custom: \"quoted value\""`
- `AT+BNCURL="GET","https://httpbin.org/get","-H","Authorization: Bearer \"token with spaces\""`

### 3.4 Invalid Headers
- `AT+BNCURL="GET","https://httpbin.org/get","-H",""` (empty header)
- `AT+BNCURL="GET","https://httpbin.org/get","-H","Invalid Header Format"` (missing colon)
- `AT+BNCURL="GET","https://httpbin.org/get","-H",": value"` (missing header name)

## 4. Data Upload Tests (-du)

### 4.1 Numeric Data Upload (POST only)
- `AT+BNCURL="POST","https://httpbin.org/post","-du","100"` (expect 100 bytes via UART)
- `AT+BNCURL="POST","https://httpbin.org/post","-du","1"` (single byte)
- `AT+BNCURL="POST","https://httpbin.org/post","-du","0"` (zero bytes)
- `AT+BNCURL="POST","https://httpbin.org/post","-du","65536"` (large payload)

### 4.2 File Upload
- `AT+BNCURL="POST","https://httpbin.org/post","-du","@/test/upload.txt"`
- `AT+BNCURL="POST","https://httpbin.org/post","-du","@/data/large_file.bin"`
- `AT+BNCURL="POST","https://httpbin.org/post","-du","@/nonexistent.txt"` (file not found)

### 4.3 Invalid -du Parameters
- `AT+BNCURL="GET","https://httpbin.org/get","-du","100"` (GET with data upload - should fail)
- `AT+BNCURL="HEAD","https://httpbin.org/get","-du","100"` (HEAD with data upload - should fail)
- `AT+BNCURL="POST","https://httpbin.org/post","-du",""` (empty parameter)
- `AT+BNCURL="POST","https://httpbin.org/post","-du","-100"` (negative number)
- `AT+BNCURL="POST","https://httpbin.org/post","-du","abc"` (non-numeric)

## 5. Data Download Tests (-dd)

### 5.1 Valid Download Destinations
- `AT+BNCURL="GET","https://httpbin.org/json","-dd","/download/test.json"`
- `AT+BNCURL="GET","https://httpbin.org/image/png","-dd","/images/test.png"`
- `AT+BNCURL="GET","https://httpbin.org/xml","-dd","@result.xml"` (@ prefix test)

### 5.2 Missing -dd (UART output)
- `AT+BNCURL="GET","https://httpbin.org/json"` (should output to UART with +LEN: format)
- `AT+BNCURL="POST","https://httpbin.org/post","-du","50"` (POST response to UART)

### 5.3 Invalid Download Paths
- `AT+BNCURL="GET","https://httpbin.org/get","-dd",""` (empty path)
- `AT+BNCURL="GET","https://httpbin.org/get","-dd","/readonly/path/file.txt"` (write-protected path)
- `AT+BNCURL="GET","https://httpbin.org/get","-dd","/very/deep/nonexistent/path/file.txt"` (path doesn't exist)

## 6. Cookie Tests

### 6.1 Save Cookies (-c)
- `AT+BNCURL="GET","https://httpbin.org/cookies/set/testcookie/testvalue","-c","session_cookies"`
- `AT+BNCURL="GET","https://httpbin.org/cookies/set/user/john","-c","user_session"`
- `AT+BNCURL="GET","https://httpbin.org/cookies/set/multi/value","-c",""` (unnamed cookie file)

### 6.2 Send Cookies (-b)
- `AT+BNCURL="GET","https://httpbin.org/cookies","-b","session_cookies"`
- `AT+BNCURL="GET","https://httpbin.org/cookies","-b","nonexistent_cookies"` (file not found)

### 6.3 Cookie Combination
- `AT+BNCURL="GET","https://httpbin.org/cookies/set/new/value","-c","cookies1","-b","cookies2"`

## 7. Range Tests (-r)

### 7.1 Valid Ranges (GET only)
- `AT+BNCURL="GET","https://httpbin.org/range/1024","-r","0-511"` (first half)
- `AT+BNCURL="GET","https://httpbin.org/range/1024","-r","512-1023"` (second half)
- `AT+BNCURL="GET","https://httpbin.org/range/1024","-r","100-200"` (middle range)
- `AT+BNCURL="GET","https://httpbin.org/range/1024","-r","0-0"` (single byte)

### 7.2 Invalid Ranges
- `AT+BNCURL="POST","https://httpbin.org/post","-r","0-100"` (range with POST - should fail)
- `AT+BNCURL="HEAD","https://httpbin.org/get","-r","0-100"` (range with HEAD - should fail)
- `AT+BNCURL="GET","https://httpbin.org/range/1024","-r","abc-def"` (non-numeric)
- `AT+BNCURL="GET","https://httpbin.org/range/1024","-r","500-100"` (invalid range - start > end)
- `AT+BNCURL="GET","https://httpbin.org/range/1024","-r","-100-200"` (negative start)

## 8. Verbose Mode Tests (-v)

### 8.1 Verbose Output
- `AT+BNCURL="GET","https://httpbin.org/get","-v"`
- `AT+BNCURL="POST","https://httpbin.org/post","-du","50","-v"`
- `AT+BNCURL="GET","https://httpbin.org/redirect/3","-v"` (test with redirects)

## 9. Complex Combination Tests

### 9.1 All Parameters Combined
```
AT+BNCURL="POST","https://httpbin.org/post","-H","Content-Type: application/xml","-H","Authorization: Bearer test123","-du","200","-dd","/results/response.xml","-c","session_cookies","-v"
```

### 9.2 Real-world SOAP Example (from spec)
```
AT+BNCURL="POST","https://blibu.dzb.de:8093/dibbsDaisy/services/dibbsDaisyPort/","-du","385","-H","Cookie: JSESSIONID=FC33F4334E8FE8074194BCCD9053C148","-H","Content-Type: text/xml;charset=UTF-8","-H","SOAPAction: \"/getQuestions\""
```

### 9.3 Range Download with Cookies
```
AT+BNCURL="GET","https://httpbin.org/range/2048","-r","1024-2047","-b","auth_cookies","-dd","/downloads/part2.bin"
```

## 10. Protocol and Security Tests

### 10.1 HTTP vs HTTPS Auto-detection
- `AT+BNCURL="GET","http://httpbin.org/get"` (should use TCP)
- `AT+BNCURL="GET","https://httpbin.org/get"` (should use SSL/TLS)

### 10.2 TLS Negotiation
- `AT+BNCURL="GET","https://tls-v1-0.badssl.com:1010/"` (old TLS)
- `AT+BNCURL="GET","https://tls-v1-2.badssl.com:1012/"` (TLS 1.2)
- `AT+BNCURL="GET","https://expired.badssl.com/"` (expired certificate)
- `AT+BNCURL="GET","https://self-signed.badssl.com/"` (self-signed certificate)

## 11. Redirect Tests

### 11.1 Automatic Redirects (301/302/303)
- `AT+BNCURL="GET","https://httpbin.org/redirect/1"`
- `AT+BNCURL="GET","https://httpbin.org/redirect/5"` (multiple redirects)
- `AT+BNCURL="GET","https://httpbin.org/redirect-to?url=https://httpbin.org/get"`

### 11.2 Redirects with Range Requests
- `AT+BNCURL="GET","https://httpbin.org/redirect-to?url=https://httpbin.org/range/1024","-r","0-511"`

## 12. Error Condition Tests

### 12.1 Network Errors
- `AT+BNCURL="GET","https://nonexistent.domain.com/"` (DNS failure)
- `AT+BNCURL="GET","https://httpbin.org:9999/"` (connection refused)
- `AT+BNCURL="GET","https://httpbin.org/delay/30"` (timeout test)

### 12.2 HTTP Error Codes
- `AT+BNCURL="GET","https://httpbin.org/status/404"` (404 Not Found)
- `AT+BNCURL="GET","https://httpbin.org/status/500"` (500 Server Error)
- `AT+BNCURL="GET","https://httpbin.org/status/403"` (403 Forbidden)

### 12.3 Malformed Commands
- `AT+BNCURL=` (incomplete command)
- `AT+BNCURL="GET"` (missing URL)
- `AT+BNCURL="GET","https://httpbin.org/get","-H"` (header flag without value)
- `AT+BNCURL="GET","https://httpbin.org/get","-du"` (data upload flag without value)
- `AT+BNCURL="GET","https://httpbin.org/get","-invalid","parameter"` (unknown flag)

## 13. Special Character and Encoding Tests

### 13.1 URLs with Special Characters
- `AT+BNCURL="GET","https://httpbin.org/get?param=test%20value"` (URL encoded spaces)
- `AT+BNCURL="GET","https://httpbin.org/get?unicode=測試"` (Unicode characters)
- `AT+BNCURL="GET","https://httpbin.org/get?special=!@#$%^&*()"` (special characters)

### 13.2 Headers with Special Characters
- `AT+BNCURL="GET","https://httpbin.org/get","-H","X-Test: value with spaces"`
- `AT+BNCURL="GET","https://httpbin.org/get","-H","X-Unicode: 測試值"`
- `AT+BNCURL="GET","https://httpbin.org/get","-H","X-Special: !@#$%^&*()"`

## 14. Large Data Tests

### 14.1 Large File Downloads
- `AT+BNCURL="GET","https://httpbin.org/drip?duration=1&numbytes=10000","-dd","/large/file.bin"`
- `AT+BNCURL="GET","https://httpbin.org/drip?duration=1&numbytes=100000"` (large UART output)

### 14.2 Large File Uploads
- `AT+BNCURL="POST","https://httpbin.org/post","-du","10000"` (10KB upload)
- `AT+BNCURL="POST","https://httpbin.org/post","-du","@/large_file.bin"` (large file upload)

### 14.3 Chunked Transfer
- Test chunked response handling with large responses
- Verify +POST:512 chunk formatting for UART output

## 15. Real-world API Tests

### 15.1 Weather API (from spec example)
```
AT+BNCURL="GET","https://api.openweathermap.org/data/2.5/weather?lat=42.567001&lon=1.598100&appid=e5eb4def9773176630cc8f18e75be406&lang=tr&units=metric"
```

### 15.2 JSON APIs
- `AT+BNCURL="GET","https://httpbin.org/json"`
- `AT+BNCURL="POST","https://httpbin.org/post","-H","Content-Type: application/json","-du","50"`

### 15.3 XML/SOAP APIs (from spec example)
```
AT+BNCURL="POST","https://example.com/soap/endpoint","-du","385","-H","Cookie: JSESSIONID=FC33F4334E8FE8074194BCCD9053C148","-H","Content-Type: text/xml;charset=UTF-8","-H","SOAPAction: \"/getQuestions\""
```

## 16. Edge Cases and Boundary Tests

### 16.1 Parameter Order Variations
- `AT+BNCURL="GET","https://httpbin.org/get","-v","-H","Accept: */*"`
- `AT+BNCURL="GET","https://httpbin.org/get","-H","Accept: */*","-v"`
- `AT+BNCURL="POST","https://httpbin.org/post","-dd","/output.txt","-du","100","-H","Content-Type: text/plain"`

### 16.2 Empty and Null Parameters
- `AT+BNCURL="GET","https://httpbin.org/get","-H",""`
- `AT+BNCURL="GET","https://httpbin.org/get","-c",""`
- `AT+BNCURL="GET","https://httpbin.org/get","-b",""`

### 16.3 Maximum Parameter Limits
- Test with 10+ headers to verify "no limit" claim
- Very long header values (1KB+)
- Very long URLs (2KB+)
- Maximum range values

## 17. UART Communication Tests

### 17.1 Data Upload Flow
1. Send command with `-du,"100"`
2. Verify ESP32 responds with `>`
3. Send exactly 100 bytes
4. Verify proper handling and server response

### 17.2 Data Download Flow (no -dd)
1. Send GET command without `-dd`
2. Verify `+LEN:xxxx,` format
3. Verify `+POST:512,` chunked data format
4. Verify final `SEND OK` confirmation

### 17.3 Flow Control Tests
- Test UART flow control with `-` and `+` signals (if implemented)
- Large data transfers that might trigger flow control

## 18. Cookie Persistence Tests

### 18.1 Cookie Save and Load Cycle
1. `AT+BNCURL="GET","https://httpbin.org/cookies/set/session/abc123","-c","test_cookies"`
2. Verify cookie saved to file and sent via UART
3. `AT+BNCURL="GET","https://httpbin.org/cookies","-b","test_cookies"`
4. Verify cookie sent to server

### 18.2 Cookie File Management
- Test overwriting existing cookie files
- Test invalid cookie file formats
- Test cookie file corruption handling

## 19. Performance and Stress Tests

### 19.1 Rapid Command Sequence
- Send multiple BNCURL commands in quick succession
- Test command queuing/processing

### 19.2 Connection Reuse
- Multiple requests to same server
- Test keep-alive connection handling

### 19.3 Memory Management
- Large response handling
- Multiple concurrent operations (if supported)

## 20. Security and Validation Tests

### 20.1 Input Sanitization
- SQL injection attempts in URLs/headers
- Command injection attempts
- Buffer overflow attempts with very long parameters

### 20.2 SSL/TLS Validation
- Valid certificates
- Invalid certificates
- Certificate chain validation
- Hostname verification

## 21. Expected Response Formats

### 21.1 Success Responses
- `OK` for successful completion
- `+LEN:1234,` for UART data length indication
- `+POST:512,<data>` for chunked UART data
- `SEND OK` for completion confirmation

### 21.2 Error Responses
- `ERROR` for general failures
- Specific error codes/messages for different failure types
- Timeout indicators
- Network error indicators

## Test Execution Notes

1. **Sequential Testing**: Run tests in order to build complexity gradually
2. **Cleanup**: Verify proper cleanup of resources between tests
3. **Logging**: Use `-v` flag intermittently to verify internal operation
4. **File System**: Ensure SD card has adequate space for download tests
5. **Network**: Use reliable test endpoints (httpbin.org is recommended)
6. **Timing**: Include delays between rapid-fire tests to prevent overwhelming
7. **Verification**: For each test, verify both command acceptance and actual network behavior
8. **Documentation**: Record actual vs expected behavior for each test case


# BNCurl Testbook (Milestone 1, Milestone 2, and Next)

**Version:** 1.0
**Date:** 27 Aug 2025
**Device Under Test (DUT):** ESP32-based BNCurl AT firmware
**AT rule:** All parameters are in **double quotes** (deviation from original spec)

---

## 0) Scope & Goals

This testbook validates the BNCurl AT implementation across:

* **Milestone 1:** `GET` parsing & URL handling; HTTPS/TLS 1.3; FAT32 SD writes; UART streaming with framing; small files; redirects (auto-follow).
* **Milestone 2:** `POST` & `HEAD`; `-du` uploads from UART/SD; `-dd` sink to SD/UART; custom `Content-Type` via `-H`; order independence; basic error handling.
* **Next milestone:** multiple custom headers; cookie save/load; `-r` range downloads (append-only); nested quotes; `-v` verbose; automatic protocol negotiation; timeout & cert error handling; integration scenarios.

**Out of scope (for now):** WPS, cert flashing APIs, cancellation command, Wi‑Fi management, progress polling, and any commands not referenced below.

---

## 1) Conventions & Policies

### 1.1 AT Command Shape

```
AT+BNCURL="<METHOD>","<url>"[,"-option","value"]...
```

* All tokens (method, url, each option and each value) are **quoted**.
* Options may appear in any order (order‑independent parsing).

### 1.2 Framing (UART body from server)

If response is sent to UART (no `"-dd"`):

* Print once before first body byte: `+LEN:<total>,`  (comma at the end)
* Stream chunks: `+POST:<n>,<raw_n_bytes>`  (repeat until total bytes sent)
* End marker: `SEND OK`
* Then AT result line: `OK` or `ERROR`.

### 1.3 Sinks & Sources

* `"-dd","/abs/path"` → save response to SD (no UART body).
* `"-du","<N>"` → upload **exactly N** bytes taken from UART after `>` prompt.
* `"-du","@/abs/path"` → upload from SD file (stream, not load‑all).
* For `-dd` paths beginning with `@`, the `@` is **not** part of the filename (normalize to `/abs/path`).

### 1.4 Option Policies

* `-H` may repeat (multiple headers). Duplicate header keys: **last wins** (recommended).
* Duplicate **options** (`-du`, `-dd`, `-r`, `-c`, `-b`) → **ERROR**.
* Illegal combos:

  * `GET`/`HEAD` with `-du` → **ERROR**.
  * `POST`/`HEAD` with `-r` → **ERROR**.
  * `-r` without `-dd` → **ERROR** (range requires append-to-file).
* `HEAD` **never** streams/saves a body; `-dd` is ignored (or rejected—pick one and remain consistent).

### 1.5 Redirect Semantics

* Auto follow 301/302/303/307/308.
* 303 after POST: **switch to GET** (standard behavior).
* 307/308 after POST: **preserve POST** (body & method unchanged).

### 1.6 Timeouts & TLS

* A single request timeout applies to connect+response (default or via future `AT+BNCURL_TIMEOUT`).
* TLS must verify host; typical failures: expired, wrong host/SAN, self‑signed. Fail with a single `ERROR`.

### 1.7 Logging (`-v`)

* `-v` prints DNS/TLS handshake, request/response start lines and headers (like curl), then (if GET/POST body to UART) the framed body as usual.

---

## 2) Test Environment & Pre‑Flight

* Stable internet access to `httpbin.org` and `badssl.com` (or controlled equivalents).
* SD card: FAT32, mounted; create folders: `/Download`, `/Upload`, `/Upload/results`, `/Cookies`.
* Prepare SD payloads: `/Upload/payload.bin` (≥256B), `/Upload/payload2.bin` (1KB), `/Upload/large_512k.bin` (512KB).
* Baud: start 115200; you may raise to 3,000,000 later for stress.

Quick sanity:

```
# list dir, write a tiny file, read it back using your existing AT FS cmds
```

---

## 3) Traceability Matrix

| Area                  | IDs        |
| --------------------- | ---------- |
| GET basics            | G1–G7      |
| GET errors            | G8–G12     |
| POST basics           | P1–P8      |
| POST errors           | PE1–PE6    |
| HEAD basics/errors    | H1–H5, HE1 |
| Headers combos        | HC1–HC4    |
| Cookies               | C1–C4      |
| Range & append        | R1–R8      |
| Redirects             | D1–D3      |
| Nested quotes         | N1         |
| Verbose               | V1–V3      |
| Protocol negotiation  | PN1–PN2    |
| Timeouts & TLS errors | E1–E5      |
| Parser robustness     | X1–X6      |
| SD robustness         | SD1–SD4    |
| Stress & soak         | S1–S4      |
| Integration flows     | I1–I3      |

---

## 4) Detailed Test Cases

> **Note:** All commands below use **double-quoted** parameters per our rule.

### 4.1 GET — Baseline

**G1 — HTTPS → UART (framed)**
`AT+BNCURL="GET","https://httpbin.org/get"`
Expect: 200; `+LEN:<n>,` then `+POST:…`; `SEND OK`.

**G2 — HTTP → UART**
`AT+BNCURL="GET","http://httpbin.org/get"`
Expect: 200; framed UART body.

**G3 — HTTPS → SD (JSON)**
`AT+BNCURL="GET","https://httpbin.org/anything/small","-dd","/Download/small.json"`
Expect: file created; no UART body.

**G4 — HTTP explicit port → SD**
`AT+BNCURL="GET","http://httpbin.org:80/uuid","-dd","/Download/uuid.txt"`
Expect: \~40–80 bytes.

**G5 — Redirect (single) → SD**
`AT+BNCURL="GET","http://httpbin.org/redirect-to?url=https%3A%2F%2Fhttpbin.org%2Fip","-dd","/Download/ip.json"`

**G6 — Redirect chain (multi) → SD**
`AT+BNCURL="GET","http://httpbin.org/redirect/3","-dd","/Download/redirect3.json"`

**G7 — Small binary 4KB → UART**
`AT+BNCURL="GET","https://httpbin.org/bytes/4096"`
Expect: `+LEN:4096,` and chunk sum = 4096.

### 4.2 GET — Error Paths

**G8 — 404 → UART**
`AT+BNCURL="GET","https://httpbin.org/status/404"`

**G9 — Invalid host**
`AT+BNCURL="GET","https://no-such-host.numois.example/"` → `ERROR`.

**G10 — SD path invalid**
`AT+BNCURL="GET","https://httpbin.org/json","-dd","/no_such_dir/out.json"` → `ERROR`.

**G11 — SD absent/unmounted**
Unmount SD; run:
`AT+BNCURL="GET","https://httpbin.org/json","-dd","/Download/test.json"` → `ERROR`.

**G12 — `-dd` with leading `@` normalization**
`AT+BNCURL="GET","https://httpbin.org/json","-dd","@/Download/atname.json"`
Expect: path realized without `@`.

**G13 — (Optional) Unknown content-length (chunked) → UART**
`AT+BNCURL="GET","https://httpbin.org/stream/20"`
Expect: proper framing even if length is not known upfront.

---

### 4.3 POST — Baseline

**P1 — POST (UART body → UART)**
`AT+BNCURL="POST","https://httpbin.org/post","-du","8"`
After `>` send `abcdefgh`.
Expect: framed JSON response.

**P2 — POST (UART body → SD)**
`AT+BNCURL="POST","https://httpbin.org/post","-du","12","-dd","/Upload/results/post_uart.json"`
After `>` send `hello world!`.
Expect: file saved.

**P3 — POST empty body**
`AT+BNCURL="POST","https://httpbin.org/post","-du","0"`
Expect: framed JSON indicating empty data.

**P4 — POST (SD body → UART)**
`AT+BNCURL="POST","https://httpbin.org/post","-du","@/Upload/payload.bin"`
Expect: framed response.

**P5 — POST (SD body → SD)**
`AT+BNCURL="POST","https://httpbin.org/post","-du","@/Upload/payload2.bin","-dd","/Upload/results/post_sd.json"`

**P6 — POST with Content‑Type (UART → UART)**
`AT+BNCURL="POST","https://httpbin.org/post","-du","18","-H","Content-Type: text/plain; charset=UTF-8"`
Send `hello-from-esp32!!`.
Expect: header echoed.

**P7 — POST with vendor Content‑Type (UART → SD)**
`AT+BNCURL="POST","https://httpbin.org/post","-du","20","-H","Content-Type: application/vnd.numois.rsu+json","-dd","/Upload/results/ct_vendor.json"`

**P8 — Order independence**
`AT+BNCURL="POST","https://httpbin.org/post","-dd","/Upload/results/order.json","-du","12"`
Send `hello order!`.
Expect: accepted, saved.

### 4.4 POST — Errors

**PE1 — Missing `-du`**
`AT+BNCURL="POST","https://httpbin.org/post"` → `ERROR`.

**PE2 — Both UART len and SD given**
`AT+BNCURL="POST","https://httpbin.org/post","-du","16","-du","@/Upload/p.bin"` → `ERROR`.

**PE3 — SD source missing**
`AT+BNCURL="POST","https://httpbin.org/post","-du","@/Upload/missing.bin"` → `ERROR`.

**PE4 — UART length mismatch (short)**
`AT+BNCURL="POST","https://httpbin.org/post","-du","20"` then send only 10 bytes → timeout/abort → `ERROR`.

**PE5 — Duplicate `-dd`**
`AT+BNCURL="POST","https://httpbin.org/post","-du","0","-dd","/a.json","-dd","/b.json"` → `ERROR`.

**PE6 — `-dd` with leading `@` normalization**
`AT+BNCURL="POST","https://httpbin.org/post","-du","0","-dd","@/Upload/results/resp.json"`
Expect: saved at `/Upload/results/resp.json`.

---

### 4.5 HEAD — Baseline & Errors

**H1 — Basic HEAD (HTTPS)**
`AT+BNCURL="HEAD","https://httpbin.org/get"`
Expect: headers only; no body framing.

**H2 — HEAD with explicit port**
`AT+BNCURL="HEAD","https://httpbin.org:443/anything"`

**H3 — HEAD redirect auto‑follow**
`AT+BNCURL="HEAD","http://httpbin.org/redirect-to?url=https%3A%2F%2Fhttpbin.org%2Fget"`

**H4 — HEAD 404**
`AT+BNCURL="HEAD","https://httpbin.org/status/404"`

**H5 — HEAD invalid host**
`AT+BNCURL="HEAD","https://no-such-host.numois.example/"` → `ERROR`.

**HE1 — Illegal `-du` on HEAD**
`AT+BNCURL="HEAD","https://httpbin.org/get","-du","4"` → `ERROR`.

---

### 4.6 Headers — Multiple & Duplicates

**HC1 — Two headers (UART → UART)**
`AT+BNCURL="POST","https://httpbin.org/post","-du","8","-H","Content-Type: text/plain","-H","X-Trace-ID: 12345"`
Send `abcdefgh`.

**HC2 — Four headers (SD → SD)**
`AT+BNCURL="POST","https://httpbin.org/post","-du","@/Upload/payload.bin","-H","Content-Type: application/octet-stream","-H","Accept: */*","-H","X-App: Numois","-H","X-Mode: test","-dd","/Upload/results/h_many.json"`

**HC3 — Duplicate header (last wins)**
`AT+BNCURL="POST","https://httpbin.org/post","-du","4","-H","Content-Type: text/plain","-H","Content-Type: application/json"`
Send `null`.
Expect: JSON shows `application/json`.

**HC4 — Malformed header value (parser robustness)**
`AT+BNCURL="POST","https://httpbin.org/post","-H","X: ","-du","0"`
Expect: either accept empty value or `ERROR` (document choice).

---

### 4.7 Cookies — Save/Load

**C1 — Save cookie to file**
`AT+BNCURL="GET","https://httpbin.org/cookies/set?session=abc123","-c","/Cookies/sess.txt","-dd","/Download/cookie_set.json"`
Expect: `/Cookies/sess.txt` created/overwritten.

**C2 — Send cookie from file**
`AT+BNCURL="GET","https://httpbin.org/cookies","-b","/Cookies/sess.txt","-dd","/Download/cookie_echo.json"`
Expect: response shows `"session":"abc123"`.

**C3 — Overwrite cookie file**
`AT+BNCURL="GET","https://httpbin.org/cookies/set?session=overwritten","-c","/Cookies/sess.txt"`

**C4 — Cookie file missing**
`AT+BNCURL="GET","https://httpbin.org/cookies","-b","/Cookies/missing.txt"` → `ERROR`.

---

### 4.8 Range Downloads (`-r start-end`) — Append Only

**R1 — First range, new file**
`AT+BNCURL="GET","https://httpbin.org/bytes/1048576","-r","0-262143","-dd","/Download/ranges.bin"`
Expect: size 262,144 bytes.

**R2 — Next contiguous range (append)**
`AT+BNCURL="GET","https://httpbin.org/bytes/1048576","-r","262144-524287","-dd","/Download/ranges.bin"`
Expect: size 524,288 bytes.

**R3 — Final range**
`AT+BNCURL="GET","https://httpbin.org/bytes/1048576","-r","524288-1048575","-dd","/Download/ranges.bin"`
Expect: size 1,048,576 bytes.

**R4 — Out‑of‑order ranges (append anyway)**
`AT+BNCURL="GET","https://httpbin.org/bytes/1048576","-r","0-65535","-dd","/Download/ranges_oOO.bin"`
`AT+BNCURL="GET","https://httpbin.org/bytes/1048576","-r","131072-196607","-dd","/Download/ranges_oOO.bin"`
`AT+BNCURL="GET","https://httpbin.org/bytes/1048576","-r","65536-131071","-dd","/Download/ranges_oOO.bin"`
Expect: final size 196,608 bytes.

**R5 — Overlapping range**
`AT+BNCURL="GET","https://httpbin.org/bytes/262144","-r","0-131071","-dd","/Download/overlap.bin"`
`AT+BNCURL="GET","https://httpbin.org/bytes/262144","-r","65536-196607","-dd","/Download/overlap.bin"`
Expect: size 262,144 bytes (duplicate region included, host responsibility).

**R6 — Range + redirect**
`AT+BNCURL="GET","http://httpbin.org/redirect-to?url=https%3A%2F%2Fhttpbin.org%2Fbytes%2F131072","-r","0-65535","-dd","/Download/range_redirect.bin"`

**R7 — Error: `-r` without `-dd`**
`AT+BNCURL="GET","https://httpbin.org/bytes/65536","-r","0-32767"` → `ERROR`.

**R8 — Error: `-r` on POST/HEAD**
`AT+BNCURL="POST","https://httpbin.org/post","-r","0-100","-du","0"` → `ERROR`.
`AT+BNCURL="HEAD","https://httpbin.org/get","-r","0-100"` → `ERROR`.

---

### 4.9 Redirect Semantics

**D1 — GET 301/302**
`AT+BNCURL="GET","http://httpbin.org/redirect/1","-dd","/Download/redir_g.json"`
Expect: final 200, saved.

**D2 — POST 303 (switch to GET)**
`AT+BNCURL="POST","https://httpbin.org/status/303","-du","0","-dd","/Upload/results/redir_303.json"`
Expect: followed as GET; saved.

**D3 — POST 307/308 (preserve POST)**
`AT+BNCURL="POST","https://httpbin.org/redirect-to?url=https%3A%2F%2Fhttpbin.org%2Fpost&status_code=307","-du","4","-dd","/Upload/results/redir_307.json"`
After `>` send `test`.
Expect: followed, method preserved.

---

### 4.10 Nested Quotes in Headers

**N1 — SOAPAction with quotes**
`AT+BNCURL="POST","https://httpbin.org/post","-H","SOAPAction: \"/getQuestions\"","-du","0","-dd","/Upload/results/soap_hdr.json"`
Expect: header appears exactly as `SOAPAction: "/getQuestions"`.

---

### 4.11 Verbose Mode (`-v`)

**V1 — GET verbose**
`AT+BNCURL="GET","https://httpbin.org/get","-v"`
Expect: DNS/TLS lines, `> GET`, `< HTTP/1.1 200`, headers, then framed body.

**V2 — POST verbose**
`AT+BNCURL="POST","https://httpbin.org/post","-du","8","-v"`
Send `abcdefgh`.
Expect: `> POST` with headers (Host, UA, Content-Length), `< HTTP/...` then framed body.

**V3 — HEAD verbose**
`AT+BNCURL="HEAD","https://httpbin.org/get","-v"`
Expect: request/response lines only; no body.

---

### 4.12 Protocol Negotiation

**PN1 — HTTP (no TLS)**
`AT+BNCURL="GET","http://httpbin.org/get","-v"`
Expect: no TLS handshake.

**PN2 — HTTPS (TLS 1.3)**
`AT+BNCURL="GET","https://httpbin.org/get","-v"`
Expect: TLS handshake; TLSv1.3 if server supports.

---

### 4.13 Timeouts & TLS Errors

**E1 — GET timeout**
*(If a timeout AT exists, set to small value first)*
`AT+BNCURL_TIMEOUT=3`
`AT+BNCURL="GET","https://httpbin.org/delay/10"`
Expect: `ERROR` after \~3s; UART still responsive.

**E2 — POST timeout**
`AT+BNCURL_TIMEOUT=3`
`AT+BNCURL="POST","https://httpbin.org/delay/10","-du","0"` → `ERROR`.

**E3 — Expired certificate**
`AT+BNCURL="GET","https://expired.badssl.com/","-v"` → `ERROR`.

**E4 — Wrong host/SAN mismatch**
`AT+BNCURL="GET","https://wrong.host.badssl.com/","-v"` → `ERROR`.

**E5 — Self‑signed**
`AT+BNCURL="GET","https://self-signed.badssl.com/","-v"` → `ERROR`.

---

### 4.14 Parser Robustness

**X1 — Unknown option**
`AT+BNCURL="GET","https://httpbin.org/get","-zz","1"` → `ERROR`.

**X2 — Duplicate option**
`AT+BNCURL="POST","https://httpbin.org/post","-du","8","-du","4"` → `ERROR`.

**X3 — Extraneous whitespace**
`AT+BNCURL   =   "GET" , "https://httpbin.org/get" , "-dd" , "/Download/ws.json"`
Expect: accepted (parser trims whitespace).

**X4 — Empty header value**
`AT+BNCURL="POST","https://httpbin.org/post","-H","X-Empty:","-du","0"`
Accept or reject consistently.

**X5 — Malformed nested quotes**
`AT+BNCURL="POST","https://httpbin.org/post","-H","SOAPAction: \"/oops","-du","0"` → `ERROR`.

**X6 — Very long URL (boundary)**
Use near-limit length your parser accepts; expect: either accepted or clear `ERROR` without crash.

---

### 4.15 SD Robustness

**SD1 — Write then immediate readback verification**
GET to SD, then GET to UART and compare sizes.

**SD2 — Power‑loss simulation (manual)**
Begin GET to SD; simulate SD removal or power loss; on next boot, FS mounts cleanly and partial file exists without corruption of FS structures.

**SD3 — Unmount/mount behavior**
Unmount SD; ensure any GET/POST with `-dd` returns `ERROR`; remount and operations succeed.

**SD4 — Path traversal rejection**
Attempt `-dd` with `..` segments; expect rejection or path normalization that prevents escape from mount root.

---

### 4.16 Stress & Soak

**S1 — Large UART POST (64 KiB)**
`AT+BNCURL="POST","https://httpbin.org/post","-du","65536"`
Send exactly 65,536 bytes; expect success; monitor heap.

**S2 — Large SD POST (512 KiB)**
`AT+BNCURL="POST","https://httpbin.org/post","-du","@/Upload/large_512k.bin","-dd","/Upload/results/p_large_sd.json"`

**S3 — Repeated GET (100x loop)**
Run G1 100 times; ensure no memory growth/leaks; UART stable.

**S4 — Mixed sequence (20 ops)**
Randomized mix of GET/POST/HEAD; ensure no deadlocks; average latency stable.

---

### 4.17 Integration Scenarios

**I1 — Cookie‑gated POST**

1. `AT+BNCURL="GET","https://httpbin.org/cookies/set?auth=ok","-c","/Cookies/auth.txt"`
2. `AT+BNCURL="POST","https://httpbin.org/post","-b","/Cookies/auth.txt","-du","12","-dd","/Upload/results/auth_post.json"`
   Send `hello world!`.

**I2 — Resumable download via ranges**

1. `AT+BNCURL="GET","https://httpbin.org/bytes/524288","-r","0-262143","-dd","/Download/resume.bin"`
2. `AT+BNCURL="GET","https://httpbin.org/bytes/524288","-r","262144-524287","-dd","/Download/resume.bin"`

**I3 — Redirect + header + cookie combo**
`AT+BNCURL="GET","http://httpbin.org/redirect-to?url=https%3A%2F%2Fhttpbin.org%2Fheaders","-H","X-Combo: 1","-c","/Cookies/comb.txt","-dd","/Download/comb.json","-H","X-After: 2"`

---

## 5) Acceptance Checklist (apply to every test)

* No firmware reset/crash; UART remains responsive.
* For UART bodies: `+LEN` appears **before** first `+POST`; chunk sum equals total; ends with `SEND OK`.
* For SD sinks: file exists, sane size; on failure, partial files are either cleaned or clearly left (document policy).
* Redirects: followed with correct method semantics.
* Headers: preserved verbatim; duplicate header policy enforced (last wins).
* Cookies: save overwrites; load sends; missing file → `ERROR`.
* Ranges: appends only; illegal combos rejected.
* `-v`: readable, non‑interleaved logs + framing.
* Timeouts & TLS errors: single `ERROR`; no partial body; next command works.
* Memory: no growth with large transfers; small fixed buffers; files/handles closed on all paths.

---

## 6) Artifacts & Logging

* Keep UART logs (with timestamps) for all failures.
* Copy saved SD files from `/Download` & `/Upload/results` for inspection.
* (Optional) Hash files on host PC to verify range-assembled binaries.
* Record HTTP status codes when available; correlate with behavior (e.g., 303 vs 307).

---

## 7) Known Good Payloads

* `abcdefgh`, `hello world!`, `hello-from-esp32!!`, `null`, repeated `A` blocks, and the prepared SD files.

---

## 8) Notes & Open Decisions

* HEAD + `-dd`: choose **ignore** or **ERROR**; stay consistent (tests assume ignore).
* Empty header values: allow or reject; document behavior and keep tests aligned.
* Chunked/unknown length: ensure header callback emits `+LEN` once value is known; otherwise buffer first few bytes until `Content-Length` observed.

---

**End of Testbook**


# ESP32 Internet Module Complete Test Cases

## 1. WPS (WiFi Protected Setup) Tests

### 1.1 WPS Connection Tests
- `AT+BNWPS="30"` (30 second WPS window)
- `AT+BNWPS="1"` (minimum 1 second)
- `AT+BNWPS="120"` (maximum timeout)
- `AT+BNWPS="300"` (beyond reasonable limit)

### 1.2 WPS Status Query Tests
- `AT+BNWPS?` (query current WPS state)
- Send `AT+BNWPS?` immediately after `AT+BNWPS="30"`
- Send `AT+BNWPS?` after WPS timeout expires
- Send multiple `AT+BNWPS?` queries in sequence

### 1.3 WPS Cancellation Tests
- `AT+BNWPS="0"` (cancel WPS mode)
- `AT+BNWPS="0"` when not in WPS mode
- Cancel WPS during active connection attempt

### 1.4 WPS Error Cases
- `AT+BNWPS=""` (empty parameter)
- `AT+BNWPS="-5"` (negative timeout)
- `AT+BNWPS="abc"` (non-numeric parameter)
- `AT+BNWPS` (missing parameter)

### 1.5 Expected WPS Responses
```
Success: +CWJAP:"MySSID","aa:bb:cc:dd:ee:ff",6,-45,1,0,0,0,3
         OK
Error:   +CWJAP:1
         ERROR
Status:  +BNWPS:"1" (active) or +BNWPS:"0" (inactive)
         OK
```

## 2. Certificate Management Tests

### 2.1 Flash Certificate from SD Card
- `AT+BNFLASH_CERT="0x2A000","@/certs/server.crt"`
- `AT+BNFLASH_CERT="0x2B000","@/certs/client.key"`
- `AT+BNFLASH_CERT="0x2C000","@/certs/ca-bundle.pem"`
- `AT+BNFLASH_CERT="0x10000","@/certs/root_ca.crt"` (different address)

### 2.2 Flash Certificate via UART
- `AT+BNFLASH_CERT="0x2A000","1400"` (expect `>` prompt, then 1400 bytes)
- `AT+BNFLASH_CERT="0x2B000","500"` (small certificate)
- `AT+BNFLASH_CERT="0x2C000","0"` (zero bytes - error case)
- `AT+BNFLASH_CERT="0x2D000","65536"` (large certificate)

### 2.3 Certificate Error Cases
- `AT+BNFLASH_CERT="","1400"` (empty address)
- `AT+BNFLASH_CERT="invalid","1400"` (non-hex address)
- `AT+BNFLASH_CERT="0x2A000","@/nonexistent.crt"` (file not found)
- `AT+BNFLASH_CERT="0x2A000",""` (empty data source)
- `AT+BNFLASH_CERT="0x2A000","-100"` (negative byte count)
- `AT+BNFLASH_CERT="0x2A000","abc"` (non-numeric byte count)

### 2.4 Certificate Address Tests
- `AT+BNFLASH_CERT="0x0","1000"` (address zero)
- `AT+BNFLASH_CERT="0xFFFFFFFF","1000"` (maximum address)
- `AT+BNFLASH_CERT="0x1000","1000"` (4KB aligned)
- Multiple certificates at different addresses

## 3. Enhanced BNCURL Tests (All Parameters in Quotes)

### 3.1 Basic Method Tests (Quoted Parameters)
- `AT+BNCURL="GET","https://httpbin.org/get"`
- `AT+BNCURL="POST","https://httpbin.org/post"`
- `AT+BNCURL="HEAD","https://httpbin.org/get"`

### 3.2 Header Tests with Quotes
- `AT+BNCURL="GET","https://httpbin.org/get","-H","Content-Type: application/json"`
- `AT+BNCURL="GET","https://httpbin.org/get","-H","User-Agent: ESP32-Client/1.0","-H","Accept: application/json"`

### 3.3 Nested Quotes in Headers
- `AT+BNCURL="GET","https://httpbin.org/get","-H","SOAPAction: \"/getQuestions\""`
- `AT+BNCURL="GET","https://httpbin.org/get","-H","X-Custom: \"quoted value\""`
- `AT+BNCURL="GET","https://httpbin.org/get","-H","Authorization: Bearer \"token with spaces\""`

### 3.4 Data Upload Tests (Quoted)
- `AT+BNCURL="POST","https://httpbin.org/post","-du","100"`
- `AT+BNCURL="POST","https://httpbin.org/post","-du","@/test/upload.xml"`
- `AT+BNCURL="POST","https://httpbin.org/post","-du","0"`

### 3.5 Data Download Tests (Quoted)
- `AT+BNCURL="GET","https://httpbin.org/json","-dd","/download/test.json"`
- `AT+BNCURL="GET","https://httpbin.org/xml","-dd","@result.xml"`

### 3.6 Cookie Tests (Quoted)
- `AT+BNCURL="GET","https://httpbin.org/cookies/set/test/value","-c","session_cookies"`
- `AT+BNCURL="GET","https://httpbin.org/cookies","-b","session_cookies"`
- `AT+BNCURL="GET","https://httpbin.org/cookies/set/user/john","-c",""`

### 3.7 Range Tests (Quoted)
- `AT+BNCURL="GET","https://httpbin.org/range/1024","-r","0-511"`
- `AT+BNCURL="GET","https://httpbin.org/range/1024","-r","512-1023"`
- `AT+BNCURL="GET","https://httpbin.org/range/1024","-r","100-200"`

### 3.8 Verbose Tests (Quoted)
- `AT+BNCURL="GET","https://httpbin.org/get","-v"`
- `AT+BNCURL="POST","https://httpbin.org/post","-du","50","-v"`

## 4. Progress Monitoring Tests

### 4.1 Valid Progress Queries
- Start: `AT+BNCURL="GET","https://httpbin.org/drip?duration=5&numbytes=10000","-dd","/large_file.bin"`
- During transfer: `AT+BNCURL_PROG?`
- Expected response: `+BNCURL_PROG:"2048/10000"` or similar
- After completion: `AT+BNCURL_PROG?`

### 4.2 Progress with File Upload
- Start: `AT+BNCURL="POST","https://httpbin.org/post","-du","@/large_upload.bin"`
- During transfer: `AT+BNCURL_PROG?`
- Multiple progress queries during same transfer

### 4.3 Progress Error Cases
- `AT+BNCURL_PROG?` (no active transfer)
- `AT+BNCURL_PROG?` (after transfer completed)
- `AT+BNCURL_PROG?` (during UART-only transfer - should error)

## 5. Transfer Cancellation Tests

### 5.1 Valid Cancellations
- Start: `AT+BNCURL="GET","https://httpbin.org/drip?duration=10&numbytes=50000","-dd","/cancel_test.bin"`
- Cancel: `AT+BNCURL_STOP?`
- Expected: `+BNCURL_STOP:` followed by `OK`

### 5.2 Cancellation during Upload
- Start: `AT+BNCURL="POST","https://httpbin.org/post","-du","10000"`
- Send partial data, then: `AT+BNCURL_STOP?`
- Verify proper cleanup

### 5.3 Cancellation Error Cases
- `AT+BNCURL_STOP?` (no active transfer)
- `AT+BNCURL_STOP?` (UART-only transfer - should error)
- Multiple cancellation attempts

## 6. Timeout Management Tests

### 6.1 Valid Timeout Settings
- `AT+BNCURL_TIMEOUT="1"` (minimum 1 second)
- `AT+BNCURL_TIMEOUT="30"` (default value)
- `AT+BNCURL_TIMEOUT="120"` (maximum 120 seconds)
- `AT+BNCURL_TIMEOUT="60"`

### 6.2 Timeout Query
- `AT+BNCURL_TIMEOUT?`
- Expected: `+BNCURL_TIMEOUT: "30"` (or current value)

### 6.3 Invalid Timeout Values
- `AT+BNCURL_TIMEOUT="0"` (below minimum)
- `AT+BNCURL_TIMEOUT="121"` (above maximum)
- `AT+BNCURL_TIMEOUT="-10"` (negative)
- `AT+BNCURL_TIMEOUT="abc"` (non-numeric)
- `AT+BNCURL_TIMEOUT=""` (empty)

### 6.4 Timeout Behavior Tests
- Set timeout to 5 seconds: `AT+BNCURL_TIMEOUT="5"`
- Test with slow server: `AT+BNCURL="GET","https://httpbin.org/delay/10"`
- Should timeout after 5 seconds with appropriate error

## 7. SD Card Management Tests

### 7.1 Mount/Unmount Operations
- `AT+BNSD_MOUNT` (mount new SD card)
- `AT+BNSD_UNMOUNT` (unmount current card)
- `AT+BNSD_MOUNT` (when already mounted)
- `AT+BNSD_UNMOUNT` (when already unmounted)

### 7.2 Hot Swap Sequence
1. `AT+BNSD_UNMOUNT`
2. Physical card swap
3. `AT+BNSD_MOUNT`
4. Verify file operations work

### 7.3 Mount/Unmount Error Cases
- `AT+BNSD_MOUNT` (no card inserted)
- `AT+BNSD_MOUNT` (corrupted card)
- `AT+BNSD_MOUNT` (non-FAT32 card)
- `AT+BNSD_UNMOUNT` (during active file transfer)

### 7.4 Expected Mount/Unmount Responses
```
Success: +BNSD_MOUNT:
         OK
Error:   +BNSD_MOUNT:
         ERROR
```

## 8. SD Card Formatting Tests

### 8.1 Valid Format Operations
- `AT+BNSD_FORMAT` (format mounted card)
- Verify FAT32 format after completion
- Test file operations after format

### 8.2 Format Error Cases
- `AT+BNSD_FORMAT` (no card mounted)
- `AT+BNSD_FORMAT` (write-protected card)
- `AT+BNSD_FORMAT` (during active file transfer)
- `AT+BNSD_FORMAT` (card removed during format)

### 8.3 Format Interruption Tests
- Start format, then try other operations
- Power cycle during format
- Card removal during format

## 9. SD Card Space Information Tests

### 9.1 Valid Space Queries
- `AT+BNSD_SPACE` (get space info)
- Expected format: `+BNSD_SIZE: "15456000000/4035037"`
- Query after file operations
- Query on nearly full card

### 9.2 Space Query Error Cases
- `AT+BNSD_SPACE` (no card mounted)
- `AT+BNSD_SPACE` (corrupted filesystem)
- `AT+BNSD_SPACE` (card removed)

### 9.3 Space Information Validation
- Verify total + free = reported total
- Test with various card sizes (1GB, 8GB, 32GB, 64GB)
- Verify values after file operations

## 10. UART Speed Configuration Tests

### 10.1 Baud Rate Settings
- Default: 115200 baud operation
- Switch to: 3000000 baud (3Mbaud) at runtime
- Test command processing at different speeds
- Verify data integrity at high speeds

### 10.2 High-Speed Data Transfer Tests
- Large file download at 3Mbaud
- Chunked UART output verification at high speed
- Flow control testing (`-` and `+` signals)

## 11. Complex Integration Tests

### 11.1 Complete Workflow Tests
```
1. AT+BNWPS="30" (connect via WPS)
2. AT+BNSD_MOUNT (mount SD card)
3. AT+BNFLASH_CERT="0x2A000","@/certs/server.crt" (load certificates)
4. AT+BNCURL_TIMEOUT="60" (set timeout)
5. AT+BNCURL="POST","https://secure-api.example.com/upload","-du","@/data/payload.xml","-H","Content-Type: text/xml","-c","session_cookies","-v"
6. AT+BNCURL_PROG? (monitor progress)
7. AT+BNCURL="GET","https://secure-api.example.com/result","-b","session_cookies","-dd","/results/response.xml"
8. AT+BNSD_SPACE (check remaining space)
```

### 11.2 Multi-part Download Sequence (from spec)
```
AT+BNCURL="GET","http://blibu.dzb.de:8082/prod/blibu/cGetDaisyRange/ms/24/1/53008/3607109/22263/04_1.mp3/","-r","0-2097151","-dd","/audio/part1.mp3"
AT+BNCURL="GET","http://blibu.dzb.de:8082/prod/blibu/cGetDaisyRange/ms/24/1/53008/3607109/22263/04_1.mp3/","-r","2097152-4194303","-dd","/audio/part2.mp3"
AT+BNCURL="GET","http://blibu.dzb.de:8082/prod/blibu/cGetDaisyRange/ms/24/1/53008/3607109/22263/04_1.mp3/","-r","4194304-6291455","-dd","/audio/part3.mp3"
```

### 11.3 SOAP Request Test (from spec)
```
AT+BNCURL="POST","https://blibu.dzb.de:8093/dibbsDaisy/services/dibbsDaisyPort/","-du","385","-H","Cookie: JSESSIONID=FC33F4334E8FE8074194BCCD9053C148","-H","Content-Type: text/xml;charset=UTF-8","-H","SOAPAction: \"/getQuestions\""
```
Expected: `>` prompt, then send 385-byte SOAP envelope

### 11.4 Weather API Test (from spec)
```
AT+BNCURL="GET","https://api.openweathermap.org/data/2.5/weather?lat=42.567001&lon=1.598100&appid=e5eb4def9773176630cc8f18e75be406&lang=tr&units=metric"
```

## 12. Error Recovery and Edge Cases

### 12.1 Network Interruption Tests
- Start large download, disconnect WiFi
- Reconnect and verify error handling
- Test timeout behavior during network loss

### 12.2 SD Card Removal Tests
- Remove card during file download
- Remove card during file upload from SD
- Remove card during format operation

### 12.3 Memory and Resource Tests
- Multiple simultaneous certificate loads
- Large file operations with low memory
- Resource cleanup after errors

### 12.4 Malformed Command Tests
- `AT+BNCURL="GET","https://example.com","-H"` (incomplete header)
- `AT+BNCURL="GET","https://example.com","-invalid","param"` (unknown parameter)
- Very long command lines (>1KB)
- Commands with unmatched quotes

## 13. UART Protocol Validation Tests

### 13.1 Data Upload Flow Validation
```
Host: AT+BNCURL="POST","https://httpbin.org/post","-du","100"
ESP32: >
Host: [exactly 100 bytes]
ESP32: [process and respond]
```

### 13.2 Data Download Flow Validation (UART Output)
```
Host: AT+BNCURL="GET","https://httpbin.org/json"
ESP32: +LEN:"543",
ESP32: +POST:"512",[512 bytes of JSON]
ESP32: +POST:"31",[remaining 31 bytes]
ESP32: SEND OK
```

### 13.3 Chunked Transfer Validation
- Verify exact `+POST:"512",` format (with comma)
- Test chunk boundaries don't break data
- Verify final `SEND OK` message
- Test very large responses (>10MB) with many chunks

### 13.4 Flow Control Tests (if implemented)
- Large upload with `-` (pause) and `+` (resume) signals
- Verify proper host response to flow control
- Test flow control during high-speed transfers

## 14. Certificate and TLS Tests

### 14.1 TLS Version Negotiation
- Connect to TLS 1.2 servers
- Connect to TLS 1.3 servers
- Test automatic version negotiation

### 14.2 Certificate Validation
- Valid certificates (Let's Encrypt, commercial CA)
- Self-signed certificates
- Expired certificates
- Wrong hostname certificates

### 14.3 Custom Certificate Usage
- Flash custom CA certificate
- Test connection using custom certificate
- Multiple certificate chains

## 15. File System Integration Tests

### 15.1 File Path Validation
- Valid paths: `"/Download/53008/nlzt0003.smil"`
- Paths with spaces: `"/My Files/test file.txt"`
- Deep directory structures
- Unicode filenames

### 15.2 File Operation Edge Cases
- Download to existing file (overwrite)
- Download with insufficient space
- Upload from empty file
- Upload from very large file (>1GB)

### 15.3 @ Prefix Handling
- Download: `-dd,"@result.xml"` (@ should be omitted from filename)
- Upload: `-du,"@/source/file.bin"` (@ indicates SD card file)
- Verify correct @ handling in both cases

## 16. Performance and Stress Tests

### 16.1 High-Speed Transfer Tests
- 3Mbaud UART with large file transfers
- Verify data integrity at maximum speed
- Test sustained high-speed operations

### 16.2 Concurrent Operation Tests
- Certificate flash during network operation
- SD operations during HTTP transfer
- Multiple HTTP requests in sequence

### 16.3 Memory Stress Tests
- Very large HTTP responses
- Multiple large certificate loads
- Fragmented memory conditions

## 17. Real-World Scenario Tests

### 17.1 Daisy Book Download (from spec example)
```
AT+BNCURL="GET","http://blibu.dzb.de:8082/prod/blibu/cGetDaisyRange/ms/24/1/53008/3607109/22263/nlzt0003.smil/","-dd","/Download/53008/nlzt0003.smil"
```

### 17.2 Multi-part Media Download
- Download large audio file in 2MB chunks
- Verify proper range request handling
- Test with redirected URLs

### 17.3 Session-based API Interaction
1. Login request with cookie saving
2. Authenticated API calls using saved cookies
3. Session timeout handling
4. Logout request

## 18. Error Response Validation

### 18.1 Expected Success Responses
- `OK` for successful operations
- `+BNCURL_PROG:"bytes/total"` for progress
- `+LEN:"size",` for UART data length
- `+POST:"chunk_size",data` for chunked data
- `SEND OK` for transfer completion

### 18.2 Expected Error Responses
- `ERROR` for general failures
- `+BNCURL_STOP: ERROR` for invalid cancellation
- Specific error codes for different failures
- Proper cleanup after errors

## 19. Security and Validation Tests

### 19.1 Input Validation
- SQL injection in URLs
- Command injection in headers
- Buffer overflow attempts
- Malicious certificate data

### 19.2 TLS Security Validation
- Certificate pinning (if implemented)
- Weak cipher rejection
- Protocol downgrade protection

## 20. Interoperability Tests

### 20.1 Server Compatibility
- Apache servers
- Nginx servers
- IIS servers
- CDN endpoints (CloudFlare, AWS)

### 20.2 Protocol Compatibility
- HTTP/1.1 servers
- HTTP/2 servers (if supported)
- Various authentication schemes

## Test Execution Strategy

### Phase 1: Basic Functionality
1. WPS and WiFi connection
2. Basic GET/POST commands
3. SD card operations

### Phase 2: Advanced Features
1. Certificate management
2. Progress monitoring and cancellation
3. Timeout configuration

### Phase 3: Integration Testing
1. Complete workflows
2. Real-world API interactions
3. Multi-part transfers

### Phase 4: Stress and Edge Cases
1. High-speed transfers
2. Error conditions
3. Resource limits

### Validation Checklist for Each Test:
- [ ] Command syntax accepted/rejected correctly
- [ ] Network operation behaves as expected  
- [ ] UART protocol format matches specification
- [ ] File operations work correctly
- [ ] Error handling is appropriate
- [ ] Resource cleanup after completion/failure
- [ ] Response format matches specification

### Critical Test Points:
1. **Quoted Parameters**: All parameters must be in double quotes
2. **Nested Quotes**: Proper handling of `\"` in headers
3. **UART Protocol**: Exact `+LEN:`, `+POST:` format compliance
4. **Flow Control**: Test `-` and `+` signals for UART flow control
5. **Certificate Management**: Proper flashing and validation
6. **Hot Swap**: SD card mount/unmount during operation
7. **Progress Accuracy**: Real-time progress reporting
8. **Range Requests**: Proper HTTP Range header handling with redirects

## 21. Web Radio/Podcast Streaming Tests (AT+BNWEBRADIO)

### 21.1 Basic Web Radio Streaming
- `AT+BNWEBRADIO="http://ice1.somafm.com/groovesalad-256-mp3"` (MP3 stream)
- `AT+BNWEBRADIO="http://relay.broadcastify.com/123456789"` (Emergency scanner)
- `AT+BNWEBRADIO="https://radio.example.com:8000/stream.mp3"` (HTTPS with port)
- `AT+BNWEBRADIO="http://stream.live365.com/a12345"` (AAC stream)

### 21.2 Podcast URL Streaming
- `AT+BNWEBRADIO="https://podcast.example.com/episode123.mp3"`
- `AT+BNWEBRADIO="http://feeds.example.com/podcast/latest.m4a"`
- `AT+BNWEBRADIO="https://cdn.example.com/audio/stream.aac"`

### 21.3 Pure Binary Output Validation
- Start stream: `AT+BNWEBRADIO="http://ice1.somafm.com/groovesalad-256-mp3"`
- Verify **NO** `+LEN:` or `+POST:512,` prefixes in output
- Verify raw MP3/AAC frames go directly to UART
- Check for valid MP3 frame headers (0xFF 0xFB, etc.)
- Verify continuous data flow without protocol overhead

### 21.4 Different Audio Formats
- MP3 streams: Various bitrates (128k, 192k, 320k)
- AAC streams: Different profiles
- OGG streams (if supported)
- Unknown/unsupported formats (should error gracefully)

### 21.5 Web Radio Stop Tests
- `AT+BNWEBRADIO_STOP` (stop active stream)
- `AT+BNWEBRADIO_STOP` (no active stream - should error)
- Multiple stop commands in sequence
- Stop during stream startup/buffering

### 21.6 Stream Interruption and Recovery
- Start stream, stop after 30 seconds
- Start new stream immediately after stop
- Network disconnection during stream
- Server-side stream termination handling

### 21.7 Long-Duration Streaming Tests
- Stream for 1+ hours continuously
- Memory usage monitoring during long streams
- Buffer management validation
- Automatic reconnection on stream interruption

### 21.8 Concurrent Operation Tests
- `AT+BNWEBRADIO="http://stream1.example.com/radio.mp3"`
- During stream: `AT+BNCURL="GET","https://api.example.com/status"` (should fail - resource conflict)
- During stream: `AT+BNSD_SPACE` (should work - read-only operation)
- During stream: `AT+BNWPS?` (should work - status query)

### 21.9 Stream URL Edge Cases
- Redirected stream URLs (301/302 to actual stream)
- Password-protected streams: `http://user:pass@stream.example.com/radio`
- Very long URLs with parameters
- URLs with special characters and encoding

### 21.10 Authentication and Headers for Streams
- Streams requiring custom headers
- User-Agent requirements for certain streams
- Referer header requirements
- Cookie-based stream authentication

### 21.11 Error Handling for Web Radio
- `AT+BNWEBRADIO=""` (empty URL)
- `AT+BNWEBRADIO="ftp://invalid.protocol/stream"` (unsupported protocol)
- `AT+BNWEBRADIO="http://nonexistent.stream.com/radio"` (DNS failure)
- `AT+BNWEBRADIO="http://example.com/notastream.txt"` (non-audio content)

### 21.12 Binary Data Integrity Tests
- Capture stream output and verify MP3 frame sync
- Check for protocol artifacts in binary data
- Validate audio decoder compatibility
- Test with various MP3 encoders (LAME, etc.)

### 21.13 Stream Metadata Handling
- ICY metadata in MP3 streams (song titles, etc.)
- Should metadata be stripped or passed through?
- Handling of variable bitrate streams
- Stream title and genre information

### 21.14 Buffer Management Tests
- Test startup latency (time to first audio data)
- Buffer underrun conditions
- High bitrate streams (320kbps+)
- Low bitrate streams (64kbps)

### 21.15 Network Quality Tests
- Streaming over poor WiFi connection
- Packet loss simulation
- Bandwidth limitation tests
- Jitter and latency impact

### 21.16 Expected Web Radio Responses
```
Start Success:
AT+BNWEBRADIO="http://stream.example.com/radio.mp3"
OK
[pure binary MP3 data flows continuously to UART]

Stop Success:
AT+BNWEBRADIO_STOP
+BNWEBRADIO_STOP:
OK

Start Error:
AT+BNWEBRADIO="http://invalid.url/"
ERROR

Stop Error (no active stream):
AT+BNWEBRADIO_STOP
+BNWEBRADIO_STOP:
ERROR
```

### 21.17 Integration with Other Commands
- Set timeout before streaming: `AT+BNCURL_TIMEOUT="30"`, then `AT+BNWEBRADIO="..."`
- WPS connection setup, then immediate streaming
- Certificate loading for HTTPS streams
- SD card operations before/after streaming

### 21.18 Resource Management Tests
- Memory usage during streaming
- Connection pool management
- Cleanup after stream termination
- Recovery from stream errors

### 21.19 Protocol Comparison Tests
**Standard BNCURL Output (with protocol overhead):**
```
AT+BNCURL="GET","http://example.com/audio.mp3"
+LEN:"1048576",
+POST:"512",[512 bytes with protocol]
+POST:"512",[512 bytes with protocol]
... (continues with protocol overhead)
SEND OK
```

**Web Radio Output (pure binary):**
```
AT+BNWEBRADIO="http://stream.example.com/radio.mp3"
OK
[raw MP3 bytes flow directly - no +LEN:, no +POST:, no SEND OK]
```

### 21.20 Stream Quality Validation
- Compare output with direct stream access
- Verify no data corruption in binary stream
- Test with audio analysis tools
- Validate MP3/AAC frame integrity

## 22. Multi-Protocol Streaming Tests

### 22.1 Different Streaming Protocols
- Icecast streams
- Shoutcast streams
- HLS streams (if supported)
- Direct file streaming vs. live streaming

### 22.2 Stream Format Detection
- Auto-detection of MP3 vs AAC vs other formats
- Handling of unsupported formats
- Content-Type header analysis
- Magic byte detection in stream

### Notes:
- Use reliable test endpoints (httpbin.org recommended)
- Ensure SD card has various test files and adequate free space
- Monitor ESP32 memory usage during stress tests
- Verify TLS certificate validation with known test certificates
- Test both IPv4 and IPv6 connectivity if available
- **Critical**: Verify pure binary output for web radio (no protocol overhead)
- **Critical**: Test audio decoder compatibility with streamed output

