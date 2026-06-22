# Intentionally Untested Source Files

Generated on: 2026-06-22

This document classifies every `src/*.cpp` file (excluding `main.cpp`) that is not
included as a compilation unit in any test target in `tests/CMakeLists.txt`.

---

## Qt Widget / Dialog Classes (untested by design)

These classes subclass `QWidget`, `QDialog`, `QFrame`, or `QMainWindow`. They
require a real display (or a platform plugin such as `offscreen`) and contain
layout, painting, and user-interaction logic that is better verified through
UI-automation or integration tests. Unit-testing them adds fragile boilerplate
without meaningful coverage of business logic (which lives in the core/controller
layer).

| File | Reason |
|------|--------|
| `src/mainwindow.cpp` | Top-level `QMainWindow`; wires all panels and services together at startup. Pure integration surface. |
| `src/ui/batchprogresswidget.cpp` | `QWidget` that displays batch transfer progress bars. |
| `src/ui/configitemspanel.cpp` | `QWidget` listing device configuration items. |
| `src/ui/configpanel.cpp` | `QWidget` (config tab panel); renders configuration views. |
| `src/ui/connectionstatuswidget.cpp` | `QWidget` showing LED-style connection indicator. |
| `src/ui/drivestatuswidget.cpp` | `QWidget` showing mounted disk image names for drives A/B. |
| `src/ui/explorepanel.cpp` | `QWidget` (main Explore/Run panel); assembles sub-controllers and file browser. |
| `src/ui/filebrowserwidget.cpp` | `QTreeView`-based file browser widget. |
| `src/ui/filedetailspanel.cpp` | `QWidget` displaying details/preview for the selected file. |
| `src/ui/localfilebrowserwidget.cpp` | `QWidget` wrapping `QFileSystemModel` for local file browsing. |
| `src/ui/playlistwidget.cpp` | `QWidget` showing the current SID/MOD playlist. |
| `src/ui/preferencesdialog.cpp` | `QDialog` for application preferences. |
| `src/ui/remotefilebrowserwidget.cpp` | `QWidget` wrapping the remote file tree view. |
| `src/ui/streamingdiagnosticswidget.cpp` | `QWidget` showing streaming statistics / diagnostics overlay. |
| `src/ui/transferpanel.cpp` | `QWidget` (Transfer tab panel); hosts transfer queue and local browser. |
| `src/ui/transferprogresscontainer.cpp` | `QWidget` container for per-item transfer progress rows. |
| `src/ui/transferprogresswidget.cpp` | `QWidget` for a single transfer item progress row. |
| `src/ui/transferqueuewidget.cpp` | `QWidget` hosting the transfer queue list view. |
| `src/ui/videodisplaywidget.cpp` | `QWidget` that paints VIC-II video frames via `QPainter`. |
| `src/ui/viewpanel.cpp` | `QWidget` (View tab panel); hosts video display and streaming controls. |

---

## Thin Builder / Adapter Classes (untested by design)

These classes have no business logic. They either delegate every call directly
to a Qt API (builders that create menus/toolbars) or act as pure pass-through
adapters bridging two existing objects. There is nothing to assert beyond what
Qt itself guarantees.

| File | Reason |
|------|--------|
| `src/ui/menubarbuilder.cpp` | Stateless builder; constructs `QMenuBar` actions and wires them with `connect()`. All logic belongs to the targets (slots). |
| `src/ui/navigationviewadapter.cpp` | Two-line adapter; forwards `setPath()`/`setUpEnabled()` to `PathNavigationWidget` and reads `currentIndex()` from `QTreeView`. |
| `src/ui/systemtoolbarbuilder.cpp` | Stateless builder; populates a `QToolBar` and constructs a `ConnectionUIController`. |

---

## Modal Dialog Helpers (untested by design)

These classes wrap `QMessageBox::exec()` directly. The entire body is a blocking
modal dialog that requires a GUI event loop and human (or robot) interaction to
proceed. The decision logic (`OverwriteResponse`, `FolderExistsResponse`) is
trivially derived from which button the dialog returns — there is no
independently-testable computation.

| File | Reason |
|------|--------|
| `src/ui/transferconfirmationdialogs.cpp` | Both methods call `QMessageBox::exec()` synchronously; the only "logic" maps a clicked-button pointer to an enum value. |

---

## Gateway / Thin Shell Classes (untested by design)

These are imperative-shell classes that either call through to a real I/O layer
with no additional logic, or are platform stubs that replace a real
implementation on unsupported platforms.

| File | Reason |
|------|--------|
| `src/services/credentialstore_stub.cpp` | Platform stub that returns `false`/`QString()` unconditionally. Contains no logic. |
| `src/ui/connectiontester.cpp` | Creates a live `C64URestClient`, calls `getInfo()`, and shows a `QMessageBox` on completion. Pure I/O shell with no testable core logic; the real behaviour tested via `DeviceConnectionTest`. |

---

## Files That Could Have Unit Tests (future work)

All previously identified candidates have now been addressed — see notes below.

*(No remaining entries.)*

---

## Previously Addressed

| File | Resolution |
|------|------------|
| `src/ui/explorepanelcore.cpp` | **Tested** — `tests/test_explorepanelcore.cpp` covers both pure functions with 28 cases across all connected/disconnected states, all FileTypes, and drive display combinations. |
| `src/ui/explorefavoritescontroller.cpp` | **Tested** — `tests/test_explorefavoritescontroller.cpp` covers `onFavoriteSelected()`, `onToggleFavorite()`, `updateForPath()`, `onFavoritesChanged()`, and `isFavorite()` using a real `FavoritesService` with isolated QSettings. 21 tests. |
| `src/ui/explorecontextmenu.cpp` | **Tested** — `showForSelection()` called `QMenu::exec()` (blocking, untestable). A minimal refactor extracted `prepareMenu()` from `showForSelection()`. `tests/test_explorecontextmenu.cpp` tests action enablement state via `prepareMenu()`. 13 tests. |
| `src/services/remotefileoperations.cpp` | **Tested** — `tests/test_remotefileoperations.cpp` covers guard-clause behaviour for null FTP client. |
| `src/services/localfilesystemservice.cpp` | **Tested** — `tests/test_localfilesystemservice.cpp` exercises all 9 ILocalFileSystemService methods against a `QTemporaryDir`. |
| `src/ui/refreshpolicymanager.cpp` | **Tested** — `tests/test_refreshpolicymanager.cpp` covers suppressor RAII, `setConnected`, `isSuppressed`, and `refreshIfStale` policy logic. |
| `src/services/streamingservice.cpp` | **Tested** — `tests/test_streamingmanager.cpp` covers constructor wiring, all `startStreaming()` guard paths, `stopStreaming()`, stream command routing, and video format detection. The legacy `error(QString)` signal was retired; all error paths now emit only `errorReported` per the IErrorEmitter contract. |

---

## Summary

| Category | Count |
|----------|-------|
| Qt Widget / Dialog (untested by design) | 20 |
| Thin Builder / Adapter (untested by design) | 3 |
| Modal Dialog Helper (untested by design) | 1 |
| Gateway / Stub (untested by design) | 2 |
| **Files needing future tests** | **0** |
| **Total untested** | **26** |
| Tested source files | 88 |
| **Grand total source files** | **114** |
