# r64u Packaging

This directory contains resources for building distributable packages of r64u.

## Supported Package Formats

| Format | Platform | Build Method | Status |
|--------|----------|--------------|--------|
| `.dmg` | macOS ARM64 | CPack DragNDrop | ✅ Supported |
| `.exe` (NSIS) | Windows x64 | CPack NSIS | ✅ Supported |
| `.msi` (WiX) | Windows x64 | CPack WIX | ✅ Supported |
| `.deb` | Linux x86_64 | CPack DEB | ✅ Supported |
| `.rpm` | Linux x86_64 | CPack RPM | ✅ Supported |
| AppImage | Linux x86_64 | linuxdeploy | ✅ Supported |

## Building Packages Locally

### Prerequisites

- CMake 3.21+
- Qt 6.5+ with Multimedia module
- Platform-specific tools (see below)

### macOS (.dmg)

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
macdeployqt build/r64u.app
cd build && cpack -G DragNDrop
```

### Windows (.exe NSIS / .msi WiX)

Prerequisites:
- NSIS: https://nsis.sourceforge.io/
- WiX Toolset: https://wixtoolset.org/

```powershell
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
cd build
windeployqt Release/r64u.exe
cpack -G NSIS -C Release   # Creates .exe installer
cpack -G WIX -C Release    # Creates .msi installer
```

### Linux (.deb / .rpm)

Prerequisites:
- dpkg-dev (for DEB)
- rpm-build (for RPM)

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build
cd build
cpack -G DEB   # Creates .deb package
cpack -G RPM   # Creates .rpm package
```

### Linux (AppImage)

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./packaging/build-appimage.sh
```

## GitHub Actions CI/CD

The `.github/workflows/build.yml` workflow automatically builds packages for all platforms.

### Build Matrix

| Runner | Platform | Packages Built |
|--------|----------|----------------|
| `macos-14` | macOS ARM64 | .dmg, .zip |
| `windows-latest` | Windows x64 | .exe (NSIS), .msi (WiX), .zip |
| `ubuntu-22.04` | Linux x86_64 | .deb, .rpm, AppImage |

### Triggering Releases

Releases are created automatically when you push a version tag:

```bash
git tag v0.3.0
git push origin v0.3.0
```

## Platform Constraints and Possibilities

### What's Possible

| Platform | Build On | Cross-Compile To | Notes |
|----------|----------|------------------|-------|
| macOS ARM64 | macos-14 runner | ❌ | Native ARM64 build only |
| macOS Intel | macos-13 runner | ❌ | Could add separate job |
| Windows x64 | windows-latest | ❌ | Native x64 build |
| Windows ARM64 | ❌ | ❌ | No GitHub ARM Windows runners |
| Linux x86_64 | ubuntu-22.04 | ❌ | Native x86_64 build |
| Linux ARM64 | ubuntu-24.04-arm | ❌ | GitHub has ARM Linux runners |

### Known Limitations

1. **macOS Universal Binaries**
   - Could create universal ARM64+x86_64 binaries with `lipo`
   - Requires building on both architectures and combining

2. **Windows ARM64**
   - GitHub doesn't currently offer Windows ARM64 runners
   - Self-hosted runners or alternative CI would be needed

3. **Linux ARM64 (aarch64)**
   - GitHub recently added ARM64 Linux runners
   - Could add `ubuntu-24.04-arm` to the build matrix
   - Would need to install Qt6 from source or PPA

4. **Code Signing**
   - macOS: Requires Apple Developer certificate (secrets configured)
   - Windows: Authenticode signing not currently configured
   - Linux: Package signing not currently configured

### Future Expansion Options

To add **macOS Intel** support:
```yaml
- os: macos-13
  name: macOS-Intel
  artifact: r64u-macos-x86_64
```

To add **Linux ARM64** support:
```yaml
- os: ubuntu-24.04-arm
  name: Linux-ARM64
  artifact: r64u-linux-arm64
```

## Files in This Directory

| File | Purpose |
|------|---------|
| `r64u.desktop` | Linux desktop entry file for DEB/RPM/AppImage |
| `build-appimage.sh` | Script to build AppImage locally |
| `README.md` | This documentation |

## Customization

### DMG Background Image

To add a custom DMG background:
1. Create `packaging/dmg_background.png` (600x400 recommended)
2. Create DS_Store file with desired icon positions
3. Uncomment the DMG customization lines in CMakeLists.txt

### WiX Banners

For custom WiX installer branding:
1. `packaging/wix_banner.png` - 493×58 pixels
2. `packaging/wix_dialog.png` - 493×312 pixels

### Application Icon

Place icons in `resources/icons/`:
- `app.icns` - macOS icon bundle
- `app.ico` - Windows icon
- `app.png` - Linux icon (256x256)
