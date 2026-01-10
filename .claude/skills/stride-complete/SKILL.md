---
name: stride-complete
description: Complete a task after running after_doing and before_review hooks
---

# Stride: Complete Task

Complete a task after executing after_doing and before_review hooks.

## Usage
Run `/stride-complete <task_id>` to complete a claimed task.

## Instructions

1. Verify all implementation work is done and committed.

2. Execute the `after_doing` hook from `.stride.md`:
   - Set environment variables: `TASK_IDENTIFIER` and `TASK_TITLE`
   - Run the bash commands in the `## after_doing` section (builds and tests)
   - Capture: exit_code, output, and duration_ms
   - If exit code is non-zero, fix the issues before proceeding

3. Execute the `before_review` hook from `.stride.md`:
   - Run the bash commands in the `## before_review` section
   - Capture: exit_code, output, and duration_ms
   - If exit code is non-zero, resolve before proceeding

4. Complete the task with both hook proofs:

```bash
TOKEN=$(grep "API Token" .stride_auth.md | sed 's/.*`\(.*\)`.*/\1/')
TASK_ID="<task_id>"
curl -s -X PATCH -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "after_doing_result": {"exit_code": 0, "output": "...", "duration_ms": 123},
    "before_review_result": {"exit_code": 0, "output": "...", "duration_ms": 456}
  }' \
  "https://www.stridelikeaboss.com/api/tasks/$TASK_ID/complete"
```

5. Check the response for `needs_review`:
   - If `true`: Inform user the task awaits human review
   - If `false`: Proceed to execute `after_review` hook and offer to claim next task

## Critical
- Both hooks MUST succeed before completing
- Build and tests must pass (after_doing hook)
- All changes must be committed and pushed
