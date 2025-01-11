#include "UefiSandbox.h"
#include "Base.h"
#include "Library/BaseLib.h"
#include "Library/MemoryAllocationLib.h"
#include "Memory.h"
#include "Print.h"
#include "ProcessorBind.h"
#include "Uefi/UefiBaseType.h"

/*
 * Assume vmr will not overlap
 */
static VMRegion *FindVMRegion (
    IN UefiSandbox *Sandbox,
    IN EFI_VIRTUAL_ADDRESS Start,
    IN UINT64 Size
    )
{
    LIST_ENTRY *Link;
    VMRegion *Vmr;

    BASE_LIST_FOR_EACH(Link, &Sandbox->VMRegions) {
        Vmr = BASE_CR(Link, VMRegion, AllVMRegion);
        if ((Vmr->Start == Start) && (Vmr->Start + Vmr->Size == Start + Size)) {
            return Vmr;
        }
    }

    return NULL;
}

EFI_STATUS AddVMRegion (
    IN UefiSandbox *Sandbox,
    IN EFI_VIRTUAL_ADDRESS Start,
    IN EFI_PHYSICAL_ADDRESS PhysicalStart,
    IN UINT64 Size,
    IN VMR_PROP_T VmrProp,
    IN BOOLEAN TableIsLive
    )
{
    EFI_STATUS Status;
    EFI_VIRTUAL_ADDRESS AlignedStart;
    EFI_VIRTUAL_ADDRESS AlignedEnd;
    VMRegion *Vmr;

    if (Sandbox == &CoreSandbox) {
        return EFI_SUCCESS;
    }

    Vmr = FindVMRegion(Sandbox, Start, Size);
    if (Vmr != NULL) {
        SBError("VMRegion already exists, Start: %x, Size: %x\n", Start, Size);
        return EFI_SUCCESS;
    }

    Vmr = AllocatePool(sizeof(VMRegion));
    if (Vmr == NULL) {
        SBError("Fail to allocate VMRegion\n");
        return EFI_OUT_OF_RESOURCES;
    }

    Vmr->Start = Start;
    Vmr->PhysicalStart = PhysicalStart;
    Vmr->Prop = VmrProp;
    Vmr->Size = Size;

    /*
     * Align to page size
     */
    AlignedStart = ROUND_DOWN(Start, PAGE_SIZE);
    AlignedEnd = ROUND_UP(Size + Start, PAGE_SIZE);

    Status = MapRangeInPageTable(
        &Sandbox->TranslationTable,
        PhysicalStart - (Start - AlignedStart),
        AlignedStart,
        AlignedEnd,
        VmrProp,
        FALSE,
        TableIsLive
    );

    if (EFI_ERROR(Status)) {
        SBError("Fail to map range in page table\n");
        FreePool(Vmr);
        return Status;
    }

    InsertTailList(&Sandbox->VMRegions, &Vmr->AllVMRegion);

    return Status;
}

EFI_STATUS ValidateVMRegion(IN UefiSandbox *Sandbox,
                            IN EFI_VIRTUAL_ADDRESS Address, IN UINT64 Size){
  LIST_ENTRY *Link;
  BASE_LIST_FOR_EACH(Link, &Sandbox->VMRegions) {
    VMRegion *Vmr = BASE_CR(Link, VMRegion, AllVMRegion);
    if(Size > Vmr->Size)
      continue;
    if (Address >= Vmr->Start && Address < Vmr->Start + Vmr->Size) {
      return EFI_SUCCESS;
    }
  }
  SBError("Address: %p, Size: 0x%lx not in VMRegion\n", Address, Size);
  return EFI_INVALID_PARAMETER;
}

EFI_STATUS RemoveVMRegion (
    IN UefiSandbox *Sandbox,
    IN EFI_VIRTUAL_ADDRESS Start,
    IN EFI_PHYSICAL_ADDRESS PhysicalStart,
    IN UINT64 Size,
    IN BOOLEAN TableIsLive
) {
    EFI_STATUS Status;
    EFI_VIRTUAL_ADDRESS AlignedStart;
    EFI_VIRTUAL_ADDRESS AlignedEnd;
    VMRegion *Vmr;

    if (Sandbox == &CoreSandbox) {
        return EFI_SUCCESS;
    }

    Vmr = FindVMRegion(Sandbox, Start, Size);
    if (Vmr == NULL) {
        SBError("VMRegion not found, Start: %x, Size: %x\n", Start, Size);
        return EFI_NOT_FOUND;
    }

    /*
     * Align to page size
     */
    AlignedStart = ROUND_DOWN(Start, PAGE_SIZE);
    AlignedEnd = ROUND_UP(Size + Start, PAGE_SIZE);

    Status = UnmapRangeInPageTable(
        &Sandbox->TranslationTable,
        AlignedStart,
        AlignedEnd,
        TableIsLive
    );

    if (EFI_ERROR(Status)) {
        SBError("Fail to unmap range in page table\n");
        FreePool(Vmr);
        return Status;
    }

    RemoveEntryList(&Vmr->AllVMRegion);

    FreePool(Vmr);

    return Status;
}

/*
 * Only used in CloseSandbox, so no need to unmap range in page table
 * as page tables will be freed recursively later.
 */
VOID FreeVMRegions (UefiSandbox *Sandbox)
{
    LIST_ENTRY *Link;
    VMRegion *Vmr;

    Link = Sandbox->VMRegions.ForwardLink;
    while (Link != &Sandbox->VMRegions) {
        Vmr = BASE_CR(Link, VMRegion, AllVMRegion);
        Link = Link->ForwardLink;

        FreePool(Vmr);
    }
}
