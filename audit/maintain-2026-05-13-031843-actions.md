All dependencies are already at their latest compatible versions. Here's a summary of what was audited:

| Dependency | Current | Status |
|---|---|---|
| `actions/checkout` | `v6.0.2` | ✅ Latest |
| `jurplel/install-qt-action` | `v4.3.1` | ✅ Latest |
| `actions/upload-artifact` | `v7.0.1` | ✅ Latest |
| `actions/download-artifact` | `v8.0.1` | ✅ Latest |
| `softprops/action-gh-release` | `v3.0.0` | ✅ Latest |
| `linuxdeploy` | `1-alpha-20251107-1` | ✅ Latest stable |
| `linuxdeploy-plugin-qt` | `1-alpha-20250213-1` | ✅ Latest stable |
| Qt in CI (`QT_VERSION`) | `6.11.1` | ✅ Latest (released May 12) |

**One documentation fix was applied**: `AGENTS.md` still said "CI downloads Qt 6.11.0" but the workflow had already been bumped to `6.11.1` in a prior commit — the note is now in sync.

Qt 6.11.1 also carries a fix for **CVE-2026-6210** (qtsvg), which is a nice security benefit of staying on the latest patch.