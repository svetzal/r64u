Your dependencies are **fully up to date**. Here's the summary from `hone maintain`:

## Dependency Audit Results

All quality gates passed on the first attempt — no retries needed.

| Dependency | Status |
|---|---|
| Qt 6.11.0 (CI) | ✅ Current |
| CMake 4.3.2 | ✅ Current |
| cppcheck 2.20.0 | ✅ Current |
| LLVM 22.1.4 (clang-format/tidy) | ✅ Current |
| All GitHub Actions | ✅ Current |
| linuxdeploy + Qt plugin | ✅ Current |

### One manual item to consider

Your **local Qt installation is 6.10.1**, while CI builds against **6.11.0** (current stable). Qt 6.10.2 and 6.10.3 patch releases also exist. Upgrading requires running `~/Qt/MaintenanceTool.app` — it can't be scripted. Since CI already validates against 6.11.0, this is low urgency, but worth doing when convenient.

No code changes were made — everything was already at the latest compatible versions.