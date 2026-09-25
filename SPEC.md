# r64u - Remote 64 Ultimate

## Overview

**r64u** is a cross-platform Qt desktop application providing remote access to Commodore 64 Ultimate (Ultimate 64 / 1541 Ultimate II+) devices over a local network. Users can browse filesystems, play music, launch programs and cartridges, and transfer files between their computer and C64U device.

## Target Platforms

- ✅ macOS (Apple Silicon and Intel)
- Windows 10/11
- Linux (Ubuntu 22.04+, Fedora 38+)

## Technology Stack

| Component | Technology | Status |
|-----------|------------|--------|
| Framework | Qt 6.5+ (Widgets) | ✅ |
| Language | C++17 | ✅ |
| Build System | CMake 3.21+ | ✅ |
| Networking | Qt Network (HTTP/REST), Qt FTP for file transfers | ✅ |
| JSON | Qt JSON | ✅ |
| Package | CPack for installers | ✅ |

## Architecture

### High-Level Components

```
┌─────────────────────────────────────────────────────────────────┐
│                         r64u Application                         │
├─────────────────────────────────────────────────────────────────┤
│  UI Layer                                                        │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────────┐  │
│  │ Mode Switch │  │ Button Bar  │  │   Preferences Dialog    │  │
│  └─────────────┘  └─────────────┘  └─────────────────────────┘  │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │                    Main Content Area                       │  │
│  │  ┌─────────────────────┐  ┌─────────────────────────────┐ │  │
│  │  │ C64U File Browser   │  │ Local File Browser          │ │  │
│  │  │ (Tree View)         │  │ (Transfer Mode Only)        │ │  │
│  │  └─────────────────────┘  └─────────────────────────────┘ │  │
│  └───────────────────────────────────────────────────────────┘  │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │                    Status Bar                              │  │
│  └───────────────────────────────────────────────────────────┘  │
├─────────────────────────────────────────────────────────────────┤
│  Service Layer                                                   │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────────────┐   │
│  │ REST Client  │  │ FTP Client   │  │ Device Discovery     │   │
│  └──────────────┘  └──────────────┘  └──────────────────────┘   │
├─────────────────────────────────────────────────────────────────┤
│  Model Layer                                                     │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────────────┐   │
│  │ Device Model │  │ File Model   │  │ Settings Model       │   │
│  └──────────────┘  └──────────────┘  └──────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
```

### Key Classes

| Class | Responsibility | Status |
|-------|----------------|--------|
| `MainWindow` | Application shell, mode switching, menu/toolbar | ✅ |
| `DeviceConnectionManager` | Manages REST API and FTP connections to a C64U | ✅ |
| `C64URestClient` | HTTP client for Ultimate REST API v1 | ✅ |
| `C64UFtpClient` | FTP client for file browsing and transfers | ✅ |
| `RemoteFileModel` | QAbstractItemModel for C64U filesystem tree | ✅ |
| `LocalFileProxyModel` | QFileSystemModel wrapper for local filesystem | ✅ |
| `PreferencesDialog` | Device configuration (IP, password, defaults) | ✅ |
| `TransferQueue` | Manages upload/download operations with progress | ✅ |
| `FileDetailsPanel` | File info and text/HTML preview panel | ✅ |
| `TransferQueueWidget` | UI widget for transfer queue display | ✅ |
| `VideoDisplayWidget` | VIC-II video frame display widget | ✅ |
| `VideoStreamReceiverService` | UDP receiver for video stream packets | ✅ |
| `AudioStreamReceiverService` | UDP receiver for audio stream packets | ✅ |
| `AudioPlaybackService` | Audio playback via Qt Multimedia | ✅ |
| `StreamControlService` | TCP client for video/audio stream control | ✅ |
| `KeyboardInputService` | PETSCII keyboard input via REST API | ✅ |
| `ConfigFileLoaderService` | Load and apply .cfg files to device | ✅ |
| `SystemCredentialStore` | Secure password storage (macOS Keychain) via `ICredentialStore` | ✅ |

## Application Modes

The application operates in three distinct modes, each optimizing the UI for specific workflows.

### Explore & Run Mode ✅

Primary mode for browsing and launching content on the C64U.

**Layout:**
```
┌────────────────────────────────────────────────────────────┐
│ [Explore/Run ▼] [Play] [Run] [Mount] [Reset] [⚙]         │
├────────────────────────────────────────────────────────────┤
│                                                            │
│  📁 /                                                      │
│   ├── 📁 Flash                                             │
│   │   ├── 📁 roms                                          │
│   │   └── 📁 carts                                         │
│   ├── 📁 Usb0                                              │
│   │   ├── 📁 Games                                         │
│   │   │   ├── 🎮 Giana Sisters.crt                        │
│   │   │   └── 💾 Boulder Dash.d64                         │
│   │   └── 📁 Music                                         │
│   │       ├── 🎵 Commando.sid                             │
│   │       └── 🎵 Ocean Loader.sid                         │
│   └── 📁 Usb1                                              │
│                                                            │
├────────────────────────────────────────────────────────────┤
│ Drive A: [none]  Drive B: [none]  │ Connected: c64u (3.12a)│
└────────────────────────────────────────────────────────────┘
```

**Available Actions:**
- ✅ **Play** - Play selected SID/MOD file
- ✅ **Run** - Run selected PRG/CRT file
- ✅ **Mount** - Mount selected D64/D71/D81/G64 to Drive A or B
- ✅ **Reset** - Reset the C64 machine
- ✅ **Unmount** - Remove mounted disk image

### Transfer Mode ✅

Dual-pane interface for file transfers between local machine and C64U.

**Layout:**
```
┌────────────────────────────────────────────────────────────┐
│ [Transfer ▼] [← Upload] [Download →] [New Folder] [⚙]    │
├──────────────────────────┬─────────────────────────────────┤
│ Local Files              │ C64U Files                      │
│                          │                                 │
│  📁 ~/Downloads          │  📁 /Usb0                       │
│   ├── 📁 C64             │   ├── 📁 Games                  │
│   │   ├── 💾 game.d64    │   │   ├── 🎮 cart.crt          │
│   │   └── 🎵 tune.sid    │   │   └── 💾 disk.d64          │
│   └── 📄 readme.txt      │   └── 📁 Music                  │
│                          │                                 │
├──────────────────────────┴─────────────────────────────────┤
│ Transfer Queue: 2 pending │ ████████░░░░░░░░ game.d64 45% │
├────────────────────────────────────────────────────────────┤
│ Drive A: [none]  Drive B: [none]  │ Connected: c64u (3.12a)│
└────────────────────────────────────────────────────────────┘
```

**Available Actions:**
- ✅ **Upload** - Transfer selected local files to C64U
- ✅ **Download** - Transfer selected C64U files to local machine
- ✅ **New Folder** - Create directory on C64U
- ✅ **Delete** - Remove selected files (with confirmation)
- ✅ **Rename** - Rename selected file/folder

### View Mode ✅

Live video and audio streaming from the C64U device.

**Layout:**
```
┌────────────────────────────────────────────────────────────┐
│ [View ▼] [▶ Start Stream] [⏹ Stop Stream] [⚙]            │
├────────────────────────────────────────────────────────────┤
│                                                            │
│  ┌──────────────────────────────────────────────────────┐ │
│  │                                                      │ │
│  │              VIC-II Video Display                    │ │
│  │              (PAL: 384×272 / NTSC: 384×240)          │ │
│  │                                                      │ │
│  └──────────────────────────────────────────────────────┘ │
│                                                            │
├────────────────────────────────────────────────────────────┤
│ Stream: PAL 50fps │ Audio: Playing    │ Connected: c64u   │
└────────────────────────────────────────────────────────────┘
```

**Features:**
- ✅ Live VIC-II video display (4-bit indexed color)
- ✅ PAL (384×272) and NTSC (384×240) format support
- ✅ Audio streaming with jitter buffer
- ✅ Keyboard input forwarding (PETSCII via REST API)
- ✅ Start/stop stream controls

## Feature Specifications

### 1. Device Connection ✅

**Configuration:**
- ✅ IP address or hostname (e.g., `192.168.1.100` or `c64u`)
- ✅ Network password (optional, stored securely)
- ✅ Auto-connect on startup (optional)

**Discovery:**
- ✅ Manual entry of IP/hostname
- Future: mDNS/Bonjour discovery of Ultimate devices

**Connection Status:**
- ✅ Visual indicator in status bar (connected/disconnected/connecting)
- ✅ Device info display (product name, firmware version)
- ✅ Automatic reconnection on network interruption

### 2. File Browser (C64U) ✅

**Tree View Features:**
- ✅ Hierarchical display of C64U filesystem
- ✅ Lazy loading of directories (expand on demand)
- ✅ File type detection based on extension
- ✅ Sort by name, size, type
- ✅ Refresh current directory
- ✅ Navigate to parent directory via toolbar button

**Supported Locations:**
- ✅ `/Flash` - Internal flash storage
- ✅ `/Usb0`, `/Usb1` - USB storage devices
- ✅ `/Temp` - Temporary storage

**File Type Recognition:**

| Extension | Type | Actions | Status |
|-----------|------|---------|--------|
| `.sid` | SID Music | Play | ✅ |
| `.mod` | MOD Music | Play | ✅ |
| `.prg` | Program | Load, Run | ✅ |
| `.crt` | Cartridge | Run | ✅ |
| `.d64` | Disk Image | Mount, Browse | ✅ Mount |
| `.d71` | Disk Image | Mount | ✅ |
| `.d81` | Disk Image | Mount | ✅ |
| `.g64` | Disk Image | Mount | ✅ |
| `.tap` | Tape Image | (Future) | |
| `.rom` | ROM File | Load ROM | ✅ |
| `.cfg` | Config File | Load Config | ✅ |

### 3. Music Playback ✅

**SID Player:**
- ✅ Play SID files directly on C64U hardware
- ✅ Song selection for multi-song SIDs (via `songnr` parameter in API)
- Display SID metadata (title, author, copyright) if parseable

**MOD Player:**
- ✅ Play Amiga MOD files on C64U
- ✅ Uses Ultimate's built-in MOD player

**Playback Controls:**
- ✅ Play/Stop (no pause - hardware limitation)
- Song number selection UI for multi-song files

### 4. Program/Cartridge Launching ✅

**PRG Files:**
- ✅ **Load**: Load PRG into memory without running
- ✅ **Run**: Load and execute PRG

**Cartridge Files:**
- ✅ Run CRT files (triggers machine reset with cartridge active)
- Warning dialog for unsaved state

### 5. Disk Image Management ✅

**Mounting:**
- ✅ Mount D64/D71/D81/G64 images to Drive A or B
- ✅ Mount modes: Read/Write, Read-Only, Unlinked
- ✅ Quick swap between images
- Disk swap delay configuration

**Browsing (Future Enhancement):**
- View contents of D64 images without mounting
- Extract files from disk images

**Creation:**
- ✅ Create new blank D64 images (35/40 tracks)
- ✅ Create new blank D81 images
- ✅ Specify disk name
- D71/DNP image creation

### 6. File Transfers ✅

**Protocol:** ✅ FTP (port 21, default Ultimate configuration)

**Upload:**
- ✅ Single file or multiple file selection
- ✅ Folder upload (recursive)
- ✅ Progress indication per file and overall
- Conflict resolution UI (overwrite/skip/rename)

**Download:**
- ✅ Single or multiple file selection
- ✅ Folder download (recursive)
- ✅ Preserve directory structure
- ✅ Progress indication

**Transfer Queue:**
- ✅ Queue multiple transfers
- ✅ Cancel individual or all transfers
- Resume interrupted transfers (if supported)
- Transfer history/log

### 7. Machine Control ✅

**Actions:**
- ✅ **Reset** - Soft reset (like pressing reset button)
- ✅ **Reboot** - Full reboot of Ultimate firmware
- ✅ **Power Off** - Shut down the device
- ✅ **Pause** - Pause C64 execution
- ✅ **Resume** - Resume C64 execution

**Menu Access:**
- ✅ Toggle Ultimate menu (equivalent to menu button)

### 8. Preferences ✅

**Device Settings:**
- ✅ IP address / hostname
- ✅ Network password (stored in system keychain)
- ✅ Default download location
- ✅ Default mount mode

**Application Settings:**
- Remember window size/position
- ✅ Startup behavior (connect automatically)
- Transfer conflict default action
- Theme (system/light/dark) - if Qt supports

**Drive Defaults:**
- ✅ Default drive for mounting (A or B)
- Default disk type for new images

### 9. Video/Audio Streaming ✅

**Video Streaming:**
- ✅ Receive UDP video stream on port 21000
- ✅ Reassemble 4-bit VIC-II indexed color frames from packets
- ✅ Support PAL (384×272) and NTSC (384×240) formats
- ✅ Display with proper aspect ratio scaling
- ✅ Auto-detect video format from stream

**Audio Streaming:**
- ✅ Receive UDP audio stream on port 21001
- ✅ 16-bit stereo samples at ~48kHz (PAL: 47982.887 Hz, NTSC: 47940.341 Hz)
- ✅ Jitter buffer for smooth playback
- ✅ Qt Multimedia audio output

**Stream Control:**
- ✅ TCP control protocol on port 64
- ✅ Start/stop video and audio streams
- ✅ Configurable stream duration

### 10. Keyboard Input ✅

**PETSCII Keyboard Input:**
- ✅ Convert Qt key events to PETSCII codes
- ✅ Write to C64 keyboard buffer via REST API memory writes
- ✅ Support for alphanumeric and control keys
- Note: Works with BASIC/KERNAL programs, not direct CIA matrix readers

### 11. Config File Loading ✅

**Configuration Files:**
- ✅ Load .cfg files from device filesystem
- ✅ Parse and apply configuration via REST API
- ✅ Status feedback on load success/failure

## User Interface Details

### Main Window ✅

- ✅ **Window Title**: `r64u - [hostname] - [mode]` (e.g., "r64u - c64u - Explore/Run")
- ✅ **Minimum Size**: 800x600
- ✅ **Default Size**: 1024x768

### Toolbar ✅

| Button | Mode | Action | Status |
|--------|------|--------|--------|
| Mode Dropdown | All | Switch between Explore/Run, Transfer, and View | ✅ |
| Play | Explore | Play selected SID/MOD | ✅ |
| Run | Explore | Run selected PRG/CRT | ✅ |
| Mount | Explore | Mount selected disk image | ✅ |
| Upload | Transfer | Upload local selection to C64U | ✅ |
| Download | Transfer | Download C64U selection to local | ✅ |
| New Folder | Transfer | Create folder on C64U | ✅ |
| Start Stream | View | Start video/audio streaming | ✅ |
| Stop Stream | View | Stop video/audio streaming | ✅ |
| Preferences | All | Open preferences dialog | ✅ |

### Context Menus ✅

**C64U File Browser:**
- ✅ Play (SID/MOD)
- ✅ Run (PRG/CRT)
- ✅ Load (PRG)
- ✅ Load Config (CFG)
- ✅ Mount to Drive A
- ✅ Mount to Drive B
- ✅ Download
- ✅ Delete
- ✅ Rename
- ✅ Refresh

**Local File Browser (Transfer Mode):**
- ✅ Upload to C64U
- ✅ New Folder
- ✅ Delete
- ✅ Rename

### Keyboard Shortcuts ✅

| Shortcut | Action | Status |
|----------|--------|--------|
| `Cmd/Ctrl + 1` | Switch to Explore/Run mode | ✅ |
| `Cmd/Ctrl + 2` | Switch to Transfer mode | ✅ |
| `Cmd/Ctrl + 3` | Switch to View mode | ✅ |
| `Cmd/Ctrl + ,` | Open Preferences | ✅ |
| `Cmd/Ctrl + R` | Refresh current view | ✅ |
| `Enter` | Execute default action for selection | ✅ |
| `Delete/Backspace` | Delete selected (with confirmation) | ✅ |
| `Cmd/Ctrl + U` | Upload selected | ✅ |
| `Cmd/Ctrl + D` | Download selected | ✅ |
| `F5` | Refresh | ✅ |

### Status Bar ✅

Left section:
- ✅ Drive A status: `Drive A: [disk name] (R/W)` or `Drive A: [none]`
- ✅ Drive B status: similar

Right section:
- ✅ Connection status: `Connected: hostname (firmware)` or `Disconnected`
- ✅ Transfer progress (when active): `Uploading: filename 45%`

## Data Flow

### REST API Integration ✅

All API calls include the `X-Password` header when a password is configured.

**Device Information Flow:**
```
App Start → GET /v1/info → Display in status bar
         → GET /v1/drives → Update drive status display
```

**File Operations Flow:**
```
User: Run PRG → PUT /v1/runners:run_prg?file=/path/to/file.prg
User: Play SID → PUT /v1/runners:sidplay?file=/path/to/music.sid
User: Mount D64 → PUT /v1/drives/a:mount?image=/path/to/disk.d64
```

### FTP Integration ✅

**Directory Listing:**
```
FTP LIST /path → Parse listing → Update RemoteFileModel
```

**File Transfer:**
```
Upload: FTP STOR /remote/path ← local file data
Download: FTP RETR /remote/path → local file
```

### Video/Audio Streaming Integration ✅

**Stream Control (TCP port 64):**
```
Start Video: [0x20] [0xFF] [len_lo] [len_hi] [duration] [IP:PORT]
Stop Video:  [0x30] [0xFF] [0x00] [0x00]
Start Audio: [0x21] [0xFF] [len_lo] [len_hi] [duration] [IP:PORT]
Stop Audio:  [0x31] [0xFF] [0x00] [0x00]
```

**Video Stream (UDP port 21000):**
```
Packet (780 bytes): Header(12) + Payload(768)
Header: seq(2), frame(2), line(2), ppl(2), lpp(1), bpp(1), enc(2)
Payload: 4 lines × 192 bytes (384 pixels × 4 bits)
```

**Audio Stream (UDP port 21001):**
```
Packet (770 bytes): Header(2) + Payload(768)
Header: sequence number (16-bit)
Payload: 192 stereo samples (16-bit signed, little-endian)
```

## Error Handling ✅

### Connection Errors
- ✅ Display error messages in status bar
- ✅ Queue operations during brief disconnections
- ✅ Clear indication of offline state

### API Errors
- ✅ Parse `errors` array from JSON responses
- ✅ Display user-friendly error messages
- ✅ Log technical details for debugging

### Transfer Errors
- ✅ Per-file error handling (continue with others)
- Retry option for failed transfers
- ✅ Clear error state indication in queue

## Security Considerations ✅

- ✅ Network password stored in OS keychain (macOS Keychain)
- ✅ No plain-text password storage in config files
- ✅ FTP credentials follow same secure storage pattern
- ✅ All network communication is local network only (no internet exposure)
- Windows Credential Manager support (stub implemented)
- Linux libsecret support (stub implemented)

## Future Considerations

- mDNS/Bonjour device discovery
- Multiple device profiles
- Disk image content browsing
- SID metadata display (HVSC integration)
- Tape image support
- ~~Video/audio streaming display~~ ✅ Implemented
- Configuration backup/restore
- REU file support
- Scripting/automation support
- File type icons in tree view
- Theme selection (system/light/dark)
- Transfer conflict resolution UI
- Transfer history/log
- D71/DNP disk image creation

## File Structure

```
r64u/
├── CMakeLists.txt
├── src/
│   ├── main.cpp
│   ├── mainwindow.cpp/.h
│   ├── ui/
│   │   ├── preferencesdialog.cpp/.h          ✅
│   │   ├── transferqueuewidget.cpp/.h        ✅
│   │   ├── filedetailspanel.cpp/.h           ✅
│   │   └── videodisplaywidget.cpp/.h         ✅
│   ├── models/
│   │   ├── remotefilemodel.cpp/.h            ✅
│   │   ├── localfileproxymodel.cpp/.h        ✅
│   │   └── transferqueue.cpp/.h              ✅
│   ├── services/
│   │   ├── deviceconnection.cpp/.h           ✅
│   │   ├── c64urestclient.cpp/.h             ✅
│   │   ├── c64uftpclient.cpp/.h              ✅
│   │   ├── streamcontrolclient.cpp/.h        ✅
│   │   ├── videostreamreceiver.cpp/.h        ✅
│   │   ├── audiostreamreceiver.cpp/.h        ✅
│   │   ├── audioplaybackservice.cpp/.h       ✅
│   │   ├── keyboardinputservice.cpp/.h       ✅
│   │   ├── configfileloader.cpp/.h           ✅
│   │   └── credentialstore.h/.cpp/.mm        ✅
│   └── utils/
│       └── logging.h                          ✅
├── resources/
│   ├── icons/
│   │   └── filetypes/
│   ├── Info.plist.in                          ✅
│   └── r64u.qrc                               ✅
├── docs/
│   └── c64-openapi.yaml                       ✅
└── tests/
    ├── CMakeLists.txt                         ✅
    └── test_transferqueue.cpp                 ✅
```

## Build & Distribution

### Build Requirements
- ✅ Qt 6.5+
- ✅ CMake 3.21+
- ✅ C++17 compiler (Clang 14+, GCC 11+, MSVC 2022)

### Packaging
- ✅ macOS: .app bundle
- ✅ macOS: .dmg installer (CPack DragNDrop)
- ✅ Windows: NSIS .exe installer (CPack NSIS)
- ✅ Windows: .msi installer (CPack WIX)
- ✅ Linux: .deb package (CPack DEB)
- ✅ Linux: .rpm package (CPack RPM)
- ✅ Linux: AppImage (linuxdeploy)

## Version History

| Version | Date | Description |
|---------|------|-------------|
| 0.1.0 | TBD | Initial release - basic browsing and playback |
