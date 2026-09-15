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
