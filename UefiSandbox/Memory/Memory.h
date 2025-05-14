#ifndef SANDBOX_MEMORY_H_
#define SANDBOX_MEMORY_H_

#include "Base.h"
#include "ProcessorBind.h"
#include "Uefi/UefiBaseType.h"
#include "UefiSandbox.h"

#if defined(__x86_64__)
#include "X64/PageTable.h"
#elif defined(__aarch64__)
#include "AArch64/PageTable.h"
#endif

#define PAGE_SIZE EFI_PAGE_SIZE
#define DEFAULT_STACK_SIZE 0x8000UL

static inline EFI_VIRTUAL_ADDRESS TO_VIRT_ADDR(EFI_PHYSICAL_ADDRESS Addr) {
  return IS_VIRT_ADDR(Addr) ? (EFI_VIRTUAL_ADDRESS)Addr : PHYS_TO_VIRT(Addr);
}

static inline EFI_PHYSICAL_ADDRESS TO_PHYS_ADDR(EFI_VIRTUAL_ADDRESS Addr) {
  return IS_PHYS_ADDR(Addr) ? (EFI_PHYSICAL_ADDRESS)Addr : VIRT_TO_PHYS(Addr);
}

#define VMR_READ (1 << 0)
#define VMR_WRITE (1 << 1)
#define VMR_EXEC (1 << 2)
#define VMR_DEVICE (1 << 3)
#define VMR_NOCACHE (1 << 4)
#define VMR_COW (1 << 5)

#define ROUND_UP(x, n) (((x) + (n) - 1) & ~((n) - 1))
#define ROUND_DOWN(x, n) ((x) & ~((n) - 1))

typedef UINT64 VMR_PROP_T;

/* This struct represents one virtual memory region inside on address space */
typedef struct {
  LIST_ENTRY AllVMRegion; /* As one node of the vmr_list */

  EFI_VIRTUAL_ADDRESS Start;
  EFI_PHYSICAL_ADDRESS PhysicalStart;
  UINT64 Size;
  VMR_PROP_T Prop;
} VMRegion;

/* VMRegion operations */
EFI_STATUS AddVMRegion(IN UefiSandbox *Sandbox, IN EFI_VIRTUAL_ADDRESS Start,
                       IN EFI_PHYSICAL_ADDRESS PhysicalStart, IN UINT64 Size,
                       IN UINT64 Attributes, IN BOOLEAN TableIsLive);

EFI_STATUS RemoveVMRegion(IN UefiSandbox *Sandbox, IN EFI_VIRTUAL_ADDRESS Start,
                          IN EFI_PHYSICAL_ADDRESS PhysicalStart, IN UINT64 Size,
                          IN BOOLEAN TableIsLive);

EFI_STATUS ValidateVMRegion(IN UefiSandbox *Sandbox,
                            IN EFI_VIRTUAL_ADDRESS Address, IN UINT64 Size);

VOID FreeVMRegions(UefiSandbox *Sandbox);

/* page table operations */
EFI_STATUS CreatePageTable(OUT UINT64 *TranslationTableBasePtr);

VOID FreePageTablesRecursive(IN UINT64 *TranslationTablePtr, IN UINTN Level);

EFI_STATUS MapRangeInPageTable(IN OUT UINT64 *TranslationTableBasePtr,
                               IN EFI_PHYSICAL_ADDRESS PhysicalStart,
                               IN EFI_VIRTUAL_ADDRESS VirtualStart,
                               IN EFI_VIRTUAL_ADDRESS VirtualEnd,
                               IN VMR_PROP_T Flags, IN BOOLEAN KernelVMR,
                               IN BOOLEAN TableIsLive);
EFI_STATUS UnmapRangeInPageTable(IN OUT UINT64 *TranslationTableBasePtr,
                                 IN EFI_VIRTUAL_ADDRESS VirtualStart,
                                 IN EFI_VIRTUAL_ADDRESS VirtualEnd,
                                 IN BOOLEAN TableIsLive);

void SetPageTable(IN UEFI_SANDBOX *Sandbox);
EFI_PHYSICAL_ADDRESS GetPageTable(void);

EFI_STATUS CreateIdenticalPageTable(IN UINT64 SrcPageTable,
                                    IN OUT UINT64 *DstPageTable);
VOID PrintPageTable(UINT64 PageTable);

EFI_STATUS InitCorePageTable(UINT64 *CorePageTablePtr);

/* Cache Operation */
VOID FlushIcacheAll(VOID);
VOID FlushIcacheRange(UINT64 Start, UINT64 End);
VOID DcacheCleanAndInvalidateArea(UINT64 Start, UINT64 End);

#endif
