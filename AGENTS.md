# Agent Instructions

This project uses **Stride** for task management. Configuration is in `.stride.md` and `.stride_auth.md`.

## C++/Qt Development

**Use the `cpp-qt-craftsperson` agent** for all C++/Qt coding and testing tasks. This agent ensures:
- Memory safety and proper RAII patterns
- Modern C++ compliance (C++17/20)
- Qt best practices (signals/slots, object ownership, threading)
- CMake configuration quality
- Comprehensive testing with Qt Test

Invoke proactively after implementing features, before committing code, or when reviewing C++/Qt changes.

## Bug Fixing Workflow

**Always create a failing test before fixing a bug.** This ensures:
- The bug is reproducible and understood
- The fix actually addresses the problem
- The bug cannot regress in the future

**Workflow:**
1. **Reproduce** - Understand and document the bug behavior
2. **Write a failing test** - Create a test that demonstrates the bug (test should FAIL)
3. **Verify the test fails** - Run the test to confirm it captures the bug
4. **Fix the bug** - Implement the minimal fix
5. **Verify the test passes** - Run the test to confirm the fix works
6. **Run all tests** - Ensure no regressions

**Example:**
```cpp
// Test written BEFORE the fix - this should fail initially
void TestTransferQueue::testConnectionLostMidTransfer()
{
    // Setup: start a download
    queue->enqueueDownload("/remote/file.txt", localPath);
    mockFtp->mockProcessNextOperation();  // Start transfer

    // Simulate connection loss mid-transfer
    mockFtp->mockSimulateDisconnect();

    // Bug: item stays InProgress forever instead of failing
    // This test will FAIL until we fix the bug
    QCOMPARE(queue->data(index, StatusRole).toInt(),
             static_cast<int>(TransferItem::Status::Failed));
}
```

**If a test cannot be written** (e.g., pure UI timing issue), document why in the commit message and create a Stride task to add testability for that scenario.

## Build Environment

Qt 6.10.1 is installed via the native Qt installer at `~/Qt/`.

```bash
# Configure and build
cmake -B build -DCMAKE_PREFIX_PATH=~/Qt/6.10.1/macos
cmake --build build

# Run tests
ctest --test-dir build --output-on-failure

# Run the app
open build/r64u.app  # macOS
```

## Stride Workflow

```bash
# Get next available task
curl -H "Authorization: Bearer $STRIDE_API_TOKEN" \
  https://www.stridelikeaboss.com/api/tasks/next

# Claim a task (requires before_doing hook result)
curl -X POST -H "Authorization: Bearer $STRIDE_API_TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"before_doing_result": {"exit_code": 0, "output": "Ready", "duration_ms": 100}}' \
  https://www.stridelikeaboss.com/api/tasks/claim

# Complete a task (requires after_doing and before_review hook results)
curl -X PATCH -H "Authorization: Bearer $STRIDE_API_TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"after_doing_result": {...}, "before_review_result": {...}}' \
  https://www.stridelikeaboss.com/api/tasks/:id/complete
```

## Lifecycle Hooks

Hooks are defined in `.stride.md` and must be executed before API calls:

| Hook | When | Timeout |
|------|------|---------|
| before_doing | Before starting work | 60s |
| after_doing | After completing work | 120s |
| before_review | When entering review | 60s |
| after_review | After review approval | 60s |

## Landing the Plane (Session Completion)

**When ending a work session**, you MUST complete ALL steps below. Work is NOT complete until `git push` succeeds.

**MANDATORY WORKFLOW:**

1. **File tasks for remaining work** - Create Stride tasks for anything that needs follow-up
2. **Run quality gates** (if code changed) - Tests, linters, builds
3. **Update task status** - Complete finished work via Stride API
4. **PUSH TO REMOTE** - This is MANDATORY:
   ```bash
   git pull --rebase
   git push
   git status  # MUST show "up to date with origin"
   ```
5. **Clean up** - Clear stashes, prune remote branches
6. **Verify** - All changes committed AND pushed
7. **Hand off** - Provide context for next session

**CRITICAL RULES:**
- Work is NOT complete until `git push` succeeds
- NEVER stop before pushing - that leaves work stranded locally
- NEVER say "ready to push when you are" - YOU must push
- If push fails, resolve and retry until it succeeds

## Release Process

### Version Number Location

The **single source of truth** for the version number is in `CMakeLists.txt`:

```cmake
project(r64u
    VERSION X.Y.Z
    ...
)
```

This version automatically propagates to:
- `src/version.h` (generated at build time via `configure_file`)
- macOS `Info.plist` bundle version
- CPack package versions (DMG, DEB, RPM, NSIS)
- Application `--version` output

### Pre-Release Checklist

1. **Update version number** in `CMakeLists.txt`
   ```cmake
   project(r64u
       VERSION 0.5.1  # Update this line
       ...
   )
   ```

2. **Verify build succeeds**
   ```bash
   cmake -B build -DCMAKE_PREFIX_PATH=~/Qt/6.10.1/macos
   cmake --build build
   ```

3. **Run tests** (if any)
   ```bash
   ctest --test-dir build --output-on-failure
   ```

4. **Update CHANGELOG.md** (if maintained)

### Creating a Release

1. **Commit all changes** including the version bump:
   ```bash
   git add -A
   git commit -m "Release vX.Y.Z"
   ```

2. **Push to remote**:
   ```bash
   git push
   ```

3. **Create and push the tag**:
   ```bash
   git tag vX.Y.Z
   git push origin vX.Y.Z
   ```

The tag push triggers the GitHub Actions workflow which:
- Builds for all platforms (macOS, Linux, Windows)
- Signs and notarizes the macOS DMG
- Creates DEB/RPM packages for Linux
- Creates NSIS installer and ZIP for Windows
- Publishes all artifacts as a GitHub Release

### Version Numbering Convention

We use semantic versioning (MAJOR.MINOR.PATCH):
- **MAJOR**: Breaking changes or significant new functionality
- **MINOR**: New features, backward compatible
- **PATCH**: Bug fixes, minor improvements
