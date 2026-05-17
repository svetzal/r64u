## Dependency Update Summary — 2026-05-17

No updates required. All dependencies are already at their latest compatible versions.

### GitHub Actions — All Current ✅

| Action | Version | Latest |
|--------|---------|--------|
| `actions/checkout` | v6.0.2 | v6.0.2 ✅ |
| `jurplel/install-qt-action` | v4.3.1 | v4.3.1 ✅ |
| `actions/upload-artifact` | v7.0.1 | v7.0.1 ✅ |
| `actions/download-artifact` | v8.0.1 | v8.0.1 ✅ |
| `softprops/action-gh-release` | v3.0.0 | v3.0.0 ✅ |

### Pinned Build Tools — All Current ✅

| Tool | Version | Notes |
|------|---------|-------|
| Qt (CI) | 6.11.1 | Latest stable (6.11.2 targeted August 2026) |
| LLVM / clang-format / clang-tidy | 22.1.5 | Latest (Homebrew, no project footprint) |
| cppcheck | 2.20.0 | Latest |
| CMake | 4.3.2 | Latest (project minimum remains 3.21) |
| Doxygen | 1.17.0 | Latest (Doxyfile updated May 15) |
| linuxdeploy | 1-alpha-20251107-1 | Latest stable tag |
| linuxdeploy-plugin-qt | 1-alpha-20250213-1 | Latest stable tag |

### Quality Gates — All Passed ✅

- **build** — clean, no errors
- **coverage** — all tests pass
- **cppcheck** — zero warnings
- **format-check** — all files correctly formatted
