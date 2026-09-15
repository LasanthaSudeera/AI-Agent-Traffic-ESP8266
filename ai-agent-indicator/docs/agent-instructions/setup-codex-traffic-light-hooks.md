# Set Up Codex Traffic-Light Hooks

Use the following instruction in a Codex session to install and verify automatic traffic-light reporting. This file is documentation only; it is not an active `AGENTS.md` file.

## Agent instruction

Set up an AI Agent Traffic Light for yourself using Codex hooks, so status is reported automatically instead of depending on you remembering to run curl.

### Contract

`~/.codex/AGENTS.md` defines the states and the endpoint. Read it first. Report as agent `codex`. Do not edit `AGENTS.md`; automate what it describes.

### Reference implementation

A working version already exists for Claude Code at:

```text
/home/lasantha/.claude-work/hooks/traffic-light.sh
```

Read it. Copy it to `~/.codex/hooks/traffic-light.sh`, change `AGENT` to `codex`, and keep the rest of the design. It takes one argument:

```text
working | blocked | permission | ready
```

### Design constraints

Each constraint below fixes a real failure mode; do not simplify it away.

1. The server expires a state after 120 seconds. Its POST reply shows `expires_in_seconds`. `working` must therefore start a detached refresher that re-posts every 60 seconds, or the light dies during exactly the long build it is meant to signal.
2. Every non-working state must revoke the refresher before posting, or a live refresher overwrites `blocked`/`permission` with `working` 60 seconds later and the light lies.
3. The refresher's ownership token must be written by the parent before the child spawns. If the child writes its own PID, a stop arriving milliseconds later sees nothing to stop and the refresher survives.
4. Whatever hook returns the state to `working` after a tool completes must be synchronous. If it can land after the end-of-turn `ready` hook, it re-creates the token and pins the light on `working` for hours.
5. Hooks run from an unspecified current working directory: use the absolute script path.
6. The script must never fail a hook. Exit 0 on every path.

### Discover the Codex hook model

Determine the installed Codex hook event names from official documentation or local help and map them to the four states:

- `working`: starting work on a turn
- `permission`: before asking the user to approve something
- `blocked`: cannot proceed without user guidance
- `ready`: immediately before the final response and when work is cancelled

A stale `[hooks.state]` entry in `~/.codex/config.toml` may mention `pre_tool_use`; do not assume its spelling is current. Determine whether global `~/.codex/hooks.json` is supported or hooks are only per-project. If hooks are per-project only, state that clearly rather than configuring only one project silently.

### Trust

Codex hashes each hook and requires it to be trusted. Complete the trust step properly. Do not use `--dangerously-bypass-hook-trust`.

The existing `config.toml` may contain a stale `trusted_hash` for `PosSystem/.codex/hooks.json`, whose `.codex` directory is empty. Do not reuse that stale hash.

### Verification

- Prove each hook actually fires. A temporary sentinel command appending a timestamp to a file is sufficient; remove it afterward.
- Test the refresher on an isolated copy with the interval shortened and `curl` stubbed to a log file. Confirm it posts repeatedly, then confirm that sending `blocked` stops it and produces no further `working` post.
- Do not test the refresher against the live endpoint while the installed hooks are firing, because hook-generated `working` posts confound the result.
- Report which hooks were proved to fire and which were supported only by documentation or reasoning.

## Verified mapping for Codex CLI 0.154.0

- `UserPromptSubmit` → `working`
- `PermissionRequest` → `permission`
- `PreToolUse` matching `^request_user_input$` → `blocked`
- synchronous `PostToolUse` → `working`
- `Stop` → `ready`
- `Interrupt` → `ready`
- `SessionEnd` → `ready`

Codex 0.154.0 supports the global `~/.codex/hooks.json` location. It does not expose a general event that distinguishes a plain-text final response requesting guidance from any other final response. Consequently, `blocked` is automatic when the structured `request_user_input` tool is used; a plain-text question reaches `Stop` and reports `ready`. Do not add message-text heuristics unless the user explicitly accepts possible false states.
