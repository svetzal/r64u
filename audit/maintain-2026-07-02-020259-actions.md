The dependency audit is complete. Here's what I found:

## Dependency Update Audit — r64u

**Result: No updates required — all dependencies are already at their latest compatible versions.**

### What I checked

| Dependency | Current | Latest | Status |
|---|---|---|---|
| `actions/checkout` | v7.0.0 | v7.0.0 | ✅ Current |
| `jurplel/install-qt-action` | v4.3.1 | v4.3.1 | ✅ Current |
| `actions/upload-artifact` | v7.0.1 | v7.0.1 | ✅ Current |
| `actions/download-artifact` | v8.0.1 | v8.0.1 | ✅ Current |
| `softprops/action-gh-release` | v3.0.1 | v3.0.1 | ✅ Current |
| Qt (CI) | 6.11.1 | 6.11.1 | ✅ Current |
| linuxdeploy | 1-alpha-20251107-1 | 1-alpha-20251107-1 | ✅ Current |
| linuxdeploy-plugin-qt | 1-alpha-20250213-1 | 1-alpha-20250213-1 | ✅ Current |

### Notable findings

- **Qt 6.12.0** exists on download.qt.io (Beta 1, June 11, 2026) but is **not yet stable** — final release is planned September 22, 2026. Staying on 6.11.1 is correct.
- **Qt 6.9.3** (current LTS) is also available if stability over new features is ever preferred.
- **linuxdeploy** has a `continuous` rolling tag that's newer than the pinned release, but pinning to the dated release is better practice for reproducible CI builds.
- The project has **no external package manager** (no vcpkg, conan, npm), so all dependencies flow through CMake's `find_package()` and the GitHub Actions CI workflow.

### Quality gates

All gates pass cleanly with no changes needed:
- ✅ `format-check` — zero formatting issues
- ✅ `cppcheck` — zero static analysis warnings  
- ✅ 383 tests — all passing