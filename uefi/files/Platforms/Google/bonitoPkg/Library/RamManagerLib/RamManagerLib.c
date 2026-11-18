#include <Uefi.h>
#include <Library/RamManagerLib.h>
#include <Library/MemoryAllocationLib.h>

EFI_STATUS
GetUsableMemoryRanges (
  OUT EFI_MEMORY_RANGE **Range,
  OUT UINT8             *RangeCount)
{
  EFI_MEMORY_RANGE *LocalRange = AllocateZeroPool (sizeof (EFI_MEMORY_RANGE) * 2);
  if (LocalRange == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  // Contiguous System RAM ranges on bonito (outside 0xA2000000..0xA6000000 DXE Heap and 0xF0000000..0xF4000000 XNU Payload)
  LocalRange[0].Address = 0xA6000000ULL;
  LocalRange[0].Length  = 0x4A000000ULL;
  LocalRange[1].Address = 0xF4000000ULL;
  LocalRange[1].Length  = 0x8A2C0000ULL;

  *Range      = LocalRange;
  *RangeCount = 2;
  return EFI_SUCCESS;
}
