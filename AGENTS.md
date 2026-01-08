# Agent Instructions

This project uses **Stride** for task management. Configuration is in `.stride.md` and `.stride_auth.md`.

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
