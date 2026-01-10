# Ultimate 64 REST API Reference

This document provides a human-readable reference for the Ultimate 64 / 1541 Ultimate REST API.
For the formal OpenAPI 3.1 specification, see [c64-openapi.yaml](c64-openapi.yaml).

**Official Documentation:** https://1541u-documentation.readthedocs.io/en/latest/api/api_calls.html

## Overview

- **Base URL:** `http://<device-ip>/v1/`
- **Firmware Required:** 3.11+ (some endpoints require 3.12+)
- **Authentication:** Optional `X-Password` header when network password is configured
- **Response Format:** JSON with `errors` array (empty on success)

## Endpoints by Category

### Device Information

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/v1/version` | Get REST API version |
| GET | `/v1/info` | Get device info (product, firmware, hostname, etc.) |

**Example Response (`/v1/info`):**
```json
{
  "product": "Ultimate 64 Elite",
  "firmware_version": "3.12a",
  "fpga_version": "121",
  "core_version": "1.45",
  "hostname": "c64u",
  "unique_id": "38C1BA",
  "errors": []
}
```

---

### Configuration

| Method | Endpoint | Parameters | Description |
|--------|----------|------------|-------------|
| GET | `/v1/configs` | | List all configuration categories |
| GET | `/v1/configs/{category}` | | Get all items in a category (wildcards supported) |
| GET | `/v1/configs/{category}/{item}` | | Get specific item with current value, min/max, default |
| PUT | `/v1/configs/{category}/{item}` | `value` | Set a configuration value |
| POST | `/v1/configs` | JSON body | Batch update multiple settings |
| PUT | `/v1/configs:load_from_flash` | | Restore configuration from flash |
| PUT | `/v1/configs:save_to_flash` | | Persist configuration to flash |
| PUT | `/v1/configs:reset_to_default` | | Reset to factory defaults |

**Example Response (`/v1/configs`):**
```json
{
  "categories": [
    "Audio Mixer",
    "SID Sockets Configuration",
    "UltiSID Configuration",
    "U64 Specific Settings",
    "C64 and Cartridge Settings",
    "Clock Settings",
    "Network Settings",
    "Drive A Settings",
    "Drive B Settings"
  ],
  "errors": []
}
```

**Example Response (`/v1/configs/Drive A Settings`):**
```json
{
  "Drive A Settings": {
    "Drive": "Enabled",
    "Drive Type": "1541",
    "Drive Bus ID": 8,
    "ROM for 1541 mode": "1541.rom",
    "ROM for 1571 mode": "1571.rom",
    "ROM for 1581 mode": "1581.rom",
    "Extra RAM": "Disabled",
    "Disk swap delay": 1,
    "Resets when C64 resets": "Yes",
    "Freezes in menu": "Yes"
  },
  "errors": []
}
```

---

### Machine Control

| Method | Endpoint | Parameters | Description |
|--------|----------|------------|-------------|
| PUT | `/v1/machine:reset` | | Soft reset the C64 |
| PUT | `/v1/machine:reboot` | | Reboot (reinitialize cartridge) |
| PUT | `/v1/machine:pause` | | Pause via DMA |
| PUT | `/v1/machine:resume` | | Resume from pause |
| PUT | `/v1/machine:poweroff` | | Power off (U64 only) |
| PUT | `/v1/machine:menu_button` | | Simulate menu button press |
| PUT | `/v1/machine:writemem` | `address`, `data` | Write to C64 memory (hex) |
| POST | `/v1/machine:writemem` | `address` + binary body | Write binary data to memory |
| GET | `/v1/machine:readmem` | `address`, `[length]` | Read C64 memory via DMA |
| GET | `/v1/machine:debugreg` | | Read debug register $D7FF (U64) |
| PUT | `/v1/machine:debugreg` | `value` | Write debug register $D7FF (U64) |

**Memory Access Notes:**
- `address` is hexadecimal (e.g., `0400` for screen memory)
- `data` for PUT is hex string (e.g., `48454C4C4F` for "HELLO")
- `length` defaults to 256, max 4096 bytes

---

### Runners (Content Playback)

| Method | Endpoint | Parameters | Description |
|--------|----------|------------|-------------|
| PUT | `/v1/runners:sidplay` | `file`, `[songnr]` | Play SID from filesystem |
| POST | `/v1/runners:sidplay` | `[songnr]` + multipart body | Play uploaded SID |
| PUT | `/v1/runners:modplay` | `file` | Play MOD from filesystem |
| POST | `/v1/runners:modplay` | binary body | Play uploaded MOD |
| PUT | `/v1/runners:load_prg` | `file` | Load PRG via DMA (no run) |
| POST | `/v1/runners:load_prg` | binary body | Load uploaded PRG |
| PUT | `/v1/runners:run_prg` | `file` | Load and run PRG |
| POST | `/v1/runners:run_prg` | binary body | Load and run uploaded PRG |
| PUT | `/v1/runners:run_crt` | `file` | Run cartridge from filesystem |
| POST | `/v1/runners:run_crt` | binary body | Run uploaded cartridge |

---

### Floppy Drives

| Method | Endpoint | Parameters | Description |
|--------|----------|------------|-------------|
| GET | `/v1/drives` | | List all drives and mounted images |
| PUT | `/v1/drives/{drive}:mount` | `image`, `[type]`, `[mode]` | Mount disk image |
| POST | `/v1/drives/{drive}:mount` | `[type]`, `[mode]` + binary body | Mount uploaded image |
| PUT | `/v1/drives/{drive}:reset` | | Reset drive |
| PUT | `/v1/drives/{drive}:remove` | | Unmount/eject disk |
| PUT | `/v1/drives/{drive}:on` | | Power on drive |
| PUT | `/v1/drives/{drive}:off` | | Power off drive |
| PUT | `/v1/drives/{drive}:load_rom` | `file` | Load drive ROM |
| POST | `/v1/drives/{drive}:load_rom` | binary body | Load uploaded ROM |
| PUT | `/v1/drives/{drive}:set_mode` | `mode` | Set drive type (1541/1571/1581) |

**Drive identifiers:** `a`, `b`

**Mount modes:** `readwrite`, `readonly`, `unlinked`

**Image types:** `d64`, `g64`, `d71`, `g71`, `d81`

**Example Response (`/v1/drives`):**
```json
{
  "drives": [
    {
      "a": {
        "enabled": true,
        "bus_id": 8,
        "type": "1541",
        "rom": "1541.rom",
        "image_file": "games.d64",
        "image_path": "/Usb0/games.d64"
      }
    },
    {
      "b": {
        "enabled": false,
        "bus_id": 9,
        "type": "1541",
        "rom": "1541.rom",
        "image_file": "",
        "image_path": ""
      }
    }
  ],
  "errors": []
}
```

---

### Data Streams (Ultimate 64 Only)

| Method | Endpoint | Parameters | Description |
|--------|----------|------------|-------------|
| PUT | `/v1/streams/{stream}:start` | `ip` | Start streaming to IP[:port] |
| PUT | `/v1/streams/{stream}:stop` | | Stop stream |

**Stream types:** `video`, `audio`, `debug`

**Default ports:**
- Video: UDP 11000
- Audio: UDP 11001
- Debug: UDP 11002

---

### File Operations

| Method | Endpoint | Parameters | Description |
|--------|----------|------------|-------------|
| GET | `/v1/files/{path}:info` | | Get file metadata (wildcards supported) |
| PUT | `/v1/files/{path}:create_d64` | `[tracks]`, `[diskname]` | Create D64 image |
| PUT | `/v1/files/{path}:create_d71` | `[diskname]` | Create D71 image |
| PUT | `/v1/files/{path}:create_d81` | `[diskname]` | Create D81 image |
| PUT | `/v1/files/{path}:create_dnp` | `tracks`, `[diskname]` | Create DNP image |

**Example Response (`/v1/files/Flash/roms/1541.rom:info`):**
```json
{
  "files": {
    "path": "/Flash/roms/1541.rom",
    "filename": "1541.rom",
    "size": 16384,
    "extension": "ROM"
  },
  "errors": []
}
```

---

## Error Handling

All responses include an `errors` array. Non-empty arrays indicate failure:

```json
{
  "errors": ["File not found", "Permission denied"]
}
```

HTTP status codes:
- `200` - Success (check `errors` array)
- `404` - Endpoint not found (older firmware)
- `401` - Authentication required (password not provided)

---

## Implementation Status in r64u

| Category | Implemented | Notes |
|----------|-------------|-------|
| `/v1/version` | Yes | `getVersion()` |
| `/v1/info` | Yes | `getInfo()` |
| `/v1/configs` GET | Yes | `getConfigCategories()`, `getConfigCategoryItems()`, `getConfigItem()` |
| `/v1/configs` PUT/POST | Yes | `setConfigItem()`, `updateConfigsBatch()` |
| `/v1/configs:*` | Yes | `saveConfigToFlash()`, `loadConfigFromFlash()`, `resetConfigToDefaults()` |
| `/v1/machine:*` | Yes | All machine control implemented |
| `/v1/runners:*` | Partial | PUT methods only (no uploads) |
| `/v1/drives` | Yes | Full drive control |
| `/v1/streams:*` | Yes | Via StreamControlClient |
| `/v1/files:info` | Yes | `getFileInfo()` |
| `/v1/files:create_*` | Partial | D64 and D81 only |

---

## References

- [Official API Documentation](https://1541u-documentation.readthedocs.io/en/latest/api/api_calls.html)
- [Ultimate 64 Firmware](https://ultimate64.com/Firmware)
- [OpenAPI Specification](c64-openapi.yaml)
