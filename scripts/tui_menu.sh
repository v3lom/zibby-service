#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BOOTSTRAP_DIR="${ROOT_DIR}/build/bootstrap"

mkdir -p "${BOOTSTRAP_DIR}"

GEN_ARGS=()
if command -v ninja >/dev/null 2>&1; then
  GEN_ARGS=("-G" "Ninja")
fi

cmake -S "${ROOT_DIR}" -B "${BOOTSTRAP_DIR}" "${GEN_ARGS[@]}" \
  -DZIBBY_ENABLE_TESTS=OFF \
  -DZIBBY_ENABLE_CALLS=OFF \
  -DZIBBY_ENABLE_PLUGINS=OFF \
  -DZIBBY_ENABLE_PANEL=OFF

cmake --build "${BOOTSTRAP_DIR}" --target zibby-build-tui

"${BOOTSTRAP_DIR}/zibby-build-tui"
exit $?
