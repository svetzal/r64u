All dependencies are already at their latest compatible versions. Here's a summary of the audit:

## Dependency Maintenance — No Updates Required

**GitHub Actions** (all current):
- `actions/checkout` → v6.0.2 ✅
- `jurplel/install-qt-action` → v4.3.1 ✅
- `actions/upload-artifact` → v7.0.1 ✅
- `actions/download-artifact` → v8.0.1 ✅
- `softprops/action-gh-release` → v3.0.0 ✅

**Pinned build tool versions** (all current):
- Qt CI: 6.11.1 (latest stable; 6.11.2 not due until August 2026)
- LLVM/clang 22.1.5, cppcheck 2.20.0, CMake 4.3.2, Doxygen 1.17.0
- linuxdeploy and linuxdeploy-plugin-qt at their latest stable tags

**Quality gates**: build ✅ · coverage ✅ · cppcheck ✅ · format-check ✅

The previous maintenance run (May 16) was thorough and covered all updates. The project is clean with no pending dependency work until Qt 6.11.2 drops in August 2026.