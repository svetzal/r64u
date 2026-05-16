The working tree is clean. Here's a summary of the dependency audit:

## Dependency Update Summary

### Updated
| Package | Before | After | Type |
|---------|--------|-------|------|
| **LLVM** (Homebrew) | 22.1.4 | 22.1.5 | Patch |

### Already Current — No Updates Needed
| Dependency | Version | Notes |
|------------|---------|-------|
| Qt (CI) | 6.11.1 | Latest; local dev uses 6.10.1 (full install) |
| cmake | 4.3.2 | Latest |
| cppcheck | 2.20.0 | Latest |
| zlib | 1.3.2 | Latest |
| `actions/checkout` | v6.0.2 | Latest |
| `jurplel/install-qt-action` | v4.3.1 | Latest |
| `actions/upload-artifact` | v7.0.1 | Latest |
| `actions/download-artifact` | v8.0.1 | Latest |
| `softprops/action-gh-release` | v3.0.0 | Latest |
| `linuxdeploy` | 1-alpha-20251107-1 | Latest stable tag |
| `linuxdeploy-plugin-qt` | 1-alpha-20250213-1 | Latest stable tag |

### Quality Gates (all ✅)
- **format-check** — clang-format 22.1.5 fully compatible; no source files needed reformatting
- **cppcheck** — zero warnings
- **build** — all targets compiled successfully
- **coverage** — all tests passed

No project files were modified; the LLVM upgrade is a pure local tooling update with no repository footprint.