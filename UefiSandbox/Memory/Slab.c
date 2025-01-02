#include "UefiSandbox.h"
#include "Library/UefiLib.h"
#include "Malloc.h"
#include "Library/BaseLib.h"
#include "Library/DebugLib.h"
#include "Memory.h"
#include "Base.h"
#include "Library/MemoryAllocationLib.h"
#include "ProcessorBind.h"
#include "Uefi/UefiBaseType.h"
#include "Uefi/UefiMultiPhase.h"
#include "Uefi/UefiSpec.h"

#define SLAB_ORDER_TO_INDEX(Order) (Order - SLAB_MIN_ORDER)

STATIC inline UINT32 SizeToOrder(UINTN Size)
{
    UINT32 Order = SLAB_MIN_ORDER;

    while ((1 << Order) < Size) {
        Order++;
    }

    return Order;
}

STATIC struct SlabHeader *InitSlabCache(UefiSandbox *Sandbox, UINT32 Order, UINTN Size)
{
    VOID *Addr;
    struct SlabSlotList *Slot;
    struct SlabHeader *Slab;
    struct SandboxPages *SlabPage;
    UINTN Count;
    UINTN ObjSize;
    INT32 i;

    /* allocate pages for slab */
    SlabPage = AllocateSandboxPages(Sandbox, AllocateAnyPages, EfiBootServicesData, Size / PAGE_SIZE, (EFI_PHYSICAL_ADDRESS *)&Addr);
    if (SlabPage == NULL) {
        return NULL;
    }

    Slab = (struct SlabHeader *)AllocatePool(sizeof(struct SlabHeader));

    ObjSize = 1 << Order;
    Count = Size / ObjSize;

    Slot = (struct SlabSlotList *)((UINTN)Addr);
    Slab->FreeListHead = (VOID *)Slot;
    Slab->Order = Order;
    Slab->TotalFreeCount = Count;
    Slab->CurrentFreeCount = Count;

    for (i = 0; i < Count - 1; i++) {
        Slot->NextFree = (VOID *)((UINTN)Slot + ObjSize);
        Slot = (struct SlabSlotList *)((UINTN)Slot + ObjSize);
    }
    Slot->NextFree = NULL;

    /* set the pages as SlabPage */
    SlabPage->Slab = Slab;

    return Slab;
}

STATIC VOID ChooseNewCurrentSlab(struct SlabPointer *pool, UINT32 Order)
{
    LIST_ENTRY *List;
    struct SlabHeader *Slab;

    List = &(pool->PartialSlabList);
    if (IsListEmpty(List)) {
        pool->CurrentSlab = NULL;
    } else {
        Slab = BASE_CR(List->ForwardLink, struct SlabHeader, Node);
        pool->CurrentSlab = Slab;
        RemoveEntryList(List->ForwardLink);
    }
}

STATIC VOID TryInsertFullSlabToPartial(struct SandboxMallocManager *Manager, struct SlabHeader *Slab)
{
    if (Slab->CurrentFreeCount != 0) {
        return;
    }

    InsertTailList(&Slab->Node, &Manager->SlabPool[SLAB_ORDER_TO_INDEX(Slab->Order)].PartialSlabList);
}

STATIC VOID TryFreeSlabPage(UefiSandbox *Sandbox, struct SandboxPages *SlabPage)
{
    struct SlabHeader *Slab;

    Slab = SlabPage->Slab;

    if (Slab->CurrentFreeCount != Slab->TotalFreeCount) {
        return;
    }

    if (Slab == Sandbox->MallocManager->SlabPool[SLAB_ORDER_TO_INDEX(Slab->Order)].CurrentSlab) {
        ChooseNewCurrentSlab(&Sandbox->MallocManager->SlabPool[SLAB_ORDER_TO_INDEX(Slab->Order)], Slab->Order);
    } else {
        /* Remove Slab from PartialList */
        RemoveEntryList(&Slab->Node);
    }

    FreePool(Slab);
    SlabPage->Slab = NULL;

    /* Free allocated pages & free SandboxPages metadata */
    FreeSandboxPages(Sandbox, SlabPage->PageStart, (SlabPage->PageEnd - SlabPage->PageStart) / PAGE_SIZE, SlabPage);
}

VOID InitSandboxSlab(struct SandboxMallocManager *Manager)
{
    UINT32 Order;

    for (Order = SLAB_MIN_ORDER; Order <= SLAB_MAX_ORDER; Order++) {
        Manager->SlabPool[SLAB_ORDER_TO_INDEX(Order)].CurrentSlab = NULL;
        InitializeListHead(&Manager->SlabPool[SLAB_ORDER_TO_INDEX(Order)].PartialSlabList);
    }
}

VOID *AllocateInSandboxSlab(UefiSandbox *Sandbox, UINTN Size)
{
    UINT32 Order;
    struct SlabHeader *CurrentSlab;
    struct SlabSlotList *FreeList;
    VOID *NextSlot;

    if (Size > (1 << SLAB_MAX_ORDER)) {
        return NULL;
    }

    Order = SizeToOrder(Size);

    EfiAcquireLock(&Sandbox->MallocManager->SlabLock);

    CurrentSlab = Sandbox->MallocManager->SlabPool[SLAB_ORDER_TO_INDEX(Order)].CurrentSlab;
    if (CurrentSlab == NULL) {
        CurrentSlab = InitSlabCache(Sandbox, Order, SIZE_OF_ONE_SLAB);
        if (CurrentSlab == NULL) {
            EfiReleaseLock(&Sandbox->MallocManager->SlabLock);
            return NULL;
        }
        Sandbox->MallocManager->SlabPool[SLAB_ORDER_TO_INDEX(Order)].CurrentSlab = CurrentSlab;
    }

    FreeList = (struct SlabSlotList *)CurrentSlab->FreeListHead;

    NextSlot = FreeList->NextFree;
    CurrentSlab->FreeListHead = NextSlot;

    CurrentSlab->CurrentFreeCount--;
    if (CurrentSlab->CurrentFreeCount == 0) {
        ChooseNewCurrentSlab(&Sandbox->MallocManager->SlabPool[SLAB_ORDER_TO_INDEX(Order)], Order);
    }

    EfiReleaseLock(&Sandbox->MallocManager->SlabLock);

    return (VOID *)FreeList;
}

VOID FreeInSandboxSlab(UefiSandbox *Sandbox, EFI_PHYSICAL_ADDRESS Address, struct SandboxPages *SandboxPage)
{
    struct SandboxPages *SlabPage;
    struct SlabHeader *Slab;
    struct SlabSlotList *Slot;

    Slot = (struct SlabSlotList *)Address;
    SlabPage = SandboxPage;
    ASSERT(SlabPage != NULL);
    Slab = SlabPage->Slab;

    EfiAcquireLock(&Sandbox->MallocManager->SlabLock);

    TryInsertFullSlabToPartial(Sandbox->MallocManager, Slab);

    Slot->NextFree = Slab->FreeListHead;
    Slab->FreeListHead = Slot;
    Slab->CurrentFreeCount += 1;

    TryFreeSlabPage(Sandbox, SlabPage);

    EfiReleaseLock(&Sandbox->MallocManager->SlabLock);
}
