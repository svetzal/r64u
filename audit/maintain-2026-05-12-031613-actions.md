All done. Here's a summary of what was assessed and updated:

**Assessed dependencies:**
| Dependency | Current | Latest | Action |
|---|---|---|---|
| Qt (CI) | 6.11.0 | **6.11.1** | ✅ Updated |
| Qt (local dev) | 6.10.1 | 6.10.3 available | ⏭ Skipped — requires Qt Maintenance Tool |
| `actions/checkout` | v6.0.2 | v6.0.2 | ✅ Already current |
| `install-qt-action` | v4.3.1 | v4.3.1 | ✅ Already current |
| `upload-artifact` | v7.0.1 | v7.0.1 | ✅ Already current |
| `download-artifact` | v8.0.1 | v8.0.1 | ✅ Already current |
| `action-gh-release` | v3.0.0 | v3.0.0 | ✅ Already current |
| linuxdeploy | 1-alpha-20251107-1 | 1-alpha-20251107-1 | ✅ Already current |
| linuxdeploy-plugin-qt | 1-alpha-20250213-1 | 1-alpha-20250213-1 | ✅ Already current |
| cppcheck | 2.20.0 | 2.20.0 | ✅ Already current |
| LLVM/clang-tidy | 22.1.4 (Homebrew) | 22.1.5 | ⏭ Skipped — system tool, not a project dependency |

**Change made:** `.github/workflows/build.yml` — CI Qt version `6.11.0 → 6.11.1` (patch release, published 07-May-2026). All 91 tests pass, `format-check` and `cppcheck` gates are green.