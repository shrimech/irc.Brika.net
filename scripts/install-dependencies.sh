#!/bin/sh
set -eu

if ! command -v apt-get >/dev/null 2>&1; then
    echo "This installer supports Debian and Ubuntu systems with apt-get." >&2
    exit 1
fi

if [ "$(id -u)" -eq 0 ]; then
    install_packages() {
        apt-get "$@"
    }
elif command -v sudo >/dev/null 2>&1; then
    install_packages() {
        sudo apt-get "$@"
    }
else
    echo "Run this script as root or install sudo to use it." >&2
    exit 1
fi

install_packages update
install_packages install -y build-essential python3
