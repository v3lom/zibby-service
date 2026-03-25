# Использование

- CLI-команды: `--help`, `--version`, `--daemon`, `--cli`
- Профиль:
	- `--profile-show`
	- `--profile-set-name "Имя"`
	- `--profile-set-bio "Описание"`
	- `--profile-set-status active|hidden|inactive`
- Сообщения (CLI):
	- `--send --chat <id> --from <user> --to <user> --message "text"`
	- `--edit --id <message_id> --message "new text"`
	- `--read --id <message_id>`
	- `--history --chat <id> --limit <n>`
- Поиск клиентов:
	- `--discover --discover-timeout 1200`
	- `--peers`
	- `--peer-add host:port:name`
- TUI мессенджера: `--tui`
- TUI: задел для следующих этапов
- API: локальный JSON-протокол (заготовка в `src/api`)
