# Codex Traffic-Light Instructions Design

## Goal

Configure Codex to report its task state to the existing ESP8266 traffic-light API while providing an inactive template that can be copied and customized for other agents.

## Files

- `AGENTS.md` is the active, project-local Codex instruction file.
- `AGENTS.example.md` is an inactive sample with the same initial content.

Keeping the files independent avoids Windows symlink issues and makes the sample easy to customize without changing active Codex behavior.

## Behavior

Both files use `http://ai-agent-status.local/api/status` and identify the sender as `codex`. They instruct the agent to:

- send `working` before beginning task work;
- refresh `working` approximately every 60 seconds during long work;
- send `blocked` when user guidance is required;
- send `permission` before requesting permission;
- send `ready` before the final response or when work is cancelled; and
- continue the main task if the indicator cannot be reached.

The files include a concise `curl` command showing the required JSON request. They do not change firmware, networking, or API behavior. The hostname is supplied by the user's hosts configuration and has already been verified from WSL.

## Validation

- Confirm both files contain the same traffic-light section.
- Confirm neither file contains the former numeric device address.
- Confirm the active file is named exactly `AGENTS.md` and the template is named exactly `AGENTS.example.md`.
- Keep the local Wi-Fi credential modification in `ai-agent-indicator.ino` unstaged and uncommitted.
