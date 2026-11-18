#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
sed -i 's/\r$//' build.sh 2>/dev/null || true

clang --target=aarch64-unknown-windows -ffreestanding -fno-builtin -fno-stack-protector -fshort-wchar \
  -mgeneral-regs-only -O2 -Wall -Wextra -c loader.c -o loader.obj

lld-link /subsystem:efi_application /entry:efi_main /nodefaultlib /base:0xA3000000 /out:loader.efi loader.obj
ls -la loader.efi
