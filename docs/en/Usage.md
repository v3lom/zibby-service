# Usage

- CLI commands: `--help`, `--version`, `--daemon`, `--cli`
- Profile:
	- `--profile-show`
	- `--profile-set-name "Name"`
	- `--profile-set-bio "Bio"`
	- `--profile-set-status active|hidden|inactive`
- Messaging (CLI):
	- `--send --chat <id> --from <user> --to <user> --message "text"`
	- `--edit --id <message_id> --message "new text"`
	- `--read --id <message_id>`
	- `--history --chat <id> --limit <n>`
- Peer discovery:
	- `--discover --discover-timeout 1200`
	- `--peers`
	- `--peer-add host:port:name`
- Messenger TUI: `--tui`
- API protocol: local JSON bridge (placeholder in `src/api`)
