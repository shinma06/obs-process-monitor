---
name: start-work
description: Begin or resume an Issue-scoped implementation, documentation or configuration change with ownership and acceptance tracking. Not for read-only advice or review.
---

# Start work

Read AGENTS.md, docs/project.md and docs/workflow.md from the repository root. Reuse unchanged context. For instructions/Skills also read docs/context.md.

Inspect the Issue/comments, related PRs, ownership, dependencies, branch, worktrees and dirty state. Search before creating an Issue. Claim scope, owner, base, target, reviewer, GUI need and next action, then read back. Unreleased claims do not expire. Resume in your existing worktree; take over only after reassignment and old-writer stop, in a separate worktree.

Fetch without disturbing edits. Use origin/develop for product work and origin/main for GUI-free tooling, following docs/workflow.md. Create `agent|codex|claude|cursor/<issue>-<slug>` in a dedicated worktree. Remove an inherited upstream to the integration branch. Install hooks with `python scripts/bootstrap.py` (Python 3.11+) after inspecting them; preserve custom hooks on conflict.

Record observable acceptance, real project checks and remaining GUI/release conditions in the Issue and docs/verification/changes/issue-N.json. Create a Draft PR on the first meaningful push with Issue, Integration, Verification, GUI and GUI reason. Continue through finish-work; setup or a Draft PR alone is not completion. Referencing agents, server settings or GUI capabilities does not authorize their use.
