#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
TEST_ONLY=0

if [[ "${1:-}" == "--test-only" ]]; then
  TEST_ONLY=1
fi

select_build_type() {
  if command -v dialog >/dev/null 2>&1; then
    dialog --stdout --menu "Select build type" 12 40 2 1 Debug 2 Release
  else
    echo "Select build type: 1) Debug 2) Release"
    read -r choice
    if [[ "${choice}" == "1" ]]; then
      echo "Debug"
    else
      echo "Release"
    fi
  fi
}

ask_yes_no() {
  local prompt="$1"
  local default="${2:-Y}"
  local answer
  read -r -p "${prompt} [${default}/$( [[ ${default} == Y ]] && echo n || echo y )]: " answer
  answer="${answer:-$default}"
  [[ "${answer}" =~ ^[Yy]$ ]]
}

BUILD_TYPE="$(select_build_type)"
if ask_yes_no "Enable tests?" "Y"; then ENABLE_TESTS=ON; else ENABLE_TESTS=OFF; fi
if ask_yes_no "Enable calls module?" "N"; then ENABLE_CALLS=ON; else ENABLE_CALLS=OFF; fi
if ask_yes_no "Enable plugins module?" "Y"; then ENABLE_PLUGINS=ON; else ENABLE_PLUGINS=OFF; fi

mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

cmake .. \
  -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
  -DBUILD_TESTS="${ENABLE_TESTS}" \
  -DBUILD_CALLS="${ENABLE_CALLS}" \
  -DBUILD_PLUGINS="${ENABLE_PLUGINS}"

if [[ "${ENABLE_TESTS}" == "ON" ]]; then
  cmake --build . --config "${BUILD_TYPE}" --target zibby-service
  ctest --output-on-failure
fi

if [[ "${TEST_ONLY}" == "1" ]]; then
  exit 0
fi

cmake --build . --config "${BUILD_TYPE}"
cpack -C "${BUILD_TYPE}"

if [[ -x "${ROOT_DIR}/scripts/generate_checksums.sh" ]]; then
  "${ROOT_DIR}/scripts/generate_checksums.sh" "${ROOT_DIR}/installers"
fi
