#!/usr/bin/env bash
set -euo pipefail

ARTIFACTS_DIR="${1:-installers}"
mkdir -p "${ARTIFACTS_DIR}"

TMP_FILE="${ARTIFACTS_DIR}/.SHA256SUMS.tmp"
OUT_FILE="${ARTIFACTS_DIR}/SHA256SUMS.txt"

find "${ARTIFACTS_DIR}" -maxdepth 1 -type f \
  \( -name "*.zip" -o -name "*.tar.gz" -o -name "*.deb" -o -name "*.rpm" -o -name "*.exe" \) \
  | sort \
  | xargs -r sha256sum > "${TMP_FILE}"

mv "${TMP_FILE}" "${OUT_FILE}"
echo "Checksums written to ${OUT_FILE}"
