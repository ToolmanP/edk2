#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/CacheMaintenanceLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/Sandbox.h>

#include <BinaryGen/BinaryGen.h>
#include <Exception/Exception.h>
#include <Interface/Registry.h>
#include <Memory/Malloc.h>
#include <Memory/Memory.h>
#include <Proxy/ProtocolProxy.h>
#include <SandboxDxe.h>
#include <Sched/Sched.h>
#include <SystemTable/SystemTable.h>
#include <Utils/Logger.h>

LIST_ENTRY mSandboxList = INITIALIZE_LIST_HEAD_VARIABLE(mSandboxList);

/*
 * CoreSandbox is used to record core page table and offer shared memory and is
 * not included in mSandboxList.
 */
UefiSandbox CoreSandbox;
UefiSandbox *CurrentSandbox = NULL;

STATIC UINTN GetNextSandBoxID() {
  static UINTN SandboxID = 0;
  return SandboxID++;
}

UefiSandbox *FindSandbox(UINTN SandboxID) {
  LIST_ENTRY *Link;
  UefiSandbox *Sandbox;
  UefiSandbox *Item;

  Sandbox = NULL;

  for (Link = mSandboxList.ForwardLink; Link != &mSandboxList;
       Link = Link->ForwardLink) {
    Item = BASE_CR(Link, UefiSandbox, SandboxListNode);

    if (Item->SandboxID == SandboxID) {
      Sandbox = Item;
      break;
    }
  }

  return Sandbox;
}

EFI_STATUS
EFIAPI
CreateSandbox(IN EFI_SANDBOX_ARCH_PROTOCOL *This,
              IN EFI_SANDBOX_IMAGE_DATA ImageData, OUT UINTN *SandboxID) {
  EFI_STATUS Status;
  UefiSandbox *Sandbox;

  Sandbox = AllocatePool(sizeof(UefiSandbox));
  if (Sandbox == NULL) {
    SBError("Fail to allocate memory for sandbox\n");
    Status = EFI_OUT_OF_RESOURCES;
    goto out;
  }

  ZeroMem(Sandbox, sizeof(UefiSandbox));

  InitializeListHead(&Sandbox->VMRegions);
  /*
   * Allocate memory for the stack and map it in the sandbox page table
   */
  Sandbox->Context.StackBase =
      (EFI_PHYSICAL_ADDRESS)AllocatePages(DEFAULT_STACK_SIZE / PAGE_SIZE);
  if (Sandbox->Context.StackBase == 0) {
    SBError("Fail to allocate memory for stack\n");
    Status = EFI_OUT_OF_RESOURCES;
    goto free_sandbox;
  }

  /*
   * Allocate page table
   */
  Status = CreatePageTable(&Sandbox->TranslationTable);
  CreateIdenticalPageTable(&CoreSandbox.TranslationTable,
                           &Sandbox->TranslationTable);
  if (EFI_ERROR(Status)) {
    SBError("Fail to create user page table\n");
    goto free_sandbox;
  }

  Sandbox->MallocManager = AllocatePool(sizeof(struct SandboxMallocManager));
  InitSandboxMallocManager(Sandbox->MallocManager);

  /* TODO: Can we map MMIO region directly? */
  /*
   * Map MMIO region
   */
  Status = AddVMRegion(Sandbox, 0x8000000, 0x8000000, 0x8000000,
                       VMR_READ | VMR_WRITE, FALSE);

  ASSERT_EFI_ERROR(Status);

  Status = AddVMRegion(Sandbox, PHYS_TO_VIRT(Sandbox->Context.StackBase),
                       (EFI_PHYSICAL_ADDRESS)Sandbox->Context.StackBase,
                       DEFAULT_STACK_SIZE, VMR_READ | VMR_WRITE, FALSE);
  ASSERT_EFI_ERROR(Status);

  /*
   * Map image in sandbox page table
   */
  Status = AddVMRegion(Sandbox, PHYS_TO_VIRT(ImageData.Info.ImageBase),
                       (EFI_PHYSICAL_ADDRESS)ImageData.Info.ImageBase,
                       ImageData.Info.ImageSize,
                       VMR_READ | VMR_WRITE | VMR_EXEC, FALSE);
  ASSERT_EFI_ERROR(Status);

  /*
   * Initialize the sandbox system table
   */

  Sandbox->ImageData = ImageData;
  Status = InitAndMapSandboxSystemTable(Sandbox, ImageData.Info.SystemTable);
  if (EFI_ERROR(Status)) {
    SBError("Fail to initialize sandbox system table\n");
    goto free_stack;
  }

  InitializeListHead(&Sandbox->LocatedInterfaces);
  InitializeListHead(&Sandbox->InstalledInterfaces);
  InsertTailList(&mSandboxList, &Sandbox->SandboxListNode);
  Sandbox->SandboxID = GetNextSandBoxID();
  *SandboxID = Sandbox->SandboxID;
  return Status;

free_stack:
  FreePool((VOID *)Sandbox->Context.StackBase);

free_sandbox:
  FreePool(Sandbox);

out:
  *SandboxID = -1;
  return Status;
}

EFI_STATUS
EFIAPI
StartSandbox(IN EFI_SANDBOX_ARCH_PROTOCOL *This, EFI_HANDLE Handle,
             IN UINTN SandboxID) {
  EFI_STATUS Status;
  UefiSandbox *Sandbox;
  VOID *ReturnTrampoline;
  UINTN SetJumpFlag;

  Sandbox = FindSandbox(SandboxID);
  if (Sandbox == NULL) {
    SBError("Sandbox not found\n");
    Status = EFI_NOT_FOUND;
    return Status;
  }

  Sandbox->JumpBuffer = AllocatePool(sizeof(BASE_LIBRARY_JUMP_BUFFER) +
                                     BASE_LIBRARY_JUMP_BUFFER_ALIGNMENT);
  if (Sandbox->JumpBuffer == NULL) {
    SBError("Fail to allocate memory for jump buffer\n");
    Status = EFI_OUT_OF_RESOURCES;
    return Status;
  }

  Sandbox->JumpContext =
      ALIGN_POINTER(Sandbox->JumpBuffer, BASE_LIBRARY_JUMP_BUFFER_ALIGNMENT);

  ReturnTrampoline =
      CreateSandboxReturnTrampoline(Sandbox, (UINT64)Sandbox->JumpContext);

  /*
   *  SetJump will record x19-x30 registers and stack pointer,
   *  LongJump will restore these registers.
   */
  SetJumpFlag = SetJump(Sandbox->JumpContext);

  if (SetJumpFlag == 0) {
    EFI_SYSTEM_CONTEXT_AARCH64 Context;

    Context.X0 = (EFI_PHYSICAL_ADDRESS)(Handle);
    /* Sandbox SystemTable pointer has been set to virtual address when creating
     * sandbox */
    Context.X1 = (EFI_VIRTUAL_ADDRESS)Sandbox->ImageData.Info.SystemTable;

    Context.SP = PHYS_TO_VIRT(Sandbox->Context.StackBase + DEFAULT_STACK_SIZE);
    Context.ELR = (EFI_VIRTUAL_ADDRESS)Sandbox->ImageData.EntryPoint;
    Context.SPSR = SPSR_EL1_USER;
    Context.LR = PHYS_TO_VIRT(ReturnTrampoline); // TODO: what now

    /* TODO: shrink the range */
    DcacheCleanAndInvaliateArea(KERNEL_SYSTEM_DRAM_BASE,
                                KERNEL_SYSTEM_DRAM_BASE +
                                    KERNEL_SYSTEM_DRAM_SIZE);
    FlushIcacheAll();
    ScheduleToSandboxInternal(Sandbox, TRUE);
    EretToSandbox(&Context);
  }

  ScheduleToSandboxInternal(&CoreSandbox, TRUE);

  ZeroMem((VOID *)Sandbox->Context.StackBase, DEFAULT_STACK_SIZE);
  FreePool(Sandbox->JumpBuffer);

  Status = SetJumpFlag - 1;

  SBDebug("SandboxStart Finished\n");

  return Status;
}

EFI_STATUS
EFIAPI
CloseSandbox(IN EFI_SANDBOX_ARCH_PROTOCOL *This, UINTN SandboxID) {
  EFI_STATUS Status;
  UefiSandbox *Sandbox;

  Sandbox = FindSandbox(SandboxID);
  if (Sandbox == NULL) {
    SBError("Sandbox not found\n");
    Status = EFI_NOT_FOUND;
    return Status;
  }

  RemoveEntryList(&Sandbox->SandboxListNode);

  /* Free Sandbox's Interface pointers */
  FreeSandboxInterfaces(Sandbox);

  /* Free Sandbox allocated pages */
  FreeAllocatedMemory(Sandbox);

  FreePool(Sandbox->MallocManager);

  /* Free Sandbox system table */
  FreeSandboxSystemTable(Sandbox);

  /* Free Sandbox Stack */
  FreePages((VOID *)Sandbox->Context.StackBase, DEFAULT_STACK_SIZE / PAGE_SIZE);

  /* Free VMRegions */
  FreeVMRegions(Sandbox);

  /* Free Sandbox Translation Table */
  FreePageTablesRecursive(&Sandbox->TranslationTable, 0);

  FreePool(Sandbox);

  SBPrint("Sandbox %d closed\n", SandboxID);

  Status = EFI_SUCCESS;
  return Status;
}

EFI_STATUS
EFIAPI
ScheduleToSandbox(IN EFI_SANDBOX_ARCH_PROTOCOL *This, IN OUT UINTN *SandboxID,
                  IN BOOLEAN TplRaise) {
  UefiSandbox *Sandbox;

  if (*SandboxID == 0) {
    Sandbox = &CoreSandbox;
  } else {
    Sandbox = FindSandbox(*SandboxID);
    if (Sandbox == NULL) {
      return EFI_NOT_FOUND;
    }
  }

  *SandboxID = ScheduleToSandboxInternal(Sandbox, TplRaise)->SandboxID;

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
GetCurrentSandboxID(IN EFI_SANDBOX_ARCH_PROTOCOL *This, OUT UINTN *SandboxID) {
  *SandboxID = CurrentSandbox->SandboxID;

  return EFI_SUCCESS;
}

//
// Globals used to initialize the protocol
//
EFI_HANDLE mSandboxHandle = NULL;
EFI_SANDBOX_ARCH_PROTOCOL mSandbox = {CreateSandbox, StartSandbox, CloseSandbox,
                                      ScheduleToSandbox, GetCurrentSandboxID};

EFI_STATUS
EFIAPI
SandboxInitialize(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable) {
  EFI_STATUS Status;
  UINT64 CoreTranslationTableBase;

  /*
   * Initialize Protocol Database
   */
  Status = InitProtocolDB();
  if (EFI_ERROR(Status)) {
    __unreachable("Fail to initialize protocol database\n");
  }

  Status = RegisterSyncExceptionHandler(FALSE);
  ASSERT_EFI_ERROR(Status);

  CoreTranslationTableBase = GetPageTable();
  DEBUG((DEBUG_INFO, "CoreTranslationTableBase: 0x%p\n",
         CoreTranslationTableBase));

  Status = InitCorePageTable(&CoreTranslationTableBase);

  if (EFI_ERROR(Status))
    __unreachable("Fail to initialize core page table\n");

  /* Initial IdleSandbox */
  CoreSandbox.SandboxID = GetNextSandBoxID();
  CoreSandbox.TranslationTable = CoreTranslationTableBase;

  InitializeListHead(&CoreSandbox.LocatedInterfaces);
  InitializeListHead(&CoreSandbox.InstalledInterfaces);
  InitializeListHead(&CoreSandbox.VMRegions);

  /* TODO: when will this memory be freed? */
  CoreSandbox.MallocManager = AllocatePool(sizeof(struct SandboxMallocManager));
  InitSandboxMallocManager(CoreSandbox.MallocManager);

  CurrentSandbox = &CoreSandbox;

  mSandbox.UserAddressBase = USER_BASE;
  Status = gBS->InstallProtocolInterface(&mSandboxHandle,
                                         &gEfiSandboxArchProtocolGuid,
                                         EFI_NATIVE_INTERFACE, &mSandbox);
  ASSERT_EFI_ERROR(Status);

  return Status;
}
