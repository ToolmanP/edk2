#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include <Memory/Malloc.h>
#include <Memory/Memory.h>
#include <SandboxDxe.h>
#include <Utils/Logger.h>

VOID InitSandboxMallocManager(struct SandboxMallocManager *Manager) {
  InitializeListHead(&Manager->AllPageList);

  InitSandboxSlab(Manager);

  EfiInitializeLock(&Manager->MallocLock, TPL_NOTIFY);
  EfiInitializeLock(&Manager->SlabLock, TPL_NOTIFY);
}

struct SandboxPages *AddressToSandboxPages(struct SandboxMallocManager *Manager,
                                           EFI_PHYSICAL_ADDRESS Address) {
  LIST_ENTRY *Link;
  struct SandboxPages *SandboxPage;

  EfiAcquireLock(&Manager->MallocLock);

  BASE_LIST_FOR_EACH(Link, &Manager->AllPageList) {
    SandboxPage = BASE_CR(Link, struct SandboxPages, AllPageEntry);

    if (Address >= SandboxPage->PageStart && Address < SandboxPage->PageEnd) {
      EfiReleaseLock(&Manager->MallocLock);
      return SandboxPage;
    }
  }

  EfiReleaseLock(&Manager->MallocLock);
  return NULL;
}

/*
 * 1. Parameter 'Memory' points to the physical address of the allocated pages,
 * 2. The allocated pages are mapped to sandbox's VM space.
 */
struct SandboxPages *AllocateSandboxPages(IN UefiSandbox *Sandbox,
                                          IN EFI_ALLOCATE_TYPE Type,
                                          IN EFI_MEMORY_TYPE MemoryType,
                                          IN UINTN PageNum,
                                          IN OUT EFI_PHYSICAL_ADDRESS *Memory) {
  EFI_STATUS Status;
  struct SandboxPages *Pages;
  VMR_PROP_T VmrProp;

  Status = gBS->AllocatePages(Type, MemoryType, PageNum, Memory);
  if (EFI_ERROR(Status)) {
    *Memory = 0;
    return NULL;
  }

  Pages = (struct SandboxPages *)AllocatePool(sizeof(struct SandboxPages));
  Pages->PageStart = *Memory;
  Pages->PageEnd = *Memory + PAGE_SIZE * PageNum;
  Pages->Slab = NULL;

  EfiAcquireLock(&Sandbox->MallocManager->MallocLock);

  InsertTailList(&Sandbox->MallocManager->AllPageList, &Pages->AllPageEntry);

  if (MemoryType == EfiBootServicesCode ||
      MemoryType == EfiRuntimeServicesCode || MemoryType == EfiLoaderCode) {
    VmrProp = VMR_READ | VMR_EXEC;
  } else {
    VmrProp = VMR_READ | VMR_WRITE;
  }
  AddVMRegion(Sandbox, PHYS_TO_VIRT(*Memory), *Memory, PageNum * PAGE_SIZE,
              VmrProp, TRUE);

  EfiReleaseLock(&Sandbox->MallocManager->MallocLock);

  return Pages;
}

EFI_STATUS FreeSandboxPages(IN UefiSandbox *Sandbox,
                            IN EFI_PHYSICAL_ADDRESS Memory, IN UINTN PageNum,
                            IN struct SandboxPages *SandboxPage OPTIONAL) {
  LIST_ENTRY *PageEntry;
  struct SandboxPages *Pages;
  EFI_STATUS Status;

  if (SandboxPage != NULL) {
    Pages = SandboxPage;
  } else {
    /*
     * Check Pages belong to this sandbox
     */
    BASE_LIST_FOR_EACH(PageEntry, &Sandbox->MallocManager->AllPageList) {
      Pages = BASE_CR(PageEntry, struct SandboxPages, AllPageEntry);
      if (Pages->PageStart <= Memory && Pages->PageEnd > Memory) {
        break;
      }
    }

    if (PageEntry == &Sandbox->MallocManager->AllPageList) {
      SBError("Try to free other pages\n");
      return EFI_NOT_FOUND;
    }
  }

  ASSERT(Pages->Slab == NULL);

  EfiAcquireLock(&Sandbox->MallocManager->MallocLock);

  RemoveVMRegion(Sandbox, PHYS_TO_VIRT(Memory), Memory, PageNum * PAGE_SIZE,
                 TRUE);

  RemoveEntryList(&Pages->AllPageEntry);

  EfiReleaseLock(&Sandbox->MallocManager->MallocLock);

  /* Free SandboxPages metadata */
  FreePool(Pages);

  /* TODO: Is it possbile to only free part of allocated pages? */
  Status = gBS->FreePages(Memory, PageNum);

  return Status;
}

EFI_STATUS AllocateSandboxPool(IN UefiSandbox *Sandbox,
                               IN EFI_MEMORY_TYPE MemoryType, IN UINTN Size,
                               IN OUT EFI_PHYSICAL_ADDRESS *Address) {
  EFI_STATUS Status;
  VOID *Memory;
  UINTN AlignedSize;

  if (Size <= SLAB_MAX_SIZE) {
    Memory = AllocateInSandboxSlab(Sandbox, Size,
                                   (MemoryType == EfiBootServicesCode ||
                                    MemoryType == EfiRuntimeServicesCode ||
                                    MemoryType == EfiLoaderCode));
  } else {
    AlignedSize = ROUND_UP(Size, PAGE_SIZE);
    AllocateSandboxPages(Sandbox, AllocateAnyPages, MemoryType,
                         AlignedSize / PAGE_SIZE,
                         (EFI_PHYSICAL_ADDRESS *)&Memory);
  }

  if (Memory == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
  } else {
    Status = EFI_SUCCESS;
    *Address = (EFI_PHYSICAL_ADDRESS)Memory;
  }

  return Status;
}

EFI_STATUS FreeSandboxPool(IN UefiSandbox *Sandbox,
                           IN EFI_PHYSICAL_ADDRESS Address) {
  struct SandboxPages *SandboxPage;
  UINTN PageNum;

  SandboxPage = AddressToSandboxPages(Sandbox->MallocManager, Address);
  if (SandboxPage == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (SandboxPage->Slab != NULL) {
    FreeInSandboxSlab(Sandbox, Address, SandboxPage);
  } else {
    PageNum = (SandboxPage->PageEnd - SandboxPage->PageStart) / PAGE_SIZE;
    FreeSandboxPages(Sandbox, Address, PageNum, SandboxPage);
  }

  return EFI_SUCCESS;
}

/*
 * Allocate memory in CoreSandbox, these memory has to be manully mapped
 */
VOID *AllocateSandboxSharedMemory(UINTN Size) {
  EFI_STATUS Status;
  VOID *Buffer;

  Status = AllocateSandboxPool(&CoreSandbox, EfiBootServicesData, Size,
                               (EFI_PHYSICAL_ADDRESS *)&Buffer);
  if (EFI_ERROR(Status)) {
    return NULL;
  }

  return Buffer;
}

VOID *AllocateSandboxMemory(UEFI_SANDBOX *Sandbox, UINTN Size) {

  EFI_STATUS Status;
  VOID *Buffer;

  Status = AllocateSandboxPool(Sandbox, EfiBootServicesData, Size,
                               (EFI_PHYSICAL_ADDRESS *)&Buffer);
  if (EFI_ERROR(Status)) {
    return NULL;
  }

  return Buffer;
}

VOID *AllocateSandboxCodeBuffer(UEFI_SANDBOX *Sandbox, UINTN Size) {

  EFI_STATUS Status;
  VOID *Buffer;

  Status = AllocateSandboxPool(Sandbox, EfiBootServicesCode, Size,
                               (EFI_PHYSICAL_ADDRESS *)&Buffer);
  if (EFI_ERROR(Status)) {
    return NULL;
  }

  return Buffer;
}

VOID FreeSandboxSharedMemory(VOID *Buffer) {
  FreeSandboxPool(&CoreSandbox, (EFI_PHYSICAL_ADDRESS)Buffer);
}

/* Only used in CloseSandbox */
VOID FreeAllocatedMemory(UefiSandbox *Sandbox) {
  LIST_ENTRY *Link;
  struct SandboxPages *SandboxPage;

  Link = Sandbox->MallocManager->AllPageList.ForwardLink;
  while (Link != (&Sandbox->MallocManager->AllPageList)) {
    SandboxPage = BASE_CR(Link, struct SandboxPages, AllPageEntry);
    Link = Link->ForwardLink;

    if (SandboxPage->Slab != NULL) {
      FreePool(SandboxPage->Slab);
    }

    FreePages((VOID *)SandboxPage->PageStart,
              (SandboxPage->PageEnd - SandboxPage->PageStart) / PAGE_SIZE);
    FreePool(SandboxPage);
  }
}
