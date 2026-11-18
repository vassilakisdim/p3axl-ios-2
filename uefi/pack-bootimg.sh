#!/bin/bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT="$ROOT/uefi/out"
MU="/root/Mu-Silicium"
UNPACKED="/mnt/c/Users/Dimitris/.gemini/antigravity/scratch/p3axl-ios/unpacked_boot"
AVBTOOL="/mnt/c/Users/Dimitris/.gemini/antigravity/scratch/p3axl-ios/my-ios-sweet/.tools/avb/avbtool.py"

mkdir -p "$OUT"

# 1. Build BootShim.bin with PAYLOAD_BASE=0xF0000000
rm -f "$MU/BootShim/BootShim.elf" "$MU/BootShim/BootShim.bin"
make -C "$MU/BootShim" REQUIRES_KERNEL_HEADER=1 FD_BASE=0x9fd00000 FD_SIZE=0x200000 \
  CFLAGS="--target=aarch64-linux-gnu -DPAYLOAD_BASE=0xF0000000"

cp "$MU/BootShim/BootShim.bin" "$OUT/BootShim.bin"
cp "$MU/Build/bonitoPkg/RELEASE_CLANGPDB/FV/SILICIUM_UEFI.fd" "$OUT/SILICIUM_UEFI.fd"

# 2. Build UEFI XNU loader (loader.efi)
bash "$ROOT/xnuloader/build.sh"
cp "$ROOT/xnuloader/loader.efi" "$OUT/loader.efi"

# 3. Build bonito.adt (phys_base = 0xa6000000, mem_end = 0xf0000000)
python3 "$ROOT/tools/mkadt.py" "$OUT/bonito.adt" --bonito

# 4. Ensure ramdisk-init.img is present
if [ ! -f "$OUT/ramdisk-init.img" ]; then
  clang -target arm64-apple-ios16.0 -c "$ROOT/tools/sweet-init.S" -o "$OUT/sweet-init.o"
  python3 "$ROOT/tools/mkinit.py" "$OUT/sweet-init.o" "$OUT/ramdisk-init.img"
fi

# 5. Build SWEETXNU payload (resolves LC_DYLD_CHAINED_FIXUPS + attaches ramdisk-init.img and bonito.adt)
RAMDISK_PATH="${1:-$OUT/ramdisk-init.img}"
CMDLINE="${2:-debug=0xc serial=3 cpus=1 rd=md0 -v keepsyms=1}"
python3 "$ROOT/tools/mkpayload.py" "$ROOT/xnu/out/kernel.development.bonito" "$OUT/bonito.adt" "$OUT/sweetxnu.bin" \
  --ramdisk "$RAMDISK_PATH" \
  --cmdline "$CMDLINE"

# 6. Build XNUP container (loader.efi + sweetxnu)
python3 "$ROOT/xnuloader/mkxnup.py" "$OUT/xnup.bin" \
  "loader.efi=$OUT/loader.efi" \
  "sweetxnu=$OUT/sweetxnu.bin"

# 7. Combine BootShim.bin + SILICIUM_UEFI.fd + xnup.bin into ARM64 Image, pad to 51,591,192 bytes, and compress with LZ4
cat "$OUT/BootShim.bin" "$OUT/SILICIUM_UEFI.fd" "$OUT/xnup.bin" > "$OUT/Image"
truncate -s 51591192 "$OUT/Image"
lz4 -f -q -B7 -BI --frame-crc "$OUT/Image" "$OUT/Image.lz4"

# 8. Pack Android Boot Image v2 for bonito ABL + 64 MiB AVB foote
mkbootimg \
  --kernel "$OUT/Image.lz4" \
  --ramdisk "$UNPACKED/ramdisk" \
  --dtb "$UNPACKED/dtb" \
  --cmdline "console=ttyMSM0,115200n8 earlycon=msm_geni_serial,0xa80000 androidboot.hardware=bonito androidboot.console=ttyMSM0 lpm_levels.sleep_disabled=1 msm_rtb.filter=0x237 service_locator.enable=1 swiotlb=1 firmware_class.path=/vendor/firmware_mnt/image loop.max_part=7 buildvariant=user" \
  --base 0x00000000 \
  --kernel_offset 0x00008000 \
  --ramdisk_offset 0x01000000 \
  --second_offset 0x00000000 \
  --tags_offset 0x00000100 \
  --dtb_offset 0x01f00000 \
  --os_version 12.0.0 \
  --os_patch_level 2022-05 \
  --pagesize 4096 \
  --header_version 2 \
  -o "$OUT/mu-silicium-bonito-boot.img"

truncate -s 51810304 "$OUT/mu-silicium-bonito-boot.img"
python3 "$AVBTOOL" add_hash_footer \
  --image "$OUT/mu-silicium-bonito-boot.img" \
  --partition_name boot \
  --partition_size 67108864 \
  --salt d466931f0a69d466931f0a69d466931f0a69d466931f0a69d466931f0a69d466

ls -la "$OUT/mu-silicium-bonito-boot.img"
