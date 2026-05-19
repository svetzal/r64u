## Dependency Audit Results — 2026-05-19

### GitHub Actions (`.github/workflows/build.yml`)
All actions are already at the latest versions:
| Action | Version | Status |
|--------|---------|--------|
| `actions/checkout` | v6.0.2 | ✅ Current (latest) |
| `jurplel/install-qt-action` | v4.3.1 | ✅ Current (latest) |
| `actions/upload-artifact` | v7.0.1 | ✅ Current (latest) |
| `actions/download-artifact` | v8.0.1 | ✅ Current (latest) |
| `softprops/action-gh-release` | v3.0.0 | ✅ Current (latest) |

### Linux Packaging Tools (pinned in workflow)
| Tool | Version | Status |
|------|---------|--------|
| `linuxdeploy` | `1-alpha-20251107-1` | ✅ Current (latest stable) |
| `linuxdeploy-plugin-qt` | `1-alpha-20250213-1` | ✅ Current (latest stable) |

### Homebrew Tools
| Package | Version | Status |
|---------|---------|--------|
| `cmake` | 4.3.2 | ✅ Current |
| `cppcheck` | 2.20.0 | ✅ Current |
| `llvm` | 22.1.5 | ✅ Current |
| `doxygen` | 1.17.0 | ✅ Current |

### Qt Framework
- **CI**: Qt 6.11.1 ✅ (latest Qt 6.x, downloaded fresh per run)
- **Local**: Qt 6.10.1 (full installation) — Qt 6.11.0 is installed locally but only contains the WebEngine add-on

### Quality Gates
All gates pass on the current codebase:
- **coverage**: All tests pass, coverage report generated ✅
- **cppcheck**: No warnings ✅
- **format-check**: All files correctly formatted ✅
- **doxygen**: Documentation generated without errors ✅

**No dependency updates were needed.** The project is fully up to date.
