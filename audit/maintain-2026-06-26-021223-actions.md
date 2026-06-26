All done. Here's a summary of the dependency update run for this C++/Qt project:

## Dependency Update Summary

### Homebrew Tooling (system-level, not in repo)
| Tool | Before | After |
|------|--------|-------|
| CMake | 4.3.3 | **4.3.4** ✅ |
| LLVM/clang | 22.1.7_1 | **22.1.8** ✅ |
| cppcheck | 2.21.0 | 2.21.0 (already current) |

### GitHub Actions (`.github/workflows/build.yml`)
All already at latest versions — no changes needed:
- `actions/checkout@v7.0.0` ✅
- `jurplel/install-qt-action@v4.3.1` ✅
- `actions/upload-artifact@v7.0.1` ✅
- `actions/download-artifact@v8.0.1` ✅
- `softprops/action-gh-release@v3.0.1` ✅

### Qt
- **Local full install**: Qt 6.10.1 (only full install available; other Qt versions locally are WebEngine-only add-ons)
- **CI**: Qt 6.11.1 — already the latest Qt 6.11.x release

### Documentation Fix
- Corrected `AGENTS.md`: the local WebEngine-only Qt add-on is `6.11.0`, not `6.11.1` as previously written

### Quality Gates (all pass after updates)
- ✅ 383 tests passed
- ✅ `format-check` — no formatting issues with clang-format 22.1.8
- ✅ `cppcheck` — no warnings with cppcheck 2.21.0