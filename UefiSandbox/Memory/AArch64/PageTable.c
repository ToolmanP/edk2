#include "Base.h"
#include "AArch64/PageTable.h"
#include "Memory.h"
#include "Print.h"
#include "ProcessorBind.h"
#include "Uefi/UefiBaseType.h"

#include "Library/ArmLib.h"
#include "Library/ArmMmuLib.h"
#include "Library/BaseLib.h"
#include "Library/BaseMemoryLib.h"
#include "Library/DebugLib.h"
#include "Library/MemoryAllocationLib.h"

#define MIN_T0SZ 16
#define BITS_PER_LEVEL 9
#define MAX_VA_BITS 48

#define CACHE_LINE_LENGTH 64

inline VOID DcacheCleanAndInvalidateArea(UINT64 start, UINT64 end)
{
	while (start < end) {
		asm volatile("dc civac, %0" : : "r"(start) : "memory");
		start += CACHE_LINE_LENGTH;
	}
    asm volatile("dsb ish");
    asm volatile("isb");
}

inline VOID FlushIcacheAll(VOID)
{
	/*
	 * ic iallu is not enough. We need instructions to be data
	 * coherence in the inner shareable domain. So we use ic ialluis:
	 * Invalidate instruction cache ALL to PoU (Inner Shareable)
	 */
	asm volatile("ic ialluis");
    
    asm volatile("dsb ish");
    asm volatile("isb");
}

void SetPageTable(void *pgtbl)
{
    ArmSetTTBR0(pgtbl);
    ArmInvalidateTlb();
}

EFI_PHYSICAL_ADDRESS GetPageTable(void)
{
    return (EFI_PHYSICAL_ADDRESS)ArmGetTTBR0BaseAddress();
}

/*
 * Convert VM region properties to page attributes
 */
static UINT64
VmrPropToPageAttr (VMR_PROP_T Prop, BOOLEAN IsKernelPTE) {
    PTE_T Pte;

    Pte.PTE = 0;

    if (IsKernelPTE) {
        if (!(Prop & VMR_EXEC)) {
            Pte.l3_page.PXN = AARCH64_MMU_ATTR_PAGE_PXN;
        }
        Pte.l3_page.UXN = AARCH64_MMU_ATTR_PAGE_UXN;

        if ((Prop & VMR_READ) && !(Prop & VMR_WRITE)) {
            Pte.l3_page.AP = AARCH64_MMU_ATTR_PAGE_AP_HIGH_RO_EL0_NONE;
        } else if (Prop & VMR_WRITE) {
            Pte.l3_page.AP = AARCH64_MMU_ATTR_PAGE_AP_HIGH_RW_EL0_NONE;
        }
    } else {
        if (!(Prop & VMR_EXEC)) {
            Pte.l3_page.UXN = AARCH64_MMU_ATTR_PAGE_UXN;
        }
        Pte.l3_page.PXN = AARCH64_MMU_ATTR_PAGE_PXN;

        if ((Prop & VMR_READ) && !(Prop & VMR_WRITE)) {
            Pte.l3_page.AP = AARCH64_MMU_ATTR_PAGE_AP_HIGH_RO_EL0_RO;
        } else if (Prop & VMR_WRITE) {
            Pte.l3_page.AP = AARCH64_MMU_ATTR_PAGE_AP_HIGH_RW_EL0_RW;
        }
    }

    Pte.l3_page.AF = AARCH64_MMU_ATTR_PAGE_AF_ACCESSED;
    Pte.l3_page.nG = 1;
    Pte.l3_page.SH = INNER_SHAREABLE;
    if (Prop & VMR_DEVICE) {
        Pte.l3_page.attr_index = DEVICE_MEMORY;
        Pte.l3_page.SH = 0;
    } else if (Prop & VMR_NOCACHE) {
        Pte.l3_page.attr_index = NORMAL_MEMORY_NON_CACHEABLE;
    } else {
        Pte.l3_page.attr_index = NORMAL_MEMORY_WB;
    }

    return Pte.PTE;
}

STATIC
UINTN
GetRootTableEntryCount(IN UINTN T0SZ) {
    return TT_ENTRY_COUNT >> (T0SZ - MIN_T0SZ) % BITS_PER_LEVEL;
}

STATIC
UINTN
GetRootTableLevel(IN UINTN T0SZ) {
    return (T0SZ - MIN_T0SZ) / BITS_PER_LEVEL;
}

STATIC
BOOLEAN
IsBlockEntry(IN UINT64 Entry, IN UINTN Level) {
    if (Level == 3) {
        return (Entry & TT_TYPE_MASK) == TT_TYPE_BLOCK_ENTRY_LEVEL3;
    }

    return (Entry & TT_TYPE_MASK) == TT_TYPE_BLOCK_ENTRY;
}

STATIC
BOOLEAN
IsTableEntry(IN UINT64 Entry, IN UINTN Level) {
    if (Level == 3) {
        //
        // TT_TYPE_TABLE_ENTRY aliases TT_TYPE_BLOCK_ENTRY_LEVEL3
        // so we need to take the level into account as well.
        //
        return FALSE;
    }

    return (Entry & TT_TYPE_MASK) == TT_TYPE_TABLE_ENTRY;
}

STATIC
VOID ReplaceTableEntry(
    IN UINT64 *Entry, 
    IN UINT64 Value, 
    IN UINT64 RegionStart, 
    IN UINT64 BlockMask, 
    IN BOOLEAN IsLiveBlockMapping) {
    BOOLEAN DisableMmu;

    //
    // Replacing a live block entry with a table entry (or vice versa) requires
    // a break-before-make sequence as per the architecture. This means the
    // mapping must be made invalid and cleaned from the TLBs first, and this is
    // a bit of a hassle if the mapping in question covers the code that is
    // actually doing the mapping and the unmapping, and so we only bother with
    // this if actually necessary.
    //

    if (!IsLiveBlockMapping || !ArmMmuEnabled()) {
        // If the mapping is not a live block mapping, or the MMU is not on yet,
        // we can simply overwrite the entry.
        *Entry = Value;
        ArmUpdateTranslationTableEntry(Entry, (VOID *)(UINTN)RegionStart);
    } else {
        // If the mapping in question does not cover the code that updates the
        // entry in memory, or the entry that we are intending to update, we can
        // use an ordinary break before make. Otherwise, we will need to
        // temporarily disable the MMU.
        DisableMmu = FALSE;
        if ((((RegionStart ^ (UINTN)ArmReplaceLiveTranslationEntry) &
              ~BlockMask) == 0) ||
            (((RegionStart ^ (UINTN)Entry) & ~BlockMask) == 0)) {
            DisableMmu = TRUE;
            DEBUG((DEBUG_WARN, "%a: splitting block entry with MMU disabled\n",
                   __func__));
        }

        ArmReplaceLiveTranslationEntry(Entry, Value, RegionStart, DisableMmu);
    }
}

STATIC
EFI_STATUS
UpdateRegionMappingRecursive(
    IN UINT64 PhysicalStart,
    IN UINT64 VirtualStart,
    IN UINT64 VirtualEnd, 
    IN UINT64 AttributeSetMask,
    IN UINT64 *PageTable,
    IN UINTN Level, 
    IN BOOLEAN ClearMapping,
    IN BOOLEAN TableIsLive
    )
{
    UINT64 RegionStart;
    UINT64 RegionEnd;
    INT64 AddressOffset;
    UINTN BlockShift;
    UINT64 BlockMask;
    UINT64 BlockEnd;
    UINT64 *Entry;
    UINT64 EntryValue;
    VOID *TranslationTable;
    EFI_STATUS Status;
    BOOLEAN NextTableIsLive;

    RegionStart = VirtualStart;
    RegionEnd = VirtualEnd;
    AddressOffset = PhysicalStart - VirtualStart;
    BlockShift = (Level + 1) * BITS_PER_LEVEL + MIN_T0SZ;
    BlockMask = MAX_UINT64 >> BlockShift;

    DEBUG((DEBUG_VERBOSE, "%a(%d): %llx (Physical: %llx) - %llx set %lx clear %d\n", __func__,
           Level, RegionStart, PhysicalStart, RegionEnd, AttributeSetMask, ClearMapping));

    for (; RegionStart < RegionEnd; RegionStart = BlockEnd) {
        BlockEnd = MIN(RegionEnd, (RegionStart | BlockMask) + 1);
        Entry = &PageTable[(RegionStart >> (64 - BlockShift)) &
                           (TT_ENTRY_COUNT - 1)];

        /*  
         * 1. If RegionStart or BlockEnd is not aligned to the block size at this
         *  level, we will have to create a table mapping in order to map less
         *  than a block, and recurse to create the block or page entries at
         *  the next level. 
         * 2. No block mappings are allowed at all at level 0, so in that case, 
         *  we have to recurse unconditionally.
        
         * 3. One special case to take into account is any region that covers the
         *  page table itself: if we'd cover such a region with block mappings,
         *  we are more likely to end up in the situation later where we need to
         *  disable the MMU in order to update page table entries safely, so
         *  prefer page mappings in that particular case.
         */
        if ((Level == 0) || 
            (((RegionStart | BlockEnd) & BlockMask) != 0) ||
            ((Level < 3) && (((UINT64)PageTable & ~BlockMask) == RegionStart)) ||
            IsTableEntry(*Entry, Level)) {
            ASSERT(Level < 3);

            if (!IsTableEntry(*Entry, Level)) {
                /*
                 * If the region we are trying to map is already covered by a
                 * block entry with the right attributes, don't bother splitting
                 * it up.
                 */
                if (IsBlockEntry(*Entry, Level) &&
                    ((*Entry & TT_ATTRIBUTES_MASK) == AttributeSetMask)) {
                    continue;
                }

                /*
                 * No table entry exists yet, so we need to allocate a page
                 * table for the next level.
                 */
                TranslationTable = AllocatePages(1);
                if (TranslationTable == NULL) {
                    return EFI_OUT_OF_RESOURCES;
                }

                ZeroMem(TranslationTable, EFI_PAGE_SIZE);

                if (IsBlockEntry(*Entry, Level)) {
                    /*
                     * We are splitting an existing block entry, so we have to
                     * populate the new table with the attributes of the block
                     * entry it replaces.
                     */
                    Status = UpdateRegionMappingRecursive(
                        (RegionStart & ~BlockMask) + AddressOffset, 
                        RegionStart & ~BlockMask, 
                        (RegionStart | BlockMask) + 1,
                        *Entry & TT_ATTRIBUTES_MASK, 
                        TranslationTable,
                        Level + 1, 
                        ClearMapping,
                        FALSE);

                    if (EFI_ERROR(Status)) {
                        /*
                         * The range we passed to UpdateRegionMappingRecursive() 
                         * is block aligned, so it is guaranteed that no
                         * further pages were allocated by it, and so we only
                         * have to free the page we allocated here.
                         */
                        FreePages(TranslationTable, 1);
                        return Status;
                    }
                }

                NextTableIsLive = FALSE;
            } else {
                /*
                 * If a table entry already exists, we can just use it. 
                 */
                TranslationTable =
                    (VOID *)(UINTN)(*Entry & TT_ADDRESS_MASK_BLOCK_ENTRY);
                NextTableIsLive = TableIsLive;
            }

            /*
             * Recurse to the next level
             */
            Status = UpdateRegionMappingRecursive(
                RegionStart + AddressOffset, 
                RegionStart, 
                BlockEnd, 
                AttributeSetMask, 
                TranslationTable, 
                Level + 1, 
                ClearMapping,
                NextTableIsLive);
            if (EFI_ERROR(Status)) {
                if (!IsTableEntry(*Entry, Level)) {
                    /*
                     * We are creating a new table entry, so on failure, we can
                     * free all allocations we made recursively, given that the
                     * whole subhierarchy has not been wired into the live page
                     * tables yet. (This is not possible for existing table
                     * entries, since we cannot revert the modifications we made
                     * to the subhierarchy it represents.)
                     */
                    FreePageTablesRecursive(TranslationTable, Level + 1);
                }

                return Status;
            }

            if (!IsTableEntry(*Entry, Level)) {
                EntryValue = (UINTN)TranslationTable | TT_TYPE_TABLE_ENTRY;
                ReplaceTableEntry(Entry, EntryValue, RegionStart, BlockMask, TableIsLive && IsBlockEntry(*Entry, Level));
            }
        } else {
            if (!ClearMapping) {
                EntryValue = AttributeSetMask;
                EntryValue |= RegionStart + AddressOffset;
                EntryValue |=
                    (Level == 3) ? TT_TYPE_BLOCK_ENTRY_LEVEL3 : TT_TYPE_BLOCK_ENTRY;
            } else {
                EntryValue = 0;
            }

            // TODO：free PTP

            ReplaceTableEntry(Entry, EntryValue, RegionStart, BlockMask, FALSE);
        }
    }

    return EFI_SUCCESS;
}

STATIC
VOID
DumpPageTableRecursive(UINT64 *PageTable, UINT64 VirtualStart, UINTN Level)
{
    UINT64 RegionStart;
    UINT64 RegionEnd;
    UINTN BlockShift;
    UINT64 BlockMask;
    UINT64 BlockEnd;
    UINT64 *Entry;
    VOID *TranslationTable;

    RegionStart = VirtualStart;
    RegionEnd = VirtualStart + TT_BLOCK_ENTRY_SIZE_AT_LEVEL(Level) * TT_ENTRY_COUNT;
    BlockShift = (Level + 1) * BITS_PER_LEVEL + MIN_T0SZ;
    BlockMask = MAX_UINT64 >> BlockShift;

    for (; RegionStart < RegionEnd; RegionStart = BlockEnd) {
        BlockEnd = MIN(RegionEnd, (RegionStart | BlockMask) + 1);
        Entry = &PageTable[(RegionStart >> (64 - BlockShift)) &
                           (TT_ENTRY_COUNT - 1)];

        if ((Level == 0) || IsTableEntry(*Entry, Level)) {
            ASSERT(Level < 3);

            if (!IsTableEntry(*Entry, Level)) {
                if (IsBlockEntry(*Entry, Level)) {
                    DebugPrint(DEBUG_INFO, "0x%lx - 0x%lx: %lx\n", RegionStart, BlockEnd, *Entry);
                    continue;
                }
            } else {
                TranslationTable = (VOID *)(UINTN)(*Entry & TT_ADDRESS_MASK_BLOCK_ENTRY);

                /*
                 * Recurse to the next level
                 */
                DumpPageTableRecursive(
                    TranslationTable,
                    RegionStart, 
                    Level + 1
                    );
            }
        } else {
            if (IsBlockEntry(*Entry, Level)) {
                DebugPrint(DEBUG_INFO, "0x%lx - 0x%lx: %lx\n", RegionStart, BlockEnd, *Entry);
            }
        }
    }
}

EFI_STATUS
MapRangeInPageTable(
    IN OUT UINT64 *TranslationTableBasePtr,
    IN EFI_PHYSICAL_ADDRESS PhysicalStart,
    IN EFI_VIRTUAL_ADDRESS VirtualStart,
    IN EFI_VIRTUAL_ADDRESS VirtualEnd, 
    IN VMR_PROP_T Flags,
    IN BOOLEAN KernelVMR,
    IN BOOLEAN TableIsLive
    ) 
{
    EFI_STATUS Status;
    UINTN T0SZ;
    
    ASSERT(((VirtualStart | PhysicalStart | VirtualEnd) & EFI_PAGE_MASK) == 0);

    T0SZ = ArmGetTCR() & TCR_T0SZ_MASK;

    SBDebug("MapRangeInPageTable: 0x%lx - 0x%lx, Physical: 0x%lx\n", VirtualStart, VirtualEnd, PhysicalStart);

    Status = UpdateRegionMappingRecursive(
        PhysicalStart, VirtualStart, VirtualEnd, VmrPropToPageAttr(Flags, KernelVMR),
        (UINT64 *)*TranslationTableBasePtr, GetRootTableLevel(T0SZ), FALSE, TableIsLive);

    return Status;
}

EFI_STATUS UnmapRangeInPageTable (
    IN OUT UINT64 *TranslationTableBasePtr,
    IN EFI_VIRTUAL_ADDRESS VirtualStart,
    IN EFI_VIRTUAL_ADDRESS VirtualEnd,
    IN BOOLEAN TableIsLive
) {
    EFI_STATUS Status;
    UINTN T0SZ;
    
    ASSERT(((VirtualStart | VirtualEnd) & EFI_PAGE_MASK) == 0);

    T0SZ = ArmGetTCR() & TCR_T0SZ_MASK;

    Status = UpdateRegionMappingRecursive(
        0, VirtualStart, VirtualEnd, 0,
        (UINT64 *)*TranslationTableBasePtr, GetRootTableLevel(T0SZ), TRUE, TableIsLive);

    return Status;
}

EFI_STATUS
CreatePageTable(OUT UINT64 *TranslationTableBasePtr) 
{
    VOID *TranslationTable;
    UINTN MaxAddressBits;
    UINT64 MaxAddress;
    UINTN T0SZ;
    UINTN RootTableEntryCount;

    MaxAddressBits = MAX_VA_BITS;
    MaxAddress = LShiftU64(1ULL, MaxAddressBits) - 1;

    T0SZ = 64 - MaxAddressBits;
    RootTableEntryCount = GetRootTableEntryCount(T0SZ);

    // Allocate pages for translation table
    TranslationTable = AllocatePages(1);
    if (TranslationTable == NULL) {
        return EFI_OUT_OF_RESOURCES;
    }

    if (TranslationTableBasePtr != NULL) {
        *TranslationTableBasePtr = (UINT64)TranslationTable;
    }

    ZeroMem(TranslationTable, RootTableEntryCount * sizeof(UINT64));

    SBPrint("ArmCreatePageTable TranslationTable: %p\n", TranslationTable);

    return EFI_SUCCESS;
}

VOID FreePageTablesRecursive(IN UINT64 *TranslationTablePtr, IN UINTN Level) {
    UINTN Index;
    UINT64 *TranslationTable = (UINT64 *)(*TranslationTablePtr);

    ASSERT(Level <= 3);

    if (Level < 3) {
        for (Index = 0; Index < TT_ENTRY_COUNT; Index++) {
            if ((TranslationTable[Index] & TT_TYPE_MASK) == TT_TYPE_TABLE_ENTRY) {
                FreePageTablesRecursive(
                    (VOID *)(UINTN)(TranslationTable[Index] & TT_ADDRESS_MASK_BLOCK_ENTRY),
                    Level + 1);
            }
        }
    }

    FreePages(TranslationTable, 1);
}

EFI_STATUS CreateIdenticalPageTable(IN UINT64 SrcPageTable, IN OUT UINT64 *DstPageTable)
{
    __unimplemented("AArch64 CreateIdenticalPageTable");
    return EFI_UNSUPPORTED;
}

VOID PrintPageTable(UINT64 PageTable)
{
  DumpPageTableRecursive(
    (UINT64 *)PageTable,
    0,
    0
  );

  DEBUG((DEBUG_INFO, "==== Dump Completed ====\n"));
}

EFI_STATUS InitCorePageTable(UINT64 *CorePageTablePtr)
{
#if defined(__raspi4__)
  EFI_STATUS Status;

  // System RAM < 1GB
  Status = MapRangeInPageTable(CorePageTablePtr, 0x400000, PHYS_TO_VIRT(0x400000), PHYS_TO_VIRT(0x3B400000), VMR_READ | VMR_WRITE | VMR_EXEC,
                        TRUE, FALSE);
  if (EFI_ERROR(Status)) {
    SBError("Fail to map range in core page table\n");
  }

  // Extended System RAM < 4GB
  Status = MapRangeInPageTable(CorePageTablePtr, 0x40000000, PHYS_TO_VIRT(0x40000000), PHYS_TO_VIRT(0xFC000000), VMR_READ | VMR_WRITE | VMR_EXEC,
                       TRUE, FALSE);
  if (EFI_ERROR(Status)) {
    SBError("Fail to map range in core page table\n");
  }

  // Extended System RAM >= 4GB
  Status = MapRangeInPageTable(CorePageTablePtr, 0x100000000, PHYS_TO_VIRT(0x100000000), PHYS_TO_VIRT(0x200000000), VMR_READ | VMR_WRITE | VMR_EXEC,
                       TRUE, FALSE);
  if (EFI_ERROR(Status)) {
    SBError("Fail to map range in core page table\n");
  }
  
  return Status;
#else
  return MapRangeInPageTable(
      CorePageTablePtr, KERNEL_SYSTEM_DRAM_BASE,
      PHYS_TO_VIRT(KERNEL_SYSTEM_DRAM_BASE),
      PHYS_TO_VIRT(KERNEL_SYSTEM_DRAM_BASE + KERNEL_SYSTEM_DRAM_SIZE),
      VMR_READ | VMR_WRITE | VMR_EXEC, TRUE, FALSE);
#endif
}