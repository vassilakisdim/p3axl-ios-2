#!/bin/bash
set -euo pipefail
cd /mnt/c/Users/Dimitris/.gemini/antigravity/scratch/p3axl-ios/repo-p3axl-ios-2
image_size=$(stat -c %s shim/out/shim-boot.img)
if [ "$image_size" -le 51810304 ]; then
  truncate -s 51810304 shim/out/shim-boot.img
fi
python3 /mnt/c/Users/Dimitris/.gemini/antigravity/scratch/p3axl-ios/my-ios-sweet/.tools/avb/avbtool.py add_hash_footer \
  --image shim/out/shim-boot.img \
  --partition_name boot \
  --partition_size 67108864 \
  --algorithm NONE \
  --hash_algorithm sha256 \
  --salt b89d58c893e911aa85f0a4ff99d77676e8dede540a7cd1b2b5465949656b7030 \
  --prop 'com.android.build.boot.fingerprint:google/twrp_bonito/bonito:99.87.36/SP2A.220405.004/10:eng/test-keys' \
  --prop com.android.build.boot.os_version:99.87.36 \
  --prop com.android.build.boot.security_patch:2099-12-31
python3 /mnt/c/Users/Dimitris/.gemini/antigravity/scratch/p3axl-ios/my-ios-sweet/.tools/avb/avbtool.py info_image --image shim/out/shim-boot.img > shim/out/avb-footer.txt
ls -lh shim/out/shim-boot.img
