# r64u - Remote 64 Ultimate

A cross-platform desktop application for remotely controlling Commodore 64 Ultimate devices (Ultimate 64 / 1541 Ultimate II+) over your local network.

> **Early Development Warning**
> This project is in a very early stage. Expect bugs, missing features, and breaking changes. Use at your own risk!

## Download

**[Download Latest Release](https://github.com/svetzal/r64u/releases/latest)**

Available for:
- macOS (signed and notarized)
- Windows
- Linux (AppImage)

## What Works Right Now

- **Browse files** on your Ultimate device (Flash, USB drives)
- **Play SID music** directly on the C64 hardware
- **Play MOD files** using the Ultimate's built-in player
- **Run PRG files** (load and execute programs)
- **Run CRT files** (cartridge images)
- **Mount disk images** (D64/D71/D81/G64) to Drive A or B
- **Upload/download files** between your computer and the device
- **Create folders** on the device
- **Reset** the C64 machine

## Screenshots

*Coming soon*

## Requirements

- A Commodore 64 Ultimate device (Ultimate 64, 1541 Ultimate II, or 1541 Ultimate II+)
- Device must be on the same local network as your computer
- Network/FTP access enabled on the device

## Quick Start

1. Download and run r64u
2. Go to **File > Preferences** (or click Preferences in the toolbar)
3. Enter your device's IP address or hostname
4. Click **Connect**
5. Browse, play, and enjoy!

## Building from Source

### Requirements
- Qt 6.5+
- CMake 3.21+
- C++17 compiler

### Build
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Roadmap

Features planned for future releases:
- Device discovery (mDNS/Bonjour)
- Disk image content browsing (view D64 contents without mounting)
- SID metadata display (title, author from HVSC)
- Transfer queue with progress for multiple files
- Drag and drop support

## License

*License TBD*

## Acknowledgments

- [Gideon's Logic](https://ultimate64.com/) for creating the Ultimate series of hardware
- The C64 community for keeping the platform alive
