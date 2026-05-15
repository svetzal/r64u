All done. Here's a summary of the dependency audit for this C++/Qt project:

## Dependency Update Summary

**All GitHub Actions pinned versions — already at latest:**
| Action | Current | Status |
|--------|---------|--------|
| `actions/checkout` | v6.0.2 | ✅ Latest |
| `jurplel/install-qt-action` | v4.3.1 | ✅ Latest |
| `actions/upload-artifact` | v7.0.1 | ✅ Latest |
| `actions/download-artifact` | v8.0.1 | ✅ Latest |
| `softprops/action-gh-release` | v3.0.0 | ✅ Latest |

**Other pinned dependencies — already at latest:**
| Dependency | Current | Status |
|-----------|---------|--------|
| Qt (CI) | 6.11.1 | ✅ Latest stable |
| linuxdeploy | 1-alpha-20251107-1 | ✅ Latest stable |
| linuxdeploy-plugin-qt | 1-alpha-20250213-1 | ✅ Latest stable |
| cppcheck | 2.20.0 | ✅ Up to date |

**Updated:**
| Tool | From | To | Change |
|------|------|----|--------|
| Doxygen + Doxyfile | 1.16.1 | 1.17.0 | Removed obsolete `DOT_MULTI_TARGETS`, added `DOT_BATCH_SIZE` and Mermaid CLI config options |

**Quality gates result:** 102/102 tests pass, cppcheck clean, clang-format clean, Doxygen generates without errors. ✅