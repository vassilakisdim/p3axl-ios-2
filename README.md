# p3axl-ios-2

**Apple's iOS 16 (iOS 16.7.16 20H392) ported to the Google Pixel 3a XL (`bonito`, Qualcomm Snapdragon 670 / SDM670).**

Based on research and implementations from [hackiosx.dev](https://hackiosx.dev), [dipa-studio/ios-sweet](https://github.com/dipa-studio/ios-sweet), [joranv1/ios-sailfish](https://github.com/joranv1/ios-sailfish), and [frokfrdk/ios-sweet](https://github.com/frokfrdk/ios-sweet).

This repository ports Apple's open-source XNU kernel (`xnu-8796`, iOS 16 generation) to the Google Pixel 3a XL and runs the unmodified iOS 16.7.16 userland of the iPhone X (`iPhone10,3` / `D22AP`): `launchd`, system daemons, `backboardd`, `SpringBoard`, and stock applications.

---

## Hardware Architecture & Platform Facts

| Area | Pixel 3a XL (`bonito`) Specification | Adaptation / Implementation |
| --- | --- | --- |
| **SoC** | Qualcomm Snapdragon 670 (SDM670, `sdm710` family) | `ARM64_BOARD_CONFIG_BONITO`, ARMv8.2-A, 16K pages, non-PAC |
| **CPU** | 8 Cores: 6x Kryo 360 Silver (Cortex-A55) + 2x Kryo 360 Gold (Cortex-A75) | ARM standard PSCI 1.0 via SMC; MPIDR clusters 0 and 1 |
| **Interrupts** | ARM GICv3: GICD `0x17a00000`, GICR `0x17a60000` (stride 0x20000) | Group 1 IRQ handling, virtual timer INTID 19 |
| **Memory** | 4 GB LPDDR4X (Low bank: `0x80000000` 2GB; High bank: `0x100000000` ~2GB) | Upper DDR bank / contiguous segment (`0xa2000000`–`0x17e2c0000`), above secure carveouts |
| **Bootloader** | Google Pixel ABL (Android Bootloader), Header Version 2 | Bare-metal shim (`shim.elf`), LZ4 framed kernel, 64 MiB AVB footer |
| **Display** | 1080×2160 Samsung AMOLED (`sofef00`), DSI Command Mode | DPU Continuous splash at `0x9c000000` (36 MiB); `SDE_CTL0_START` (`0x0ae0201c`) kickoff |
| **Storage** | 64 GB SKHynix eMMC 5.1 (`sdhci@7c4000` / `/dev/block/mmcblk0`) | SDHCI polled driver (`emmcroot0` on `userdata` `mmcblk0p72`) or RAM-disk (`rd=md0`) |
| **Touchscreen**| Synaptics DSX over GENI QUP I²C (`0xa84000` at slave `0x20`) | Polled I²C / HID digitizer events dispatched to `backboardd` |
| **Buttons** | Power & Vol- on PMIC PON (`0x00800`), Vol+ on GPIO 7 | PMIC SPMI observer polling; mapped to Apple HID consumer keys |
| **Device ID** | Identity presented to iOS: iPhone X (`iPhone10,3` / `D22AP`) | Home-button-less gesture navigation UI, MobileGestalt matching |
| **Activation** | Standalone "Hactivation" path in `mobileactivationd` | Dev-fused device-tree security status; no Apple servers needed |

---

## How It Boots

```
Pixel 3a XL Bootloader (ABL, unlocked)
  │
  ├─> fastboot boot shim-boot.img                (Temporary boot; nothing flashed to boot/recovery)
  │     │
  │     └─> shim: bare-metal AArch64 loader (start.S + console.c + mmu.c + loader.c)
  │           ├─ Extracts payload from ABL ramdisk (rd_start / rd_end in DTB /chosen)
  │           ├─ Resolves Mach-O segments to physical RAM (0xa2000000 / 0x100000000)
  │           ├─ Places Apple Device Tree (ADT) and struct boot_args behind kernel
  │           └─ Jumps to XNU kernel entry point (entry_va) with MMU off at EL1
  │
  └─> Apple XNU Kernel (xnu-8796 BONITO)
        ├─ locore: bootstrap TTBR0/TTBR1 16K translation tables, MMU on
        ├─ Framebuffer console: kprintf drawn at 0x9c000000 with 0x0ae0201c DPU kickoff
        ├─ Storage mount:
        │    ├─ Safe testing: RAM-disk root (rd=md0)
        │    └─ Permanent: Qualcomm SDHCI driver mounts HFS+ root from userdata (rd=emmcroot0)
        └─ iOS Userland launch:
             └─ /sbin/launchd -> daemons -> backboardd -> SpringBoard (Lock/Home Screen)
```

---

## Building and Running

### Prerequisites
- Host: Windows with WSL2 (Ubuntu) or Linux x86_64
- Cross-compiler: `aarch64-linux-gnu-gcc`, `binutils-aarch64-linux-gnu`, `lz4`, `mkbootimg`, `python3`
- GitHub CLI (`gh`) for automated XNU builds
- Google Pixel 3a XL (`bonito`) with **unlocked bootloader** and USB debugging enabled

### 1. Build the XNU Kernel
The kernel builds on macOS runners via GitHub Actions using [blacktop/darwin-xnu-build](https://github.com/blacktop/darwin-xnu-build):

```bash
gh workflow run xnu.yml -R vassilakisdim/p3axl-ios-2
gh run download -R vassilakisdim/p3axl-ios-2 -n xnu-kernel -D xnu/out
```

The pre-compiled kernel for `bonito` (`kernel.development.bonito`) is placed in `xnu/out/`.

### 2. Generate Apple Device Tree (ADT) and Payload
Build the `bonito` ADT containing the SDM670 MMIO layout, Samsung command-mode display kickoff, and iPhone X identity:

```bash
mkdir -p shim/out
python3 tools/mkadt.py shim/out/bonito.adt --bonito

python3 tools/mkpayload.py xnu/out/kernel.development.bonito shim/out/bonito.adt shim/out/payload.bin \
  --cmdline "debug=0xc serial=3 cpus=1 keepsyms=1 -enable_kprintf_spam -noprogress rd=emmcroot0 unentitled_ios_sim_launch=1 ignition_level=1 wdt_test=no_user_checkin ufsrw=1 rootrw=1 launchd_unsecure_cache=1 wdt=-1 sweetfb_noconsole=1 sweetfb_hz=30"
```

### 3. Build Bootloader Shim Image
Compile the bare-metal shim, frame it with LZ4, and pad with the 64 MiB AVB hash footer required by Pixel ABL:

```bash
cd shim
bash build.sh out/payload.bin
cd ..
bash apply-avb.sh
```

### 4. Build iOS 16 Root Filesystem (`ios16-root.simg`)
Download the iPhone X (`iPhone10,3`) iOS 16.7.16 (`20H392`) IPSW from Apple:

```bash
tools/mkiosroot.sh OS.dmg SystemOS.dmg AppOS.dmg ios16-root.img 16G
sudo fsck.hfsplus -fy ios16-root.img
sudo mount -t hfsplus -o loop,force ios16-root.img /mnt/iosroot
sudo python3 tools/sweet-root-tweaks.py /mnt/iosroot
sudo umount /mnt/iosroot
img2simg ios16-root.img ios16-root.simg
```

### 5. Flash and Boot
Boot the phone into bootloader mode (`adb reboot bootloader` or hold `Power + Vol Down`):

```bash
# Flash the iOS HFS+ root image to the userdata partition:
fastboot -S 512M flash userdata ios16-root.simg

# Boot the custom XNU bootloader shim temporarily (does NOT flash boot partitions):
fastboot boot shim/out/shim-boot.img
```

The phone boots through the custom shim directly into Apple's XNU kernel, initializes display, touch, and brings up iOS 16 `SpringBoard`.

---

## Repository Structure

```
├── .github/workflows/
│   ├── shim.yml            # CI cross-compilation of AArch64 boot shim
│   └── xnu.yml             # CI build of Apple XNU kernel for BONITO
├── docs/
│   ├── bonito/
│   │   ├── NOTES.md        # Technical execution log, register maps, memory ranges
│   │   └── PORTING.md      # Detailed porting guide and milestone tracking
├── shim/
│   ├── start.S             # AArch64 Linux Image entry, EL1 vectors, stack setup
│   ├── shim.c              # Milestone diagnostics, PSCI/GIC tests
│   ├── console.c           # Text console with SDE_CTL0_START command-mode kickoff
│   ├── loader.c            # iBoot Mach-O segment loader & boot_args handoff
│   ├── mmu.c               # Early 16K translation tables & identity mapping
│   ├── sdm670.dtb          # Device Tree Blob from stock Pixel 3a XL Android 12
│   └── build.sh            # Shim compiler and boot.img v2 packager
├── tools/
│   ├── mkadt.py            # Apple Device Tree generator for bonito
│   ├── mkpayload.py        # Packs XNU Mach-O + ADT + boot-args
│   ├── mkiosroot.sh        # Assembles HFS+ root volume from Apple IPSW
│   ├── sweet-root-tweaks.py# Customizations for setup skip, activation, watchdog bypass
│   └── sweet-log-pull.sh   # Extracts persistent kernel console log from userdata
└── xnu/
    ├── patches/            # Patch series 0001–0086 (Machine config, DPU kickoff, SDHCI eMMC)
    └── out/                # Output directory for compiled XNU kernel
```

---

## License
Apple open-source components (`xnu`, `libpthread`, `hfs`) are licensed under the [Apple Public Source License (APSL 2.0)](LICENSE). Port additions and shims are provided under the MIT license.
