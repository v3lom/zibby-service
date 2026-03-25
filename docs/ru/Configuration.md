# Конфигурация

Основной файл: `config/config.json.example`.

Путь по умолчанию:
- Windows: `%APPDATA%\\Zibby\\config.json`
- Linux: `~/.config/zibby/config.json`

Пути хранения по умолчанию:
- Каталог данных:
  - Windows: `%APPDATA%\\Zibby`
  - Linux: `~/.local/share/zibby`
- Каталог кеша:
  - Windows: `%LOCALAPPDATA%\\Zibby\\cache`
  - Linux: `~/.cache/zibby` (или `$XDG_CACHE_HOME/zibby`)
- Каталог загрузок:
  - Windows: `%USERPROFILE%\\Downloads\\ZibbyDownloads`
  - Linux: `~/Downloads/zibby-downloads`

Полезные CLI-команды:
- `--paths` — вывести итоговые пути (config/data/cache/download/db).
- `--clear-cache` — очистить локальный кеш и пересоздать каталог.
