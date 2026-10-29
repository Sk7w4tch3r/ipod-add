#!/usr/bin/env bash
# SPDX-License-Identifier: LGPL-2.1-or-later
set -euo pipefail

clock_sync=yes
if [[ ${1:-} == --without-clock-sync ]]; then
    clock_sync=no
    shift
fi
if (($#)); then
    printf 'Usage: %s [--without-clock-sync]\n' "$0" >&2
    exit 2
fi

if [[ ! -r /etc/os-release ]]; then
    printf 'ipod-add: /etc/os-release is missing; this installer supports Ubuntu only\n' >&2
    exit 1
fi
. /etc/os-release
if [[ ${ID:-} != ubuntu ]]; then
    printf 'ipod-add: this installer supports Ubuntu (detected %s)\n' "${ID:-unknown}" >&2
    exit 1
fi

sudo apt-get update
sudo apt-get install -y \
    build-essential \
    ffmpeg \
    imagemagick \
    libglib2.0-dev \
    libgpod-common \
    libgpod-dev \
    pkg-config

make clean all check
sudo make install
if [[ $clock_sync == yes ]]; then
    sudo make enable-clock-sync
fi

printf '\nipod-add installed successfully. Try: ipod-add --help\n'
