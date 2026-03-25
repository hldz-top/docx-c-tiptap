#!/usr/bin/env bash
set -euo pipefail

# Install common build dependencies on Debian/Ubuntu. Adjust for other distros.
if [ "$(id -u)" -ne 0 ]; then
  echo "Run as root or with sudo: sudo ./scripts/setup_linux.sh"
  exit 1
fi

if command -v apt-get >/dev/null 2>&1; then
  apt-get update
  apt-get install -y build-essential cmake pkg-config libxml2-dev
  echo "Dependencies installed via apt-get."
else
  echo "Unsupported package manager. Please install: build-essential, cmake, pkg-config, libxml2-dev"
  exit 1
fi
