# r64u - Remote 64 Ultimate

## Overview

**r64u** is a cross-platform Qt desktop application providing remote access to Commodore 64 Ultimate (Ultimate 64 / 1541 Ultimate II+) devices over a local network. Users can browse filesystems, play music, launch programs and cartridges, and transfer files between their computer and C64U device.

## Target Platforms

- macOS (Apple Silicon and Intel)
- Windows 10/11
- Linux (Ubuntu 22.04+, Fedora 38+)

## Technology Stack

| Component | Technology |
|-----------|------------|
| Framework | Qt 6.5+ (Widgets) |
| Language | C++17 |
| Build System | CMake 3.21+ |
| Networking | Qt Network (HTTP/REST), libcurl or Qt FTP for file transfers |
| JSON | Qt JSON or nlohmann/json |
| Package | CPack for installers |

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

| Class | Responsibility |
|-------|----------------|
| `MainWindow` | Application shell, mode switching, menu/toolbar |
| `DeviceConnection` | Manages REST API and FTP connections to a C64U |
| `C64URestClient` | HTTP client for Ultimate REST API v1 |
| `C64UFtpClient` | FTP client for file browsing and transfers |
| `RemoteFileModel` | QAbstractItemModel for C64U filesystem tree |
| `LocalFileModel` | QFileSystemModel wrapper for local filesystem |
| `PreferencesDialog` | Device configuration (IP, password, defaults) |
| `TransferQueue` | Manages upload/download operations with progress |

## Application Modes

The application operates in two distinct modes, each optimizing the UI for specific workflows.

### Explore & Run Mode

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
- **Play** - Play selected SID/MOD file
- **Run** - Run selected PRG/CRT file
- **Mount** - Mount selected D64/D71/D81/G64 to Drive A or B
- **Reset** - Reset the C64 machine
- **Unmount** - Remove mounted disk image

### Transfer Mode

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
- **Upload** - Transfer selected local files to C64U
- **Download** - Transfer selected C64U files to local machine
- **New Folder** - Create directory on C64U
- **Delete** - Remove selected files (with confirmation)
- **Rename** - Rename selected file/folder

## Feature Specifications

### 1. Device Connection

**Configuration:**
- IP address or hostname (e.g., `192.168.1.100` or `c64u`)
- Network password (optional, stored securely)
- Auto-connect on startup (optional)

**Discovery:**
- Manual entry of IP/hostname
- Future: mDNS/Bonjour discovery of Ultimate devices

**Connection Status:**
- Visual indicator in status bar (connected/disconnected/connecting)
- Device info display (product name, firmware version)
- Automatic reconnection on network interruption

### 2. File Browser (C64U)

**Tree View Features:**
- Hierarchical display of C64U filesystem
- Lazy loading of directories (expand on demand)
- File type icons based on extension
- Sort by name, size, type
- Refresh current directory
- Navigate to path via breadcrumb or text input

**Supported Locations:**
- `/Flash` - Internal flash storage
- `/Usb0`, `/Usb1` - USB storage devices
- `/Temp` - Temporary storage

**File Type Recognition:**

| Extension | Type | Icon | Actions |
|-----------|------|------|---------|
| `.sid` | SID Music | 🎵 | Play |
| `.mod` | MOD Music | 🎵 | Play |
| `.prg` | Program | 📄 | Load, Run |
| `.crt` | Cartridge | 🎮 | Run |
| `.d64` | Disk Image | 💾 | Mount, Browse |
| `.d71` | Disk Image | 💾 | Mount |
| `.d81` | Disk Image | 💾 | Mount |
| `.g64` | Disk Image | 💾 | Mount |
| `.tap` | Tape Image | 📼 | (Future) |
| `.rom` | ROM File | 🔧 | Load ROM |

### 3. Music Playback

**SID Player:**
- Play SID files directly on C64U hardware
- Song selection for multi-song SIDs (via `songnr` parameter)
- Display SID metadata (title, author, copyright) if parseable

**MOD Player:**
- Play Amiga MOD files on C64U
- Uses Ultimate's built-in MOD player

**Playback Controls:**
- Play/Stop (no pause - hardware limitation)
- Song number selection for multi-song files

### 4. Program/Cartridge Launching

**PRG Files:**
- **Load**: Load PRG into memory without running
- **Run**: Load and execute PRG

**Cartridge Files:**
- Run CRT files (triggers machine reset with cartridge active)
- Warning dialog for unsaved state

### 5. Disk Image Management

**Mounting:**
- Mount D64/D71/D81/G64 images to Drive A or B
- Mount modes: Read/Write, Read-Only, Unlinked
- Quick swap between images
- Disk swap delay configuration

**Browsing (Future Enhancement):**
- View contents of D64 images without mounting
- Extract files from disk images

**Creation:**
- Create new blank D64/D71/D81/DNP images
- Specify disk name
- Track count selection for D64 (35/40 tracks)

### 6. File Transfers

**Protocol:** FTP (port 21, default Ultimate configuration)

**Upload:**
- Single file or multiple file selection
- Folder upload (recursive)
- Progress indication per file and overall
- Conflict resolution (overwrite/skip/rename)

**Download:**
- Single or multiple file selection
- Folder download (recursive)
- Preserve directory structure
- Progress indication

**Transfer Queue:**
- Queue multiple transfers
- Cancel individual or all transfers
- Resume interrupted transfers (if supported)
- Transfer history/log

### 7. Machine Control

**Actions:**
- **Reset** - Soft reset (like pressing reset button)
- **Reboot** - Full reboot of Ultimate firmware
- **Power Off** - Shut down the device

**Menu Access:**
- Toggle Ultimate menu (equivalent to menu button)

### 8. Preferences

**Device Settings:**
- IP address / hostname
- Network password (stored in system keychain)
- Default download location
- Default mount mode

**Application Settings:**
- Remember window size/position
- Startup behavior (connect automatically, restore last path)
- Transfer conflict default action
- Theme (system/light/dark) - if Qt supports

**Drive Defaults:**
- Default drive for mounting (A or B)
- Default disk type for new images

## User Interface Details

### Main Window

- **Window Title**: `r64u - [hostname] - [mode]` (e.g., "r64u - c64u - Explore/Run")
- **Minimum Size**: 800x600
- **Default Size**: 1024x768

### Toolbar

| Button | Mode | Action |
|--------|------|--------|
| Mode Dropdown | Both | Switch between Explore/Run and Transfer |
| Play | Explore | Play selected SID/MOD |
| Run | Explore | Run selected PRG/CRT |
| Mount | Explore | Mount selected disk image |
| Upload | Transfer | Upload local selection to C64U |
| Download | Transfer | Download C64U selection to local |
| New Folder | Transfer | Create folder on C64U |
| Preferences | Both | Open preferences dialog |

### Context Menus

**C64U File Browser:**
- Play (SID/MOD)
- Run (PRG/CRT)
- Load (PRG)
- Mount to Drive A
- Mount to Drive B
- Download
- Delete
- Rename
- Refresh

**Local File Browser (Transfer Mode):**
- Upload to C64U
- Open in Finder/Explorer
- Copy Path

### Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| `Cmd/Ctrl + 1` | Switch to Explore/Run mode |
| `Cmd/Ctrl + 2` | Switch to Transfer mode |
| `Cmd/Ctrl + ,` | Open Preferences |
| `Cmd/Ctrl + R` | Refresh current view |
| `Enter` | Execute default action for selection |
| `Delete/Backspace` | Delete selected (with confirmation) |
| `Cmd/Ctrl + U` | Upload selected |
| `Cmd/Ctrl + D` | Download selected |
| `F5` | Refresh |

### Status Bar

Left section:
- Drive A status: `Drive A: [disk name] (R/W)` or `Drive A: [none]`
- Drive B status: similar

Right section:
- Connection status: `Connected: hostname (firmware)` or `Disconnected`
- Transfer progress (when active): `Uploading: filename 45%`

## Data Flow

### REST API Integration

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

### FTP Integration

**Directory Listing:**
```
FTP LIST /path → Parse listing → Update RemoteFileModel
```

**File Transfer:**
```
Upload: FTP STOR /remote/path ← local file data
Download: FTP RETR /remote/path → local file
```

## Error Handling

### Connection Errors
- Display modal dialog with retry/cancel options
- Queue operations during brief disconnections
- Clear indication of offline state

### API Errors
- Parse `errors` array from JSON responses
- Display user-friendly error messages
- Log technical details for debugging

### Transfer Errors
- Per-file error handling (continue with others)
- Retry option for failed transfers
- Clear error state indication in queue

## Security Considerations

- Network password stored in OS keychain (macOS Keychain, Windows Credential Manager, libsecret on Linux)
- No plain-text password storage in config files
- FTP credentials follow same secure storage pattern
- All network communication is local network only (no internet exposure)

## Future Considerations

- mDNS/Bonjour device discovery
- Multiple device profiles
- Disk image content browsing
- SID metadata display (HVSC integration)
- Tape image support
- Video/audio streaming display
- Configuration backup/restore
- REU file support
- Scripting/automation support

## File Structure

```
r64u/
├── CMakeLists.txt
├── src/
│   ├── main.cpp
│   ├── mainwindow.cpp/.h
│   ├── ui/
│   │   ├── preferencesdialog.cpp/.h
│   │   ├── transferqueuewidget.cpp/.h
│   │   └── filebrowserwidget.cpp/.h
│   ├── models/
│   │   ├── remotefilemodel.cpp/.h
│   │   ├── localfilemodel.cpp/.h
│   │   └── transferqueuemodel.cpp/.h
│   ├── services/
│   │   ├── deviceconnection.cpp/.h
│   │   ├── c64urestclient.cpp/.h
│   │   ├── c64uftpclient.cpp/.h
│   │   └── transfermanager.cpp/.h
│   └── utils/
│       ├── filetypeutils.cpp/.h
│       └── securestorage.cpp/.h
├── resources/
│   ├── icons/
│   │   ├── app.icns (macOS)
│   │   ├── app.ico (Windows)
│   │   └── filetypes/
│   └── r64u.qrc
├── docs/
│   └── c64-openapi.yaml
└── tests/
    └── ...
```

## Build & Distribution

### Build Requirements
- Qt 6.5+
- CMake 3.21+
- C++17 compiler (Clang 14+, GCC 11+, MSVC 2022)

### Packaging
- macOS: .app bundle in .dmg
- Windows: NSIS or WiX installer
- Linux: AppImage, .deb, .rpm

## Version History

| Version | Date | Description |
|---------|------|-------------|
| 0.1.0 | TBD | Initial release - basic browsing and playback |
