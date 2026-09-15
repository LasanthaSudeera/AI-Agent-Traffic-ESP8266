# Codex Traffic-Light Instructions Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add active Codex instructions and a matching inactive template that report task status to the ESP8266 traffic light.

**Architecture:** Two independent Markdown files contain identical project-local guidance. Codex automatically loads `AGENTS.md`; `AGENTS.example.md` remains an inert copy for customization. Each status transition is a bounded, fail-open HTTP POST to the hostname already configured by the user.

**Tech Stack:** Codex `AGENTS.md`, Markdown, `curl`, ESP8266 JSON HTTP API

## Global Constraints

- Use `http://ai-agent-status.local/api/status`; do not embed a numeric device address.
- Identify the sender as `codex`, within the API's 32-byte agent-name limit.
- Map `working` to red, `blocked` and `permission` to yellow, and `ready` to green.
- Refresh `working` approximately every 60 seconds during long work because device state expires after 120 seconds.
- Indicator failures must not interrupt the user's main task.
- Do not modify firmware or stage the local Wi-Fi credential change in `ai-agent-indicator.ino`.

---

### Task 1: Add active and example agent instructions

**Files:**
- Create: `AGENTS.md`
- Create: `AGENTS.example.md`
- Test: shell assertions executed from the project directory

**Interfaces:**
- Consumes: ESP8266 endpoint `POST http://ai-agent-status.local/api/status` with JSON fields `state` and `agent`.
- Produces: active Codex task-state instructions and an identical inactive template.

- [ ] **Step 1: Run the file contract before implementation**

Run:

```sh
test -f AGENTS.md && test -f AGENTS.example.md && cmp -s AGENTS.md AGENTS.example.md
```

Expected: FAIL because both instruction files do not exist yet.

- [ ] **Step 2: Create both instruction files with identical content**

Create `AGENTS.md` and `AGENTS.example.md` with this exact content:

````markdown
# AI Agent Traffic Light

Report Codex task state to `http://ai-agent-status.local/api/status` using the agent name `codex`.

## State updates

Before beginning task work, send `working`:

```sh
curl -fsS --max-time 5 -X POST -H 'Content-Type: application/json' --data '{"state":"working","agent":"codex"}' http://ai-agent-status.local/api/status >/dev/null || true
```

During long-running work, refresh `working` approximately every 60 seconds.

When progress cannot continue without user guidance, send `blocked`:

```sh
curl -fsS --max-time 5 -X POST -H 'Content-Type: application/json' --data '{"state":"blocked","agent":"codex"}' http://ai-agent-status.local/api/status >/dev/null || true
```

Before requesting user permission, send `permission`:

```sh
curl -fsS --max-time 5 -X POST -H 'Content-Type: application/json' --data '{"state":"permission","agent":"codex"}' http://ai-agent-status.local/api/status >/dev/null || true
```

Immediately before the final response, or when work is cancelled, send `ready`:

```sh
curl -fsS --max-time 5 -X POST -H 'Content-Type: application/json' --data '{"state":"ready","agent":"codex"}' http://ai-agent-status.local/api/status >/dev/null || true
```

Traffic-light requests are best-effort. A connection or HTTP failure must not interrupt the user's main task. This device currently uses last-writer-wins behavior, so concurrent agents may replace each other's displayed state.
````

- [ ] **Step 3: Validate filenames, equality, hostname, and state coverage**

Run:

```sh
test -f AGENTS.md
test -f AGENTS.example.md
cmp -s AGENTS.md AGENTS.example.md
test "$(rg -o 'http://ai-agent-status\.local/api/status' AGENTS.md | wc -l)" -eq 5
test "$(rg -o '\"state\":\"(working|blocked|permission|ready)\"' AGENTS.md | sort -u | wc -l)" -eq 4
! rg -n '192\.168\.[0-9]+\.[0-9]+' AGENTS.md AGENTS.example.md
```

Expected: all commands exit 0. The hostname appears once in the introduction and once in each of four commands; all four states are present; neither file contains a numeric private-network address.

- [ ] **Step 4: Verify the live hostname without changing device state**

Run:

```sh
curl -fsS --max-time 5 http://ai-agent-status.local/api/status
```

Expected: HTTP success with a JSON object containing `state`, `agent`, and `expires_in_seconds`.

- [ ] **Step 5: Confirm credential isolation and commit only the instruction files**

Run:

```sh
git status --short
git diff --check -- AGENTS.md AGENTS.example.md
git add AGENTS.md AGENTS.example.md
git diff --cached --check
git diff --cached --name-only
git commit -m "docs: configure Codex traffic light status"
```

Expected: the staged-name output lists only `ai-agent-indicator/AGENTS.md` and `ai-agent-indicator/AGENTS.example.md`; the existing `ai-agent-indicator.ino` credential edit remains unstaged.
