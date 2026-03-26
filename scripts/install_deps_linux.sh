#!/usr/bin/env bash
set -euo pipefail

if command -v apt >/dev/null 2>&1; then
  sudo apt update
  sudo apt install -y build-essential cmake ninja-build pkg-config \
    libboost-all-dev libssl-dev libsqlite3-dev catch2
elif command -v pacman >/dev/null 2>&1; then
  sudo pacman -Sy --noconfirm base-devel cmake ninja pkgconf \
    boost openssl sqlite catch2
elif command -v yum >/dev/null 2>&1; then
  sudo yum install -y gcc-c++ make cmake ninja-build pkgconfig \
    boost-devel openssl-devel sqlite-devel catch2
else
  echo "Unsupported package manager. Install dependencies manually."
  exit 1
fi

echo "Dependencies installed."
