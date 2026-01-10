---
name: stride-claim
description: Claim a task after running the before_doing hook and begin work
---

# Stride: Claim Task

Claim the next available task after executing the before_doing hook.

## Usage
Run `/stride-claim` to claim a task and begin work.

## Instructions

1. First, fetch the next task using the Stride API to confirm one is available:

```bash
TOKEN=$(grep "API Token" .stride_auth.md | sed 's/.*`\(.*\)`.*/\1/')
curl -s -H "Authorization: Bearer $TOKEN" \
  https://www.stridelikeaboss.com/api/tasks/next
```

2. If a task is available, extract task identifier and title from the response.

3. Execute the `before_doing` hook from `.stride.md`:
   - Set environment variables: `TASK_IDENTIFIER` and `TASK_TITLE`
   - Run the bash commands in the `## before_doing` section
   - Capture: exit_code, output, and duration_ms
   - If exit code is non-zero, stop and report the error

4. Claim the task with hook proof:

```bash
TOKEN=$(grep "API Token" .stride_auth.md | sed 's/.*`\(.*\)`.*/\1/')
curl -s -X POST -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"before_doing_result": {"exit_code": 0, "output": "...", "duration_ms": 123}}' \
  https://www.stridelikeaboss.com/api/tasks/claim
```

5. Present the claimed task details and begin implementation work.

## Critical
- The before_doing hook MUST succeed before claiming
- Include actual hook output in the API request
- Track the task ID for later completion
