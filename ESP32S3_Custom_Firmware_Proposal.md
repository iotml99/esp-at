# ESP32-S3 Custom Firmware Proposal  
### Migration of AT-Bones Command Set to nanopb RPC over UART

---

## 1. Executive Summary

This proposal outlines the development of a **custom ESP32-S3 firmware** that re-implements all existing **AT-Bones** functionality (HTTP/HTTPS, SD card, WPS, Certificates, Web Radio, and planned USB-MSC support) using a **modern binary RPC interface** built on **nanopb** (Protocol Buffers for embedded systems) over **UART**.

The new architecture replaces the legacy text-based AT command interpreter with a **schema-driven, binary-framed RPC protocol**—yielding higher performance, structured data exchange, and simpler extensibility.

> ⚠️ **ESP-AT firmware is not supported on ESP32-S3.**  
> Espressif officially states that *“ESP-AT will not support ESP32-S3.”*  
> Therefore, porting or extending the current AT framework is not a maintainable option.

---

## 2. Background

### 2.1 Native ESP-AT Commands in Use

| Category | Command | Description |
|-----------|----------|-------------|
| UART | `AT+UART_CUR=` | Configure UART baud rate and parameters. |
| Wi-Fi | `AT+CWMODE=` | Set Wi-Fi mode (Station/AP/Combo). |
| Wi-Fi | `AT+CWJAP=` | Join access point with SSID and password. |
| Wi-Fi | `AT+CWLAPOPT=` | Configure Wi-Fi scan options. |
| TCP/IP | `AT+CIPSTAMAC?` | Get the station MAC address. |
| TCP/IP | `AT+CIPSTART=` | Start TCP/UDP connection. |
| TCP/IP | `AT+CIPMODE=` | Enable transparent TCP mode. |
| TCP/IP | `AT+CIPSEND=` | Send TCP/UDP data. |
| TCP/IP | `AT+CIPCLOSE` | Close TCP/UDP connection. |

### 2.2 Custom AT-Bones Extensions

| Command | Description |
|----------|-------------|
| `AT+BNCURL` | HTTP/HTTPS GET/POST/HEAD client. Supports redirects, headers, progress, resume, and save-to-SD options. |
| `AT+BNURLCFG` | Store or configure long URLs for repeated use. |
| `AT+BNSD_*` | SD card operations: mount, format, unmount, query space, read/write blocks. |
| `AT+BNWPS` | Initiate Wi-Fi Protected Setup (PIN or PBC mode). |
| `AT+BNCERT_*` | Certificate management: flash, query, and erase certificates from flash memory. |
| `AT+BNWEB_RADIO` | Internet radio streaming over HTTP/HTTPS, optionally recording to SD card. |
| `AT+BNUSB_*` | USB Mass-Storage Class (MSC) host interface: detect media, inquiry (size), read/write 512-byte sectors, flush/eject, and alert pin handling. |

---

## 3. Why We Cannot Use ESP-AT on ESP32-S3

1. **No official support:**  
   Espressif explicitly excludes ESP32-S3 from supported devices for ESP-AT.

2. **Closed-source core:**  
   AT runtime components are provided only as precompiled libraries for selected SoCs.  
   There is no source code for S3 porting or extension.

3. **Feature mismatch:**  
   AT firmware provides no support for USB host or SDMMC modules, both of which are required for BNUSB and BNSD features.

4. **Performance limitations:**  
   Text-based AT parsing, string tokenization, and ASCII-to-binary conversions lead to high CPU overhead and low UART efficiency.

5. **Difficult to extend:**  
   Each new command requires manual grammar edits, breaking backward compatibility easily.

---

## 4. Proposed Solution: nanopb RPC over UART

### 4.1 Architecture Overview

```
Host MCU <---- UART (framed protobuf RPC) ----> ESP32-S3 Firmware
```

| Layer | Responsibility |
|--------|----------------|
| **Framing Layer** | Defines packet boundaries using `[SOF][LEN][PAYLOAD][CRC16]`, with RTS/CTS flow control and retry on CRC failure. |
| **Serialization Layer** | Encodes and decodes structured messages using **nanopb** (Protocol Buffers for MCUs). |
| **RPC Dispatcher** | Routes requests by service and method to corresponding handler modules. |
| **Feature Modules** | Wi-Fi, HTTP, SDMMC, USB-MSC, Certificate, and Web Radio services implemented using ESP-IDF APIs. |

### 4.2 Advantages over AT

| Feature | AT Firmware | nanopb RPC |
|----------|-------------|-------------|
| **Encoding** | ASCII text | Compact binary |
| **Parsing** | Manual tokenization | Auto-generated stubs |
| **Error recovery** | Weak | CRC-protected framing |
| **Streaming** | Manual chunking | Native chunk messages |
| **Async events** | Difficult | Built-in (e.g., progress, hotplug) |
| **Extensibility** | Hardcoded grammar | Schema-based |
| **Throughput** | Limited | High (binary + DMA) |
| **ESP32-S3 support** | ❌ | ✅ (via ESP-IDF) |

---

## 5. RPC Service Mapping

| Legacy AT Command | RPC Service.Method | Description |
|--------------------|--------------------|-------------|
| `AT+CWMODE=` | `Wifi.SetMode(mode)` | Set Wi-Fi mode. |
| `AT+CWJAP=` | `Wifi.Connect(ssid, pass, timeout)` | Join AP with credentials. |
| `AT+CWLAPOPT=` | `Wifi.Scan(options)` | Scan networks with filter options. |
| `AT+BNCURL` | `Http.Start(req)` / `Http.ReadChunk()` / `Http.Finish()` | HTTP/HTTPS client with chunked streaming. |
| `AT+HTTPCPOST` | `Http.UploadChunk()` / `Finish()` | HTTP POST streaming. |
| `AT+BNSD_*` | `SdCard.Mount()`, `GetSpace()`, `Format()`, `ReadBlock()`, `WriteBlock()` | SD card management and I/O. |
| `AT+BNWPS` | `Wifi.StartWps(timeout)` | WPS initiation. |
| `AT+BNCERT_*` | `Cert.Store()`, `Cert.Clear()`, `Cert.Query()` | Certificate operations. |
| `AT+BNWEB_RADIO` | `WebRadio.Start()`, `Stop()`, `ProgressEvt()` | Stream or record audio. |
| `AT+BNUSB_*` | `UsbMsc.Detect()`, `Inquiry()`, `ReadSector()`, `WriteSector()`, `Flush()` | USB Mass-Storage host features. |
| `AT+CIP*` | `Tcp.Open()`, `Tcp.Send()`, `Tcp.Recv()`, `Tcp.Close()` | TCP/UDP socket management. |

---

## 6. Appendix A — Framing Protocol

### UART Frame Format

```
+------+--------+---------------+--------+
| SOF  | Length | Payload (PB)  | CRC16  |
+------+--------+---------------+--------+
```

- **SOF**: Start-of-frame byte (0x7E)
- **Length**: 2 bytes, little-endian
- **Payload**: Encoded protobuf message
- **CRC16**: CCITT-FALSE polynomial, covers Length + Payload

### Example Transaction: BNUSB Read Sector

1. **Host → ESP32-S3:**  
   `UsbMsc.ReadSector(lba=1024, count=1)`  
2. **ESP32-S3 → Host:**  
   `UsbMsc.OpRsp(status=0, data=512B)`  
3. **Host → ESP32-S3:**  
   `UsbMsc.Flush()` → `OK`

---

## 7. Appendix B — Minimal .proto Example

```proto
syntax = "proto3";

package bones.rpc;

message Header {
  uint32 req_id = 1;
  uint32 service = 2;
  uint32 method = 3;
}

message UsbReadReq {
  uint64 lba = 1;
  uint32 count = 2;
}

message UsbOpRsp {
  int32 status = 1;
  bytes data = 2;
}
```

---

## 8. Implementation Timeline (5 Weeks)

| Phase | Duration | Deliverables |
|--------|-----------|--------------|
| **1. Transport & Schema** | 1 week | UART framing, CRC16, nanopb `.proto` design, test Ping/Version RPC. |
| **2. Wi-Fi & TCP** | 1 week | Implement Wi-Fi (mode, connect, WPS) and basic TCP sockets. |
| **3. HTTP/HTTPS (CURL)** | 1 week | Full `Http.Start`, chunk streaming, TLS verify, redirects, timeouts. |
| **4. SDMMC & USB-MSC** | 1 week | SD card operations, USB detection, inquiry, R/W, flush, alert pin events. |
| **5. Integration & Testing** | 1 week | Full validation, memory profiling, documentation, and host SDK stub. |

---

## 9. Deliverables

- ESP-IDF source code for ESP32-S3 firmware  
- `.proto` schema and generated nanopb files  
- UART protocol specification  
- RPC API documentation for host MCU integration  
- Throughput and memory usage report  
- Test logs and validation results  

---

## 10. Conclusion

Migrating from AT-based firmware to **nanopb RPC over UART** provides:

- Native **ESP32-S3 support** (ESP-IDF maintained)  
- Faster, binary-safe communications  
- Support for **USB Host** and **SDMMC**—impossible under AT firmware  
- A long-term, maintainable, extensible architecture  

**ESP-AT is obsolete for this platform**; RPC ensures full control, stability, and future scalability.

---

**Prepared for:** Bones AG – ESP32 Internet Module Upgrade  
**Date:** October 2025  
**Prepared by:** Numois Tech Embedded Systems Division
