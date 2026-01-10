---
name: stride-next
description: Fetch the next available task from Stride task queue
---

# Stride: Get Next Task

Get the next available task from Stride.

## Usage
Run `/stride-next` to fetch the next task matching your capabilities.

## Instructions

1. Find the `.stride_auth.md` file in the current project directory
2. Extract the API token from the line containing `**API Token:**`
3. Execute this curl command:

```bash
TOKEN=$(grep "API Token" .stride_auth.md | sed 's/.*`\(.*\)`.*/\1/')
curl -s -H "Authorization: Bearer $TOKEN" \
  https://www.stridelikeaboss.com/api/tasks/next
```

4. Parse the JSON response and present the task details:
   - Task identifier and title
   - Description and acceptance criteria
   - Key files to modify
   - Testing strategy
   - Any patterns to follow or pitfalls to avoid

5. If no task is available, inform the user that the queue is empty.
