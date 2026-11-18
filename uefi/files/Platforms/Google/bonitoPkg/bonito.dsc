##
#  Google Pixel 3a XL (bonito, Qualcomm SDM670) Mu-Silicium Platform DSC
#  SPDX-License-Identifier: BSD-2-Clause-Patent
##

[Defines]
  PLATFORM_NAME                  = bonito
  PLATFORM_GUID                  = 8A940F33-4011-4ACB-8348-1A6BB928C8A6
  PLATFORM_VERSION               = 0.1
  DSC_SPECIFICATION              = 0x00010005
  OUTPUT_DIRECTORY               = Build/bonitoPkg
  SUPPORTED_ARCHITECTURES        = AARCH64
  BUILD_TARGETS                  = RELEASE|DEBUG
  SKUID_IDENTIFIER               = DEFAULT
  FLASH_DEFINITION               = bonitoPkg/bonito.fdf
  USE_CUSTOM_DISPLAY_DRIVER      = 0
  USE_PHYSICAL_TIMER             = 0

  HAS_ACTLR_EL1_UNIMPLEMENTED_ERRATA         = 0
  HAS_AMCNTENSET0_EL0_UNIMPLEMENTED_ERRATA   = 0
  HAS_GIC_V3_WITHOUT_IRM_FLAG_SUPPORT_ERRATA = 0
  HAS_PSCI_MEMPROTECT_FAILING_ERRATA         = 0

!include SiliciumPkg/SiliciumPkg.dsc.inc

[BuildOptions]
  *_*_AARCH64_CC_FLAGS = -march=armv8.2-a+crypto+rcpc -D HAS_ACTLR_EL1_UNIMPLEMENTED_ERRATA=0 -D HAS_AMCNTENSET0_EL0_UNIMPLEMENTED_ERRATA=0 -D HAS_GIC_V3_WITHOUT_IRM_FLAG_SUPPORT_ERRATA=0 -D HAS_PSCI_MEMPROTECT_FAILING_ERRATA=0

[PcdsFixedAtBuild]
  #
  # DDR Memory
  #
  gArmTokenSpaceGuid.PcdSystemMemoryBase|0x80000000

  #
  # UEFI Stack
  #
  gArmPlatformTokenSpaceGuid.PcdCPUCoresStackBase|0x9FF90000
  gArmPlatformTokenSpaceGuid.PcdCPUCorePrimaryStackSize|0x40000

  #
  # CPU
  #
  gArmPlatformTokenSpaceGuid.PcdCoreCount|8
  gArmPlatformTokenSpaceGuid.PcdClusterCount|2

  #
  # GICv3 (SDM670: GICD 0x17A00000, GICR 0x17A60000)
  #
  gArmTokenSpaceGuid.PcdGicDistributorBase|0x17A00000
  gArmTokenSpaceGuid.PcdGicRedistributorsBase|0x17A60000

  #
  # Arch Timer (SDM670: Virtual Timer PPI 3 = INTID 19)
  #
  gArmTokenSpaceGuid.PcdArmArchTimerSecIntrNum|17
  gArmTokenSpaceGuid.PcdArmArchTimerIntrNum|18
  gArmTokenSpaceGuid.PcdArmArchTimerVirtIntrNum|19
  gArmTokenSpaceGuid.PcdArmArchTimerHypIntrNum|20

  #
  # Boot Manager Timeout (1s to show Mu-Silicium boot logo before XnuLoaderMain)
  #
  gEfiMdePkgTokenSpaceGuid.PcdPlatformBootTimeOut|1

  #
  # SMBIOS
  #
  gSiliciumPkgTokenSpaceGuid.PcdSmbiosSystemManufacturer|"Google"
  gSiliciumPkgTokenSpaceGuid.PcdSmbiosSystemModel|"Pixel 3a XL"
  gSiliciumPkgTokenSpaceGuid.PcdSmbiosSystemRetailModel|"bonito"
  gSiliciumPkgTokenSpaceGuid.PcdSmbiosSystemRetailSku|"G020A"
  gSiliciumPkgTokenSpaceGuid.PcdSmbiosSystemBoardModel|"bonito"
  gSiliciumPkgTokenSpaceGuid.PcdSmbiosProcessorModel|"Snapdragon (TM) 670 @ 2.00 GHz"
  gSiliciumPkgTokenSpaceGuid.PcdSmbiosProcessorPartNumber|"SDM670"

  #
  # Simple Frame Buffer (Samsung sofef00 1080x2160 32-bpp at 0x9C000000)
  #
  gSiliciumPkgTokenSpaceGuid.PcdFrameBufferWidth|1080
  gSiliciumPkgTokenSpaceGuid.PcdFrameBufferHeight|2160
  gSiliciumPkgTokenSpaceGuid.PcdFrameBufferColorDepth|32

[LibraryClasses]
  MemoryMapLib|bonitoPkg/Library/MemoryMapLib/MemoryMapLib.inf
  RamManagerLib|bonitoPkg/Library/RamManagerLib/RamManagerLib.inf
  PlatformSecLib|NapaliPkg/Library/PlatformSecLib/PlatformSecLib.inf
