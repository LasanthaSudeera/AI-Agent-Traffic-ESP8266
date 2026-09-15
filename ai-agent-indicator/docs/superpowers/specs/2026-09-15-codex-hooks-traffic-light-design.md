# Codex Hooks Traffic-Light Design

## Goal

Automate the existing Codex traffic-light contract with global Codex lifecycle hooks, while preserving the setup request as a reusable, inactive repository instruction.

## Scope

- Install the reporter at `/home/lasantha/.codex/hooks/traffic-light.sh`.
- Install global lifecycle configuration at `/home/lasantha/.codex/hooks.json` so it applies in every project.
- Preserve the setup prompt at `docs/agent-instructions/setup-codex-traffic-light-hooks.md`.
- Do not modify `/home/lasantha/.codex/AGENTS.md`, the repository `AGENTS.md`, firmware, or Wi-Fi credentials.

## State mapping

| State | Codex event | Reason |
| --- | --- | --- |
| `working` | `UserPromptSubmit` | Fires when a new turn begins. |
| `working` | `PostToolUse` | Restores work state synchronously after a tool or approval flow completes. |
| `permission` | `PermissionRequest` | Fires immediately before Codex asks for tool approval. |
| `blocked` | `PreToolUse` matching `^request_user_input$` | Represents Codex pausing for structured user guidance. |
| `ready` | `Stop` | Fires as the turn stops and the final response is delivered. |
| `ready` | `Interrupt` | Clears work state when an active turn is cancelled. |
| `ready` | `SessionEnd` | Cleans up when the main session closes. |

Codex 0.154.0 has no general lifecycle event meaning “the final response asks the user for guidance.” The `blocked` mapping is therefore exact only when Codex uses the structured `request_user_input` tool. Plain-text questions reach `Stop` and report `ready`; no message-text heuristic will be added because false blocked states would make the indicator unreliable.

## Reporter design

The reporter retains the Claude reference implementation's token-owned detached refresher:

- `working` posts immediately, writes the ownership token in the parent process, and then spawns a detached loop.
- The loop sleeps 60 seconds, verifies ownership, and reposts `working` for at most two hours.
- `blocked`, `permission`, and `ready` delete the token before posting and terminate the matching refresher process.
- Requests are bounded and best-effort.
- Every invocation, including invalid input and local filesystem or network failures, exits with status zero.

The `PostToolUse` hook is deliberately synchronous. Codex waits for it before continuing the agentic loop, so it cannot finish after a later `Stop` hook and recreate a refresher after `ready`.

## Trust and verification

Codex records trust against each hook definition's hash. Installation will use the interactive `/hooks` review flow twice: first for temporary sentinel handlers and again for the final definitions. The stale `PosSystem` trust entry will not be reused and the trust bypass flag will not be used.

Verification has three layers:

1. An isolated, shortened reporter copy uses a stub `curl` to prove repeated `working` posts and prove `blocked` prevents every later refresh.
2. Temporary global sentinel hooks append timestamps and event metadata to a log while disposable Codex sessions trigger lifecycle events. The sentinel configuration and files are removed afterward.
3. The final hook configuration is reviewed and trusted, then syntax, absolute paths, synchronous `PostToolUse`, agent name, and installation permissions are checked.
