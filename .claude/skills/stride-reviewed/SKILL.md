---
name: stride-reviewed
description: Finalize a task after human review approval by running after_review hook
---

# Stride: Mark Task Reviewed

Finalize a task after human review approval.

## Usage
Run `/stride-reviewed <task_id>` after a task has been approved by a human reviewer.

## Instructions

1. Confirm the task has been approved by a human reviewer.

2. Execute the `after_review` hook from `.stride.md`:
   - Set environment variables: `TASK_IDENTIFIER` and `TASK_TITLE`
   - Run the bash commands in the `## after_review` section
   - Capture: exit_code, output, and duration_ms
   - If exit code is non-zero, resolve before proceeding

3. Mark the task as reviewed:

```bash
TOKEN=$(grep "API Token" .stride_auth.md | sed 's/.*`\(.*\)`.*/\1/')
TASK_ID="<task_id>"
curl -s -X PATCH -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"after_review_result": {"exit_code": 0, "output": "...", "duration_ms": 123}}' \
  "https://www.stridelikeaboss.com/api/tasks/$TASK_ID/mark_reviewed"
```

4. Confirm the task is fully complete.

5. Offer to fetch the next available task with `/stride-next`.

## Critical
- Only run this after human approval is confirmed
- The after_review hook must succeed
- This is the final step in the task lifecycle
