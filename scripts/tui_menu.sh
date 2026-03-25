#!/usr/bin/env bash
set -euo pipefail

if command -v dialog >/dev/null 2>&1; then
  BUILD_TYPE=$(dialog --stdout --menu "Build type" 12 40 2 1 Debug 2 Release)
  TESTS=$(dialog --stdout --yesno "Enable tests?" 7 40; echo $?)
  echo "BUILD_TYPE=${BUILD_TYPE}"
  if [[ "${TESTS}" == "0" ]]; then
    echo "ENABLE_TESTS=ON"
  else
    echo "ENABLE_TESTS=OFF"
  fi
else
  echo "dialog is not installed"
  exit 1
fi
