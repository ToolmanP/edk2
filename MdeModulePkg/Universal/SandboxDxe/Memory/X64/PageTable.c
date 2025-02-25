#include "PageTable.h"
#include "Base.h"
#include "Library/BaseLib.h"
#include "Library/BaseMemoryLib.h"
#include "Library/CpuPageTableLib.h"
#include "Library/MemoryAllocationLib.h"
#include "Library/UefiBootServicesTableLib.h"
#include "Memory.h"
#include "ProcessorBind.h"
#include "Register/Intel/Cpuid.h"
#include "Uefi/UefiBaseType.h"
#include "Utils/Logger.h"

/**
 Check the WP status in CR0 register. This bit is used to lock or unlock write
 access to pages marked as read-only.

  @retval TRUE    Write protection is enabled.
  @retval FALSE   Write protection is disabled.
**/
BOOLEAN
IsReadOnlyPageWriteProtected(VOID) {
  IA32_CR0 Cr0;

  Cr0.UintN = AsmReadCr0();
  return (BOOLEAN)(Cr0.Bits.WP != 0);
}

/**
 Disable Write Protect on pages marked as read-only.
**/
VOID DisableReadOnlyPageWriteProtect(VOID) {
  IA32_CR0 Cr0;

  Cr0.UintN = AsmReadCr0();
  Cr0.Bits.WP = 0;
  AsmWriteCr0(Cr0.UintN);
}

/**
 Enable Write Protect on pages marked as read-only.
**/
VOID EnableReadOnlyPageWriteProtect(VOID) {
  IA32_CR0 Cr0;

  Cr0.UintN = AsmReadCr0();
  Cr0.Bits.WP = 1;
  AsmWriteCr0(Cr0.UintN);
}

/**
  Convert VMR_PROP_T flags
(VMR_READ/VMR_WRITE/VMR_EXEC/VMR_DEVICE/VMR_NOCACHE/VMR_COW) to
IA32_MAP_ATTRIBUTE suitable for EDK2 page-table usage.

  @param[in]  VmrProp  The combination of VMR_* flags.
  @param[in]  KernelVMR  Whether the VMR is for kernel.

  @return     IA32_MAP_ATTRIBUTE representing the corresponding page table
attributes.
**/
IA32_MAP_ATTRIBUTE
ConvertVmrToMapAttr(IN UINT32 VmrProp, IN BOOLEAN KernelVMR) {
  IA32_MAP_ATTRIBUTE MapAttr;
  MapAttr.Uint64 = 0;

  // Present
  if ((VmrProp & (VMR_READ | VMR_WRITE | VMR_EXEC)) != 0) {
    MapAttr.Bits.Present = 1;
  }

  // Read / Write
  if ((VmrProp & VMR_WRITE) != 0) {
    MapAttr.Bits.ReadWrite = 1;
  }

  if ((VmrProp & VMR_EXEC) == 0) {
    MapAttr.Bits.Nx = 1;
  }

  if (!KernelVMR) {
    MapAttr.Bits.UserSupervisor = 1;
  }

  if ((VmrProp & (VMR_DEVICE | VMR_NOCACHE)) != 0) {
    MapAttr.Bits.CacheDisabled = 1;
  }

  return MapAttr;
}

EFI_STATUS CreatePageTable(OUT UINT64 *TranslationTableBasePtr) {
  __unimplemented("X64 CreatePageTable");
  return EFI_UNSUPPORTED;
}

VOID FreePageTablesRecursive(IN UINT64 *TranslationTable, IN UINTN Level) {
  __unimplemented("X64 FreePageTablesRecursive");
}

EFI_STATUS MapRangeInPageTable(IN OUT UINT64 *TranslationTableBasePtr,
                               IN EFI_PHYSICAL_ADDRESS PhysicalStart,
                               IN EFI_VIRTUAL_ADDRESS VirtualStart,
                               IN EFI_VIRTUAL_ADDRESS VirtualEnd,
                               IN VMR_PROP_T Flags, IN BOOLEAN KernelVMR,
                               IN BOOLEAN TableIsLive) {
  EFI_STATUS Status;
  IA32_MAP_ATTRIBUTE MapAttr;
  IA32_MAP_ATTRIBUTE AttrMask;

  if (TranslationTableBasePtr == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (VirtualEnd <= VirtualStart) {
    return EFI_SUCCESS;
  }

  MapAttr.Uint64 = PhysicalStart;
  MapAttr.Uint64 |= ConvertVmrToMapAttr(Flags, KernelVMR).Uint64;
  AttrMask.Uint64 = MAX_UINT64;

  UINT64 Length = (UINT64)(VirtualEnd - VirtualStart);
  PAGING_MODE PagingMode = Paging4Level1GB;
  BOOLEAN IsModified = FALSE;

  VOID *MapBuffer = NULL;
  UINTN BufferSize = 0;
  Status = PageTableMap(TranslationTableBasePtr, PagingMode, MapBuffer,
                        &BufferSize, (UINT64)VirtualStart, Length, &MapAttr,
                        &AttrMask, &IsModified);
  if (Status != EFI_BUFFER_TOO_SMALL) {
    return Status;
  }

  // TODO: manager page table buffer
  MapBuffer = AllocateZeroPool(BufferSize);
  if (MapBuffer == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = PageTableMap(TranslationTableBasePtr, PagingMode, MapBuffer,
                        &BufferSize, (UINT64)VirtualStart, Length, &MapAttr,
                        &AttrMask, &IsModified);

  return Status;
}

EFI_STATUS
UnmapRangeInPageTable(IN OUT UINT64 *TranslationTableBasePtr,
                      IN EFI_VIRTUAL_ADDRESS VirtualStart,
                      IN EFI_VIRTUAL_ADDRESS VirtualEnd,
                      IN BOOLEAN TableIsLive) {
  EFI_STATUS Status;

  if (TranslationTableBasePtr == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (VirtualEnd <= VirtualStart) {
    return EFI_SUCCESS;
  }

  UINT64 Length = (UINT64)(VirtualEnd - VirtualStart);

  IA32_MAP_ATTRIBUTE UnmapAttr;
  ZeroMem(&UnmapAttr, sizeof(UnmapAttr));
  UnmapAttr.Bits.Present = 0;

  IA32_MAP_ATTRIBUTE Mask;
  ZeroMem(&Mask, sizeof(Mask));
  Mask.Bits.Present = 1;

  PAGING_MODE PagingMode = Paging4Level1GB;
  BOOLEAN IsModified = FALSE;

  VOID *MapBuffer = NULL;
  UINTN BufferSize = 0;
  Status = PageTableMap(TranslationTableBasePtr, PagingMode, MapBuffer,
                        &BufferSize, (UINT64)VirtualStart, Length, &UnmapAttr,
                        &Mask, &IsModified);
  if (Status != EFI_BUFFER_TOO_SMALL) {
    return Status;
  }

  // TODO: manager page table buffer
  MapBuffer = AllocateZeroPool(BufferSize);
  if (MapBuffer == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = PageTableMap(TranslationTableBasePtr, PagingMode, MapBuffer,
                        &BufferSize, (UINT64)VirtualStart, Length, &UnmapAttr,
                        &Mask, &IsModified);

  return Status;
}

void SetPageTable(void *pgtbl) { AsmWriteCr3((UINT64)pgtbl); }

EFI_PHYSICAL_ADDRESS GetPageTable(void) { return AsmReadCr3(); }

VOID PrintPageTable(UINT64 PageTable) {
  EFI_STATUS Status;
  IA32_MAP_ENTRY *Map = NULL;
  UINT64 MapCount = 0;

  Status = PageTableParse(PageTable, Paging4Level1GB, Map, &MapCount);
  if (Status != RETURN_BUFFER_TOO_SMALL) {
    SBError("PageTableParse failed: %r\n", Status);
    return;
  }

  Map = AllocatePool(sizeof(IA32_MAP_ENTRY) * MapCount);
  if (Map == NULL) {
    SBError("AllocatePool for Map failed\n");
    return;
  }

  Status = PageTableParse(PageTable, Paging4Level1GB, Map, &MapCount);
  if (EFI_ERROR(Status)) {
    SBError("PageTableParse failed: %r\n", Status);
    FreePool(Map);
    return;
  }

  for (UINTN Index = 0; Index < MapCount; Index++) {
    SBDebug("Map[%d]: LinearAddress: 0x%lx, Length: 0x%lx, Attribute: 0x%lx\n",
            Index, Map[Index].LinearAddress, Map[Index].Length,
            Map[Index].Attribute.Uint64);
  }

  FreePool(Map);
}

EFI_STATUS CreateIdenticalPageTable(IN UINT64 SrcPageTable,
                                    IN OUT UINT64 *DstPageTable) {
  EFI_STATUS Status;
  IA32_MAP_ENTRY *Map = NULL;
  UINT64 MapCount = 0;

#if 0
  IA32_CR4                    Cr4;
  BOOLEAN                     Page5LevelSupport;
  UINT32                      RegEax;
  BOOLEAN                     Page1GSupport;
  CPUID_EXTENDED_CPU_SIG_EDX  RegEdx;

  //
  // Check Page5Level Support or not.
  //
  Cr4.UintN         = AsmReadCr4 ();
  Page5LevelSupport = (Cr4.Bits.LA57 ? TRUE : FALSE);

  //
  // Check Page1G Support or not.
  //
  Page1GSupport = FALSE;
  AsmCpuid (CPUID_EXTENDED_FUNCTION, &RegEax, NULL, NULL, NULL);
  if (RegEax >= CPUID_EXTENDED_CPU_SIG) {
    AsmCpuid (CPUID_EXTENDED_CPU_SIG, NULL, NULL, NULL, &RegEdx.Uint32);
    if (RegEdx.Bits.Page1GB != 0) {
      Page1GSupport = TRUE;
    }
  }
  DebugPrint(DEBUG_INFO, "Page5LevelSupport = %d, Page1GSupport = %d\n", Page5LevelSupport, Page1GSupport);
#endif

  Status = PageTableParse(SrcPageTable, Paging4Level1GB, Map, &MapCount);
  if (Status != RETURN_BUFFER_TOO_SMALL) {
    SBError("PageTableParse failed: %r\n", Status);
    return Status;
  }

  Map = AllocatePool(sizeof(IA32_MAP_ENTRY) * MapCount);
  if (Map == NULL) {
    SBError("AllocatePool for Map failed\n");
    return Status;
  }

  Status = PageTableParse(SrcPageTable, Paging4Level1GB, Map, &MapCount);
  if (EFI_ERROR(Status)) {
    SBError("PageTableParse failed: %r\n", Status);
    FreePool(Map);
    return Status;
  }

  VOID *TableBuffer;
  UINT64 BufferSize;
  IA32_MAP_ATTRIBUTE Mask;

  for (UINTN Index = 0; Index < MapCount; Index++) {
    BufferSize = 0;
    TableBuffer = NULL;
    Mask.Uint64 = MAX_UINT64;

    // TODO: should be independent of memory size
    if (Map[Index].LinearAddress + Map[Index].Length > 0x10000000000) {
      SBError("Map[%d]: LinearAddress: 0x%lx, Length: 0x%lx, out of range\n",
              Index, Map[Index].LinearAddress, Map[Index].Length);
      continue;
    }

    Status =
        PageTableMap(DstPageTable, Paging4Level1GB, TableBuffer, &BufferSize,
                     Map[Index].LinearAddress, Map[Index].Length,
                     &Map[Index].Attribute, &Mask, NULL);
    if (EFI_ERROR(Status) && Status != EFI_BUFFER_TOO_SMALL) {
      SBError("PageTableMap failed: %r\n", Status);
      FreePool(Map);
      return Status;
    }

    // TODO: manager page table buffer
    TableBuffer = AllocateZeroPool(BufferSize);
    if (TableBuffer == NULL) {
      SBError("AllocateZeroPool for TableBuffer failed\n");
      FreePool(Map);
      return EFI_OUT_OF_RESOURCES;
    }

    Status =
        PageTableMap(DstPageTable, Paging4Level1GB, TableBuffer, &BufferSize,
                     Map[Index].LinearAddress, Map[Index].Length,
                     &Map[Index].Attribute, &Mask, NULL);
    if (EFI_ERROR(Status)) {
      SBError("PageTableMap failed: %r\n", Status);
      FreePool(TableBuffer);
      break;
    }
  }

  FreePool(Map);

  return Status;
}

EFI_STATUS InitCorePageTable(UINT64 *CorePageTablePtr) {
  EFI_STATUS Status;
  UINT64 PhysicalStart;
  UINT64 PhysicalEnd;
  BOOLEAN Flag = FALSE;

  if (IsReadOnlyPageWriteProtected()) {
    Flag = TRUE;
    DisableReadOnlyPageWriteProtect();
  }

  PhysicalStart = 0x600000;
  PhysicalEnd = 0x10000000000;
  Status = MapRangeInPageTable(
      CorePageTablePtr, PhysicalStart, PHYS_TO_VIRT(PhysicalStart),
      PHYS_TO_VIRT(PhysicalEnd), VMR_READ | VMR_WRITE | VMR_EXEC, TRUE, FALSE);

  if (Flag) {
    EnableReadOnlyPageWriteProtect();
  }

  return Status;
}
