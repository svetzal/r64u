# Changelog

All notable changes to r64u will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.6.1] - 2026-01-10

### Added
- **Video scaling modes** - Three display scaling options for video streaming: Sharp (nearest-neighbor), Smooth (bilinear), and Integer (pixel-perfect with letterboxing)
- Scaling mode preference in Preferences dialog under Video Display
- Radio buttons on View mode toolbar for live scaling mode switching

### Changed
- Default video scaling mode is now Integer (pixel-perfect) for the sharpest retro display

## [0.6.0] - 2026-01-10

### Added
- **SID file viewer** - Display detailed metadata for SID music files including title, author, release info, format version (PSID/RSID v1-v4), song list, SID chip model (6581/8580), video standard (PAL/NTSC), and memory addresses. Supports multi-SID (2SID/3SID) files.
- **System control buttons** - Added Reboot, Pause, Resume, Menu, and Power Off buttons to the main toolbar for device control
- Power Off confirmation dialog with automatic disconnect

### Changed
- System control buttons (Reset, Reboot, Pause, Resume, Menu, Power Off) now enable based on REST API connectivity, not requiring FTP connection

### Fixed
- Connection state machine now properly guards state transitions
- FTP command queue is cleared on disconnect to prevent stale commands
- RemoteFileModel cache is cleared on disconnect
- Connect button now shows "Cancel" during Connecting/Reconnecting states
- Separate data buffers for FTP LIST and RETR operations prevent data corruption
- Added connection timeouts to REST (15s) and FTP (15s) clients

## [0.5.1] - 2026-01-10

### Added
- Intel macOS build support
- ARM builds for Linux and Windows

### Changed
- Linux packages now built on Ubuntu 20.04 for broader compatibility
- Refactored PETSCII conversion to use lookup tables and keep raw data until display

### Fixed
- macOS notarization workflow improvements
- CPack no longer overwrites signed binaries

## [0.5.0] - 2026-01-09

### Added
- **View mode** - Live video and audio streaming from Ultimate 64 devices
  - UDP video receiver with VIC-II color palette
  - UDP audio receiver with Qt Multimedia playback
  - Stream control via TCP port 64
- **Disk image viewer** - Display D64/D71/D81 directory listings with PETSCII graphics using C64 Pro font
- Application icon for all platforms
- Debug logging with `--verbose` flag

### Changed
- Removed Qt WebEngine dependency for lighter builds
- Improved PETSCII to Unicode character mapping

### Fixed
- Local IP detection for streaming
- Stream startup command handling

## [0.4.0] - 2026-01-08

### Added
- Remote file rename functionality
- Remote file delete from toolbar
- Local folder creation
- Local file rename functionality
- Local file delete functionality
- Recursive folder upload and download
- Transfer progress bar (replaced transfer queue widget)
- Quality tooling configuration (clang-tidy, cppcheck, coverage)
- Doxygen documentation
- Unit tests for transfer queue

### Changed
- Reorganized Transfer mode with action toolbars above file panels
- Reset button moved to main toolbar as a machine action
- Consistent toolbar layout across modes
- Improved memory management with smart pointers and RAII patterns

### Fixed
- Recursive download path handling with trailing slashes
- Explore/Run mode navigation now matches Transfer mode behavior

## [0.3.0] - 2026-01-05

### Added
- File details panel with text/HTML preview
- CFG configuration file loading
- MIT license

### Fixed
- CI build issues for Qt WebEngine

## [0.2.0] - 2026-01-01

### Added
- Eject disk functionality for drives A and B
- Transfer queue for managing file operations
- Test connection feature
- Secure credential storage (macOS Keychain)
- README with feature documentation

### Changed
- GitHub releases are now public instead of drafts

## [0.1.0] - 2026-01-01

### Added
- Initial release
- **Explore/Run mode** - Browse remote filesystem, play SID/MOD files, run PRG/CRT programs, mount disk images
- **Transfer mode** - Upload and download files between local and remote filesystems
- REST API client for device control
- FTP client for file transfers
- Auto-reconnect on connection loss
- Preferences dialog for device configuration
- macOS code signing and notarization
- Multi-platform builds (macOS, Linux, Windows)

[0.6.1]: https://github.com/svetzal/r64u/compare/v0.6.0...v0.6.1
[0.6.0]: https://github.com/svetzal/r64u/compare/v0.5.1...v0.6.0
[0.5.1]: https://github.com/svetzal/r64u/compare/v0.5.0...v0.5.1
[0.5.0]: https://github.com/svetzal/r64u/compare/v0.4.0...v0.5.0
[0.4.0]: https://github.com/svetzal/r64u/compare/v0.3.0...v0.4.0
[0.3.0]: https://github.com/svetzal/r64u/compare/v0.2.0...v0.3.0
[0.2.0]: https://github.com/svetzal/r64u/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/svetzal/r64u/releases/tag/v0.1.0
