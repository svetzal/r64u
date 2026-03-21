# r64u Project Charter

## Purpose

r64u is a cross-platform desktop application for remotely controlling Commodore 64 Ultimate devices (Ultimate 64, 1541 Ultimate II/II+) over a local network. It replaces the need to physically interact with the device for common tasks like browsing files, launching programs, managing disk images, and streaming video/audio output.

## Goals

- Provide a native desktop GUI (Qt/C++) for browsing, launching, and managing content on C64 Ultimate devices
- Support all major C64 file types: SID, MOD, PRG, CRT, D64/D71/D81/G64, CFG, ROM
- Enable bidirectional file transfers between local machine and device via FTP with queuing and progress tracking
- Stream live VIC-II video and SID audio from the device to the desktop
- Deliver cross-platform builds for macOS, Windows, and Linux with proper signing and packaging
- Integrate retro computing metadata databases (HVSC, GameBase64) for enriched file information

## Non-Goals

- Emulating a Commodore 64 -- this is a remote control tool for real hardware only
- Providing a web-based or mobile interface
- Supporting devices outside the Gideon's Logic Ultimate product family
- Replacing the device's built-in menu system for low-level firmware configuration

## Target Users

Commodore 64 enthusiasts who own Ultimate 64 or 1541 Ultimate II/II+ hardware and want a modern desktop interface to manage their device from across the room or across the network, without repeatedly walking to the physical unit.
