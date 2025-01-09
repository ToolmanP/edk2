#ifndef SANDBOX_MALLOC_H_
#define SANDBOX_MALLOC_H_

#include "UefiSandbox.h"
#include "Base.h"
#include "Library/UefiLib.h"
#include "Uefi/UefiBaseType.h"

#define SLAB_MIN_ORDER (5)
#define SLAB_MAX_ORDER (11)

#define SLAB_MAX_SIZE (1UL << SLAB_MAX_ORDER)

#define SIZE_OF_ONE_SLAB (128 * 1024)

struct SlabHeader {
    VOID *FreeListHead;
    /* Used in PartialSlabList */
    LIST_ENTRY Node;

    UINT32 Order;
    UINT32 TotalFreeCount;
    UINT32 CurrentFreeCount;
    BOOLEAN Executable;
};

struct SlabSlotList {
    VOID *NextFree;
};

struct SlabPointer {
    struct SlabHeader *CurrentSlab;
    LIST_ENTRY PartialSlabList;
};

/*
 * SandboxPages record the metadata of the allocated pages.
 * If several pages are allocated to be used as slab memory, variable Slab will be set.
 */
struct SandboxPages {
    LIST_ENTRY AllPageEntry;

    EFI_PHYSICAL_ADDRESS PageStart;
    EFI_PHYSICAL_ADDRESS PageEnd;

    struct SlabHeader *Slab;
};

struct SandboxMallocManager {
    /* record all the SandboxPages that have been allocated*/
    LIST_ENTRY AllPageList;

    EFI_LOCK MallocLock;
    EFI_LOCK SlabLock;

    /* Pointers to conveniently find SlabHeader, while corresponding SandboxPages also has pointer of SlabHeader */
    struct SlabPointer DataSlabPool[SLAB_MAX_ORDER - SLAB_MIN_ORDER + 1];
    struct SlabPointer CodeSlabPool[SLAB_MAX_ORDER - SLAB_MIN_ORDER + 1];
};

VOID InitSandboxMallocManager(struct SandboxMallocManager *Manager);

struct SandboxPages *AddressToSandboxPages(struct SandboxMallocManager *Manager, EFI_PHYSICAL_ADDRESS Address);

struct SandboxPages *AllocateSandboxPages(
    IN UefiSandbox *Sandbox,
    IN EFI_ALLOCATE_TYPE Type,
    IN EFI_MEMORY_TYPE MemoryType,
    IN UINTN PageNum,
    IN OUT EFI_PHYSICAL_ADDRESS *Memory
);

EFI_STATUS FreeSandboxPages(
    IN UefiSandbox *Sandbox,
    IN EFI_PHYSICAL_ADDRESS Memory,
    IN UINTN PageNum,
    IN struct SandboxPages *SandboxPage OPTIONAL
);

EFI_STATUS AllocateSandboxPool(
    IN UefiSandbox *Sandbox,
    IN EFI_MEMORY_TYPE MemoryType,
    IN UINTN Size,
    IN OUT EFI_PHYSICAL_ADDRESS *Address
);

EFI_STATUS FreeSandboxPool(
    IN UefiSandbox *Sandbox,
    IN EFI_PHYSICAL_ADDRESS Address
);

VOID InitSandboxSlab(struct SandboxMallocManager *Manager);
VOID *AllocateInSandboxSlab(UefiSandbox *Sandbox, UINTN Size, BOOLEAN Executable);
VOID FreeInSandboxSlab(UefiSandbox *Sandbox, EFI_PHYSICAL_ADDRESS Address, struct SandboxPages *SandboxPage);

VOID *AllocateSandboxMemory(UefiSandbox *Sandbox, UINTN Size);
VOID *AllocateSandboxCodeBuffer(UefiSandbox *Sandbox, UINTN Size);
VOID *AllocateSandboxSharedMemory(UINTN Size);
VOID FreeSandboxSharedMemory(VOID *Buffer);

VOID FreeAllocatedMemory(UefiSandbox *Sandbox);

#endif
