# Configuration

Main config: `config/config.json.example`.

Runtime default:
- Windows: `%APPDATA%\\Zibby\\config.json`
- Linux: `~/.config/zibby/config.json`

Storage defaults:
- Data directory:
  - Windows: `%APPDATA%\\Zibby`
  - Linux: `~/.local/share/zibby`
- Cache directory:
  - Windows: `%LOCALAPPDATA%\\Zibby\\cache`
  - Linux: `~/.cache/zibby` (or `$XDG_CACHE_HOME/zibby`)
- Downloads directory:
  - Windows: `%USERPROFILE%\\Downloads\\ZibbyDownloads`
  - Linux: `~/Downloads/zibby-downloads`

Useful CLI commands:
- `--paths` — print effective config/data/cache/download/db paths.
- `--clear-cache` — clear local cache directory and recreate it.
