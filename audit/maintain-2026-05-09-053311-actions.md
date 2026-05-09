All quality gates are green. Here's the complete dependency audit report:

---

## Dependency Audit — r64u (2026-05-09)

### What was checked

| Dependency | Type | Current | Latest | Status |
|---|---|---|---|---|
| Qt (CI) | Framework | 6.11.0 | 6.11.0 | ✅ Current |
| Qt (local) | Framework | 6.10.1 | 6.11.0 | ⚠️ See below |
| CMake (Homebrew) | Build tool | 4.3.2 | 4.3.2 | ✅ Current |
| cppcheck | Static analysis | 2.20.0 | 2.20.0 | ✅ Current |
| LLVM (clang-format/tidy) | Tooling | 22.1.4 | 22.1.4 | ✅ Current |
| `actions/checkout` | CI action | v6.0.2 | v6.0.2 | ✅ Current |
| `jurplel/install-qt-action` | CI action | v4.3.1 | v4.3.1 | ✅ Current |
| `actions/upload-artifact` | CI action | v7.0.1 | v7.0.1 | ✅ Current |
| `actions/download-artifact` | CI action | v8.0.1 | v8.0.1 | ✅ Current |
| `softprops/action-gh-release` | CI action | v3.0.0 | v3.0.0 | ✅ Current |
| linuxdeploy | CI tool | 1-alpha-20251107-1 | 1-alpha-20251107-1 | ✅ Current |
| linuxdeploy-plugin-qt | CI tool | 1-alpha-20250213-1 | 1-alpha-20250213-1 | ✅ Current |
| ZLIB | System lib | (system) | n/a | ✅ N/A |

### Quality gates (all green)
- **Build + 281 tests**: All passed ✅
- **cppcheck static analysis**: Clean, no findings ✅
- **clang-format check**: All files correctly formatted ✅

### No code changes were needed

Every dependency — GitHub Actions, Homebrew tooling, CI Qt version, and bundled assets — is already at the latest compatible version. The project has no submodules and no vendored third-party libraries, so there's nothing else to track.

### One item to consider (manual, outside code changes)

Your **local Qt installation is 6.10.1**, while the CI builds against **6.11.0** (the current stable). Qt 6.10.2 and 6.10.3 patch releases also exist. Upgrading locally requires running `~/Qt/MaintenanceTool.app` and installing the full 6.11.0 modules — not something that can be scripted. The build works correctly with 6.10.1, and the CI already catches any 6.11.0-specific behaviour, so this is low urgency.