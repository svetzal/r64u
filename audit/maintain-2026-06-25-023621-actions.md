All quality gates pass cleanly. Here's the full dependency audit summary:

---

## Dependency Audit — r64u

### Result: All dependencies are already at their latest compatible versions

After auditing every dependency in the project, no updates were needed:

| Dependency | Current | Latest | Status |
|---|---|---|---|
| `actions/checkout` | v7.0.0 | v7.0.0 | ✅ Current |
| `jurplel/install-qt-action` | v4.3.1 | v4.3.1 | ✅ Current |
| `actions/upload-artifact` | v7.0.1 | v7.0.1 | ✅ Current |
| `actions/download-artifact` | v8.0.1 | v8.0.1 | ✅ Current |
| `softprops/action-gh-release` | v3.0.1 | v3.0.1 | ✅ Current |
| `linuxdeploy` | 1-alpha-20251107-1 | 1-alpha-20251107-1 | ✅ Current |
| `linuxdeploy-plugin-qt` | 1-alpha-20250213-1 | 1-alpha-20250213-1 | ✅ Current |
| Qt (CI) | 6.11.1 | 6.11.1 (latest in 6.11.x) | ✅ Current |

There are no vendored libraries, no FetchContent-managed dependencies, no npm/Python/Conan/vcpkg lock files — this is a lean project that depends only on Qt, zlib (system-provided), and GitHub Actions tooling.

### Quality Gates (all green)

| Gate | Result |
|---|---|
| Tests (`./build_test.sh`) | ✅ 383 passed, 0 failed |
| Format check | ✅ Clean |
| Cppcheck | ✅ Clean |
| Coverage | ✅ Passed |

No changes committed — nothing required updating.