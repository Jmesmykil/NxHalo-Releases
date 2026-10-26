#!/bin/sh
set -eu
export DEVKITPRO=/opt/devkitpro
export DEVKITA64=/opt/devkitpro/devkitA64
cd "$(dirname "$0")"
export PATH="$PWD/native/bin:$PWD/venv/bin:/opt/homebrew/bin:$PATH"
python prepare_uam.py
ninja -f build.ninja -j4 switch-uam > uam-build.log 2>&1
/opt/devkitpro/devkitA64/bin/aarch64-none-elf-ar t build/uam/libuam.a > archive-members.txt
/opt/devkitpro/devkitA64/bin/aarch64-none-elf-ld -r --whole-archive build/uam/libuam.a --no-whole-archive -o uam-link-test.o
/opt/devkitpro/devkitA64/bin/aarch64-none-elf-readelf -h uam-link-test.o > link-test-header.txt
shasum -a 256 build/uam/libuam.a
