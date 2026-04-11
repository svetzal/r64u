# Changelog

All notable changes to r64u will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.10.0] - 2026-04-11

### Added
- **HVSC STIL integration** - Display tune commentary, cover information, and history for SID files from the HVSC STIL.txt database
- **HVSC BUGlist integration** - Show warnings for known playback issues from the HVSC BUGlist.txt database
- **GameBase64 integration** - Display game metadata (publisher, year, genre, musician, rating) for disk images and SID files from the GameBase64 database (~29,000 games)
- Download controls in Preferences for STIL, BUGlist, and GameBase64 databases
- HVSC path tracking in Songlengths database for metadata cross-referencing
- Diagnostic messages for GameBase64 service state
- `filetype::` pure core module for C64 file type detection
- `IRestClient` interface and `IFileDownloader` gateway for dependency injection
- KeyboardInputService unit tests via mock injection
- Imperative-shell service tests for transfer and HTTP download
- Doxygen documentation gate across full source tree
- Project charter (CHARTER.md) documenting purpose, goals, and scope

### Changed
- **Architecture: functional core / imperative shell** - Extracted pure functional cores across the codebase for testability:
  - `transfer::` - processNext, FTP error/timeout, confirmation, and directory scanning logic
  - `avi::` - AVI container format logic
  - `audiostream::` - Audio packet parsing
  - `restresponse::` - REST JSON response parsing
  - `streamprotocol::` - Stream control protocol
  - `ftp::` - FTP response parsing
  - `filetype::` - C64 file type detection
- Applied Single Responsibility Principle extractions across ExplorePanel, TransferQueue, and MainWindow
- Migrated all services and tests to use `IRestClient` interface
- Extracted device data types to `devicetypes.h`
- Improved const-correctness and removed redundant casting throughout codebase
- Applied clang-format to entire codebase
- Improved Preferences dialog with two-column layout
- HVSC Databases section in Preferences reorganized with separate download buttons for Songlengths, STIL, and BUGlist
- Upgraded Linux CI runner from Ubuntu 22.04 to 24.04 LTS
- Updated CI dependencies: Qt versions, GitHub Action versions
- Improved quality gate infrastructure: Homebrew LLVM discovery, cppcheck Qt suppressions

### Fixed
- GameBase64 filename matching to use base filename only
- HttpFileDownloader cancel race condition
- Flaky testMessageTimeoutClearsDisplay: use QTRY_VERIFY for timer jitter
- Flaky LocalFileProxyModel tests: replace fixed waits with polling
- diskimagereader constant naming (CamelCase) and struct member initialization
- clang-tidy warnings exposed by Qt 6.10.1 and LLVM 22 upgrade
- Windows Qt install and Linux AppImage build failures
- Removed Oracle SQL plugin from Linux AppImage build

## [0.9.0] - 2026-01-25

### Added
- **SID playlist/jukebox** - Queue and play multiple SID files with multi-selection support
- **HVSC Songlengths database** - Look up SID tune durations from the HVSC database
- Song length column and elapsed time display in playlist
- Auto-streaming when playing SID/MOD or running PRG/disk images
- Audio recording support for AVI video capture
- **Favorites, screenshot, and video recording** (Phase 1 enhancements)

### Changed
- Favorites sorted alphabetically (case-insensitive) with star emoji indicators
- Improved favorites button behavior

### Fixed
- Stop button now resets C64 instead of toggling pause

## [0.8.1] - 2026-01-22

### Fixed
- Crash on launch due to setSectionResizeMode called before model was set

## [0.8.0] - 2026-01-22

### Added
- **Recursive folder delete** - Delete folders with all their contents from the remote filesystem
- **Cancel button** - Abort long-running transfer or delete operations from the progress bar
- Progress bar shows delete operation progress (previously only showed copy progress)
- Status bar displays each filename as it's deleted during bulk operations
- Overwrite confirmation and action-oriented dialogs for file transfers
- Folder exists confirmation dialog for recursive uploads
- Multi-batch progress display for concurrent transfers
- ErrorHandler service for standardized error handling
- TransferService to decouple UI from TransferQueue model
- DeviceConnection state machine with unit tests
- Comprehensive RemoteFileModel unit tests
- C64UFtpClient protocol unit tests
- ConfigurationModel dirty state edge case tests
- FilePreview strategy unit tests
- StatusMessageService for coordinated status bar messages
- Cache TTL and invalidation for RemoteFileModel
- Connection state guards on all UI operations
- Visual design specification document
- build_test.sh script for development workflow

### Changed
- **UI overhaul** - Replaced mode dropdown with tab bar, extracted panels from MainWindow
- Progress bar is now a generic operation progress indicator (uploads, downloads, deletes)
- Redesigned TransferQueue with single state machine for serialized operations
- File listings sort with folders first in both local and remote views
- Local file browser columns aligned with C64-specific types
- Panels manage their own settings persistence and subscribe directly to DeviceConnection
- FilePreviewStrategy pattern for file preview rendering
- Extracted StreamingManager and FileBrowserWidget base class

### Fixed
- UI freeze during rapid file transfers (added debounce)
- Empty folder download hanging forever in queue
- Queued batches not starting after cancelling hung batch
- Overwrite confirmation not appearing for second transfer operation
- Progress count showing N+1 of N when transfer completes
- Progress count accumulation across multiple downloads
- Batches not completing when items processed before batch is active
- Overwrite-all selection preserved across queued batches
- Multi-file selection and status message issues
- Quit segfault and overwrite confirmation bugs
- File browser location sync when switching panels
- Upload regression with improved operation guards
- Folder upload/overwrite refresh storms with operation timeout

## [0.7.1] - 2026-01-10

### Fixed
- macOS permissions issues
- Added unit tests for core components

## [0.7.0] - 2026-01-10

### Added
- **Config mode** - Full device configuration management with category browser and item editor
- Config item metadata with dropdown options for enum types and min/max ranges for numeric values
- **Run disk images** - Run D64/D71/D81 files directly (mounts disk, resets, injects LOAD/RUN commands)
- Eject buttons in status bar for quick disk ejection when mounted
- Context menu in Explore/Run mode with Play, Run, Load Config, and Mount options

### Changed
- Config items sorted alphabetically for easier navigation
- Config category list styled to match tree views (alternating colors, better spacing)
- Text input fields in Config panel limited to reasonable width
- Context menu items enable/disable based on file type (matching toolbar behavior)
- "Set as Destination" only available for directories in Transfer mode

### Fixed
- Config mode now switches correctly (was showing Explore/Run panel)
- Config panel reliably loads items when category is selected
- Keyboard buffer injection properly handles commands longer than 10 characters

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

[Unreleased]: https://github.com/svetzal/r64u/compare/v0.10.0...HEAD
[0.10.0]: https://github.com/svetzal/r64u/compare/v0.9.1...v0.10.0
[0.9.0]: https://github.com/svetzal/r64u/compare/v0.8.1...v0.9.0
[0.8.1]: https://github.com/svetzal/r64u/compare/v0.8.0...v0.8.1
[0.8.0]: https://github.com/svetzal/r64u/compare/v0.7.1...v0.8.0
[0.7.1]: https://github.com/svetzal/r64u/compare/v0.7.0...v0.7.1
[0.7.0]: https://github.com/svetzal/r64u/compare/v0.6.1...v0.7.0
[0.6.1]: https://github.com/svetzal/r64u/compare/v0.6.0...v0.6.1
[0.6.0]: https://github.com/svetzal/r64u/compare/v0.5.1...v0.6.0
[0.5.1]: https://github.com/svetzal/r64u/compare/v0.5.0...v0.5.1
[0.5.0]: https://github.com/svetzal/r64u/compare/v0.4.0...v0.5.0
[0.4.0]: https://github.com/svetzal/r64u/compare/v0.3.0...v0.4.0
[0.3.0]: https://github.com/svetzal/r64u/compare/v0.2.0...v0.3.0
[0.2.0]: https://github.com/svetzal/r64u/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/svetzal/r64u/releases/tag/v0.1.0
