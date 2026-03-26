# Zibby Service

Zibby Service is a cross-platform background daemon for a decentralized IP messenger with secure peer-to-peer communication.

Repository: https://github.com/v3lom/zibby-service

## Key Features

- Decentralized architecture without a central server.
- Encrypted communication primitives (OpenSSL-based).
- Local encrypted-ready storage layer (SQLite/SQLCipher compatible).
- Extensible architecture with plugin, API, media, and platform-specific layers.
- CLI-first bootstrap for headless/server environments.

## Requirements

- CMake 3.16+
- C++ compiler: GCC 10+ / Clang 12+ / MSVC 2019+
- Boost (filesystem, system, program_options, Asio)
- OpenSSL 1.1+ or 3.x
- SQLite3 (SQLCipher-compatible builds are supported by option)

## Quick Build

### Windows (PowerShell)

```powershell
./scripts/install_deps_windows.ps1
./build.ps1
```

### Linux

```bash
./scripts/install_deps_linux.sh
./build.sh
```

## Run

```bash
zibby-service --help
zibby-service --version
zibby-service --daemon
zibby-service --cli
zibby-service --check-updates
```

`--cli` connects to a running local daemon instance and checks service availability.

### Windows launcher + Panel (beta)

- On Windows, running `zibby-service.exe` **without arguments** acts as a launcher:
	- if the service is NOT running: starts the daemon in background and exits
	- if the service IS running: opens the embedded Qt control panel (full build)
- You can also force-open the panel (full build):

```powershell
zibby-service --panel
```

To build the panel with MSYS2/MinGW, install Qt6:

```powershell
./scripts/install_deps_windows.ps1
```

Build variants:

```powershell
# full build (embedded Qt panel, default)
./build.ps1 -EnablePanel $true

# normal build (headless, no Qt panel)
./build.ps1 -EnablePanel $false
```

### Windows service / autostart

- NSIS installer (when available) runs best-effort setup:
	- tries to install + start the Windows Service (requires Administrator)
	- if not elevated, falls back to enabling per-user autostart (HKCU Run -> `--daemon`)

- Best-effort setup/cleanup (what the installer uses):

```powershell
zibby-service --install-best-effort
zibby-service --uninstall-best-effort
```

- Install Windows Service (requires Administrator):

```powershell
zibby-service --install-service
zibby-service --start-service
zibby-service --stop-service
zibby-service --uninstall-service
```

- Enable autostart for current user (no admin, uses HKCU Run -> `--daemon`):

```powershell
zibby-service --enable-autostart
zibby-service --disable-autostart
```

## Configuration

Default config file locations:

- Windows: `%APPDATA%\Zibby\config.json`
- Linux: `~/.config/zibby/config.json`

An example config is available in `config/config.json.example`.

If config does not exist, it is created automatically on first start.

## Logs and Data

- Data directory defaults:
	- Windows: `%APPDATA%\Zibby`
	- Linux: `~/.local/share/zibby`
- Log file: `zibby.log` in data directory.
- User keys (`private.pem`, `public.pem`) are generated at first run.

## Updates

- The service and panel can check GitHub latest release (see `--check-updates`).
- If your network/IP is rate-limited by GitHub API, set `ZIBBY_GITHUB_TOKEN` (or `GITHUB_TOKEN`) to use authenticated requests with higher limits.

## Security Notes

- Transport and key primitives are handled via OpenSSL.
- Local data layer is designed for encrypted SQLite deployment.
- Protect generated private keys and data directory permissions.

## Documentation

See `docs/`:

- `docs/en` – English docs
- `docs/ru` – Russian docs
- `docs/doxygen` – API docs configuration

## License

GNU General Public License v3.0.

See `LICENSE` for the full text.

