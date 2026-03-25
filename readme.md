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
```

`--cli` connects to a running local daemon instance and checks service availability.

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

