# Codex Hooks Traffic-Light Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Install and trust global Codex hooks that automatically report traffic-light lifecycle states, verify their timing behavior, and preserve the setup request as an inactive repository instruction.

**Architecture:** A fail-open Bash reporter owns a detached 60-second refresher through a token file. Global `~/.codex/hooks.json` handlers invoke it by absolute path for turn, approval, structured-question, tool-completion, stop, interruption, and session-end events. Temporary sentinel handlers and an isolated curl stub verify hook delivery and refresher ordering without using the live endpoint for the refresher test.

**Tech Stack:** Bash, Codex CLI 0.154.0 lifecycle hooks, JSON, `curl`, `setsid`, `pgrep`

## Global Constraints

- Do not edit `/home/lasantha/.codex/AGENTS.md` or repository `AGENTS.md`.
- Report to `http://ai-agent-status.local/api/status` as agent `codex`.
- `working` must refresh every 60 seconds because server state expires after 120 seconds.
- Every non-working state must revoke the refresher before posting.
- The parent must write the refresher token before spawning the child.
- `PostToolUse` must remain synchronous.
- Every configured hook command must use `/home/lasantha/.codex/hooks/traffic-light.sh` rather than a relative path.
- The reporter must exit zero on every path.
- Trust hooks through `/hooks`; never use `--dangerously-bypass-hook-trust`.
- Do not stage, commit, print, or overwrite `ai-agent-indicator.ino`.

---

### Task 1: Preserve the reusable setup instruction

**Files:**
- Create: `docs/agent-instructions/setup-codex-traffic-light-hooks.md`
- Test: shell assertions from the repository root

**Interfaces:**
- Consumes: the requested contract, reference location, race constraints, trust requirements, and verification requirements.
- Produces: an inactive Markdown prompt suitable for a future Codex session.

- [ ] **Step 1: Verify the prompt does not exist yet**

Run:

```sh
test ! -e docs/agent-instructions/setup-codex-traffic-light-hooks.md
```

Expected: exit 0 before creation.

- [ ] **Step 2: Create the reusable instruction**

Create `docs/agent-instructions/setup-codex-traffic-light-hooks.md` with the user's complete setup request, followed by an implementation note documenting the verified Codex 0.154.0 mapping and the `request_user_input` limitation.

- [ ] **Step 3: Verify required clauses are preserved**

Run:

```sh
rg -n 'traffic-light\.sh|working \| blocked \| permission \| ready|120 seconds|every 60s|BEFORE the child spawns|synchronous|absolute script path|Exit 0|hooks\.json|trusted_hash|sentinel|curl stubbed' docs/agent-instructions/setup-codex-traffic-light-hooks.md
```

Expected: every requested contract and verification topic has a match.

### Task 2: Build the reporter test-first

**Files:**
- Create temporarily: `/tmp/codex-traffic-light-setup/traffic-light.sh`
- Create temporarily: `/tmp/codex-traffic-light-setup/curl`
- Test output: `/tmp/codex-traffic-light-setup/posts.log`

**Interfaces:**
- Consumes: one argument, `working`, `blocked`, `permission`, or `ready`.
- Produces: best-effort JSON posts and, for `working`, a token-owned detached refresher.

- [ ] **Step 1: Create the curl stub and shortened failing test fixture**

The stub appends `date +%s%N` and all curl arguments to `posts.log`. Copy the candidate reporter beside it, change only `REFRESH_SECS=60` to `REFRESH_SECS=1`, `MAX_REFRESH_SECS=7200` to `MAX_REFRESH_SECS=10`, and prepend the stub directory to `PATH`.

- [ ] **Step 2: Prove the test detects missing refresh behavior**

Run the test against a fixture whose detached-spawn line is disabled.

Expected: after 2.5 seconds, `posts.log` contains exactly one `working` request and the repeated-post assertion fails.

- [ ] **Step 3: Implement the reporter from the Claude reference**

Copy `/home/lasantha/.claude-work/hooks/traffic-light.sh`, update its comments and `AGENT='codex'`, and change the invalid-argument branch to print usage and exit zero. Preserve token creation before `setsid`, token deletion before non-working posts, ownership checks before refresh posts, 60-second production interval, and the two-hour ceiling.

- [ ] **Step 4: Prove repeated refresh and stop ordering**

Run `working`, wait 2.5 seconds, assert at least three `working` posts, run `blocked`, wait 2.5 seconds, and assert the log has one `blocked` post with no later `working` post.

Expected: all assertions pass and the reporter returns zero for all valid states, missing input, and invalid input.

### Task 3: Prove Codex lifecycle events with temporary sentinels

**Files:**
- Create temporarily: `/home/lasantha/.codex/hooks.json`
- Create temporarily: `/tmp/codex-hook-sentinel.sh`
- Test output: `/tmp/codex-hook-sentinel.log`

**Interfaces:**
- Consumes: hook JSON on standard input.
- Produces: timestamped `hook_event_name` and `tool_name` records without influencing Codex decisions.

- [ ] **Step 1: Back up any existing global hooks file without altering `AGENTS.md`**

Check whether `/home/lasantha/.codex/hooks.json` exists. If present, copy it to a uniquely named file in `/tmp` and merge rather than discard its definitions.

- [ ] **Step 2: Install sentinel handlers for every planned event**

Configure `UserPromptSubmit`, `PermissionRequest`, `PreToolUse` with `^request_user_input$`, `PostToolUse`, `Stop`, `Interrupt`, and `SessionEnd` to invoke `/tmp/codex-hook-sentinel.sh` synchronously.

- [ ] **Step 3: Review and trust the sentinel definitions**

Start the Codex CLI, open `/hooks`, inspect every source and command, and trust the exact definitions. Do not use the trust-bypass flag and do not copy the stale `PosSystem` hash.

- [ ] **Step 4: Trigger and record lifecycle events**

Use disposable Codex sessions to submit a prompt, execute a harmless tool, request an approval, invoke structured user input where the CLI supports it, stop normally, interrupt active work, and close a session.

Expected: the sentinel log proves each triggered handler by timestamp and event name. Any event the installed client cannot produce is explicitly classified as reasoned rather than proven.

- [ ] **Step 5: Remove the sentinel configuration and temporary files**

Stop all disposable sessions, restore the pre-test global configuration if one existed, and remove only `/tmp/codex-hook-sentinel.sh` and `/tmp/codex-hook-sentinel.log` after recording the evidence.

### Task 4: Install and trust the final global hooks

**Files:**
- Create: `/home/lasantha/.codex/hooks/traffic-light.sh`
- Create or merge: `/home/lasantha/.codex/hooks.json`

**Interfaces:**
- Consumes: Codex lifecycle events.
- Produces: traffic-light state posts through the absolute reporter path.

- [ ] **Step 1: Install the tested reporter with executable permissions**

Install the production candidate as `/home/lasantha/.codex/hooks/traffic-light.sh` with mode `0755` and verify `bash -n` succeeds.

- [ ] **Step 2: Install the final global mapping**

Configure synchronous command handlers for `UserPromptSubmit -> working`, `PermissionRequest -> permission`, `PreToolUse(^request_user_input$) -> blocked`, `PostToolUse -> working`, `Stop -> ready`, `Interrupt -> ready`, and `SessionEnd -> ready`. Set timeouts to six seconds except `Interrupt` and `SessionEnd`, whose release limits are three seconds.

- [ ] **Step 3: Review and trust the final definitions**

Restart Codex, open `/hooks`, inspect the changed hashes and absolute commands, and trust them interactively. Confirm Codex no longer reports the global hooks as needing review.

- [ ] **Step 4: Verify the installed contract**

Run read-only checks that prove the agent is `codex`, interval is 60, production endpoint uses the hostname, every command path is absolute, `PostToolUse` has no `async: true`, reporter mode is executable, and missing/invalid arguments exit zero.

Expected: every check passes without posting test refresh traffic to the live endpoint.

### Task 5: Final repository and evidence checks

**Files:**
- Verify: `docs/agent-instructions/setup-codex-traffic-light-hooks.md`
- Verify: `/home/lasantha/.codex/hooks/traffic-light.sh`
- Verify: `/home/lasantha/.codex/hooks.json`

**Interfaces:**
- Consumes: the completed installation and captured test observations.
- Produces: a concise handoff distinguishing proved hook events from reasoned mappings.

- [ ] **Step 1: Check repository isolation**

Run `git status --short` and `git diff --check` for new documentation. Confirm `ai-agent-indicator.ino` remains unstaged and untouched.

- [ ] **Step 2: Check temporary artifacts are gone**

Confirm the sentinel handler/log and isolated refresher processes are absent. Preserve only the final reporter, global hook configuration, repository instruction, design, and implementation plan.

- [ ] **Step 3: Report evidence**

List the hook events observed in the sentinel log, events only supported by documentation or reasoning, isolated refresher post counts, proof that no `working` post followed `blocked`, global hook scope, trust completion, and the structured-input limitation.
