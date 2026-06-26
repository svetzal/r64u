No source files changed — the upgrades were to system tools only, nothing to commit.

## Summary

Here's what was audited and updated for this C++/Qt project:

### GitHub Actions (workflow pinned versions)
All already at their latest versions — no changes needed:
| Action | Version |
|--------|---------|
| `actions/checkout` | v7.0.0 ✓ |
| `jurplel/install-qt-action` | v4.3.1 ✓ |
| `actions/upload-artifact` | v7.0.1 ✓ |
| `actions/download-artifact` | v8.0.1 ✓ |
| `softprops/action-gh-release` | v3.0.1 ✓ |
| `linuxdeploy` | 1-alpha-20251107-1 ✓ |
| `linuxdeploy-plugin-qt` | 1-alpha-20250213-1 ✓ |

### Homebrew Build Toolchain (updated)
| Tool | Before | After |
|------|--------|-------|
| `cmake` | 4.3.3 | **4.3.4** |
| `llvm` (clang-format/clang-tidy) | 22.1.7_1 | **22.1.8** |
| `apache-arrow` (llvm dependency) | 24.0.0_4 | **24.0.0_5** |

### Qt / System Libraries
- **Qt 6.10.1** (local) / **Qt 6.11.1** (CI) — installed via Qt installer, not a Homebrew/vcpkg package; upgrading would require running the Qt maintenance tool
- **ZLIB** — system library, auto-detected by CMake

### Quality Gates (all pass after upgrades)
- ✅ **383 tests** — all passed
- ✅ **format-check** — clang-format 22.1.8 reports no formatting violations
- ✅ **cppcheck** — no new warnings from cppcheck 2.21.0