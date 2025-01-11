#include "BinaryGen/BinaryGen.h"
#include "Exception.h"
#include "Interface/Registry.h"
#include "Library/ArmLib.h"
#include "ProcessorBind.h"
#include "Proxy/ProtocolProxy.h"
#include "SystemTable/SystemTable.h"
#include "Memory/Memory.h"
#include "Memory/Malloc.h"
#include "Memory/Memory.h"
#include "Print.h"
#include "Proxy/ProtocolProxy.h"
#include "Sched/Sched.h"
#include "Init/ArchInit.h"

#include "Base.h"
#include "Library/BaseLib.h"
#include "Library/BaseMemoryLib.h"
#include "Library/DebugLib.h"
#include "Library/MemoryAllocationLib.h"
#include "Library/UefiBootServicesTableLib.h"
#include "Library/UefiLib.h"
#include "Protocol/DebugSupport.h"
#include "Protocol/Sandbox.h"
#include "Uefi/UefiBaseType.h"
#include "Uefi/UefiMultiPhase.h"
#include "Uefi/UefiSpec.h"
#include "UefiSandbox.h"

#define ProtocolDBTest 0
#if ProtocolDBTest
#include "Protocol/DiskIo.h"
#include "Protocol/SerialIo.h"
#include "Protocol/SimpleFileSystem.h"
#endif

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

#if defined(__x86_64__)
  Status = CreateIdenticalPageTable(CoreSandbox.TranslationTable, &Sandbox->TranslationTable);
  if (EFI_ERROR(Status)) {
    SBError("MapExistingMappings failed: %r\n", Status);
    goto free_stack;
  }

  /*
   * Map the FV region
   */
  Status = AddVMRegion(Sandbox, 0x800000, 0x800000, 0x1400000, VMR_READ | VMR_WRITE, FALSE);
#elif defined(__aarch64__)
  /*
   * Allocate page table
   */
  Status = CreatePageTable(&Sandbox->TranslationTable);
  if (EFI_ERROR(Status)) {
    SBError("Fail to create user page table\n");
    goto free_sandbox;
  }

#if defined(__raspi4__)
  // FD
  Status = MapRangeInPageTable(&Sandbox->TranslationTable, 0x0, 0x0, 0x3C0000, VMR_READ | VMR_WRITE | VMR_EXEC,
                        TRUE, FALSE);
  if (EFI_ERROR(Status)) {
    SBError("Fail to map range in user page table\n");
    goto free_sandbox;
  }

  // FD Variables
  Status = MapRangeInPageTable(&Sandbox->TranslationTable, 0x3C0000, 0x3C0000, 0x3E0000, VMR_READ | VMR_WRITE,
                        FALSE, FALSE);
  if (EFI_ERROR(Status)) {
    SBError("Fail to map range in user page table\n");
    goto free_sandbox;
  }

  // Flattened Device Tree
  Status = MapRangeInPageTable(&Sandbox->TranslationTable, 0x3E0000, 0x3E0000, 0x3F0000, VMR_READ | VMR_WRITE,
                        FALSE, FALSE);
  if (EFI_ERROR(Status)) {
    SBError("Fail to map range in user page table\n");
    goto free_sandbox;
  }

  // System RAM < 1GB
  Status = MapRangeInPageTable(&Sandbox->TranslationTable, 0x400000, 0x400000, 0x3B400000, VMR_READ | VMR_WRITE | VMR_EXEC,
                        TRUE, FALSE);
  if (EFI_ERROR(Status)) {
    SBError("Fail to map range in user page table\n");
    goto free_sandbox;
  }

  // GPU Reserved
  Status = MapRangeInPageTable(&Sandbox->TranslationTable, 0x3B400000, 0x3B400000, 0x40000000, VMR_READ | VMR_WRITE | VMR_DEVICE,
                       FALSE, FALSE);
  if (EFI_ERROR(Status)) {
    SBError("Fail to map range in user page table\n");
    goto free_sandbox;
  }

  // SoC Reserved
  Status = MapRangeInPageTable(&Sandbox->TranslationTable, 0xFC000000, 0xFC000000, 0x100000000, VMR_READ | VMR_WRITE | VMR_DEVICE,
                       FALSE, FALSE);
  if (EFI_ERROR(Status)) {
    SBError("Fail to map range in user page table\n");
    goto free_sandbox;
  }

  // Extended System RAM < 4GB
  Status = MapRangeInPageTable(&Sandbox->TranslationTable, 0x40000000, 0x40000000, 0xFC000000, VMR_READ | VMR_WRITE | VMR_EXEC,
                       TRUE, FALSE);
  if (EFI_ERROR(Status)) {
    SBError("Fail to map range in user page table\n");
    goto free_sandbox;
  }

  // Extended System RAM >= 4GB
  Status = MapRangeInPageTable(&Sandbox->TranslationTable, 0x100000000, 0x100000000, 0x200000000, VMR_READ | VMR_WRITE | VMR_EXEC,
                       TRUE, FALSE);
  if (EFI_ERROR(Status)) {
    SBError("Fail to map range in user page table\n");
    goto free_sandbox;
  }
#else
  /* TODO: can we map FV region directly? */
  // FIXME: shoule be readonly?
  /*
   * Map the FV region as normal executable memory
   */
  Status = MapRangeInPageTable(&Sandbox->TranslationTable, 0x1000, 0x1000, 0x1FF000, VMR_READ | VMR_EXEC,
                       TRUE, FALSE);

  ASSERT_EFI_ERROR(Status);

  /* TODO: Can we map MMIO region directly? */
  /*
   * Map MMIO region
   */
  Status = AddVMRegion(Sandbox, 0x8000000, 0x8000000, 0x8000000,
                       VMR_READ | VMR_WRITE, FALSE);

  ASSERT_EFI_ERROR(Status);
  /*
   * Map kernel range in sandbox page table, user can't access kernel memory
   */
  Status = MapRangeInPageTable(
      &Sandbox->TranslationTable, KERNEL_SYSTEM_DRAM_BASE,
      KERNEL_SYSTEM_DRAM_BASE,
      KERNEL_SYSTEM_DRAM_BASE + KERNEL_SYSTEM_DRAM_SIZE,
      VMR_READ | VMR_WRITE | VMR_EXEC, TRUE, FALSE);
  ASSERT(!EFI_ERROR(Status));
#endif
#endif

  Status = AddVMRegion(Sandbox, PHYS_TO_VIRT(Sandbox->Context.StackBase),
                       (EFI_PHYSICAL_ADDRESS)Sandbox->Context.StackBase,
                       DEFAULT_STACK_SIZE, VMR_READ | VMR_WRITE, FALSE);
  ASSERT(!EFI_ERROR(Status));

  /*
   * Map image in sandbox page table
   */
  Status = AddVMRegion(Sandbox, PHYS_TO_VIRT(ImageData.Info.ImageBase),
                       (EFI_PHYSICAL_ADDRESS)ImageData.Info.ImageBase,
                       ImageData.Info.ImageSize,
                       VMR_READ | VMR_WRITE | VMR_EXEC, FALSE);
  ASSERT(!EFI_ERROR(Status));

  Sandbox->ImageData = ImageData;

  /*
   * Initialize the sandbox system table
   */
  Status = InitAndMapSandboxSystemTable(Sandbox, ImageData.Info.SystemTable);
  if (EFI_ERROR(Status)) {
    SBError("Fail to initialize sandbox system table\n");
    goto free_stack;
  }

  Sandbox->MallocManager = AllocatePool(sizeof(struct SandboxMallocManager));
  InitSandboxMallocManager(Sandbox->MallocManager);

  InitializeListHead(&Sandbox->LocatedInterfaces);
  InitializeListHead(&Sandbox->InstalledInterfaces);

  InitializeListHead(&Sandbox->SVCTrampolineList);
  EfiInitializeLock(&Sandbox->SVCTrampolineLock, TPL_NOTIFY);
  InsertTailList(&mSandboxList, &Sandbox->SandboxListNode);
  Sandbox->SandboxID = GetNextSandBoxID();
  *SandboxID = Sandbox->SandboxID;

  for(int i =SLAB_MIN_ORDER;  i <= SLAB_MAX_ORDER; i++) {
    AllocateSandboxMemory(Sandbox, 1 << i);
  }

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
StartSandbox(IN EFI_SANDBOX_ARCH_PROTOCOL *This, EFI_HANDLE Handle, IN UINTN SandboxID) {
  EFI_STATUS Status;
  UefiSandbox *Sandbox;
  VOID *ReturnTrampoline;
  UINTN SetJumpFlag;

  DisableInterrupts();

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

  ReturnTrampoline = CreateSandboxReturnTrampoline(Sandbox, (UINT64)Sandbox->JumpContext);

  /*
   *  SetJump will record x19-x30 registers and stack pointer,
   *  LongJump will restore these registers.
   */
  SetJumpFlag = SetJump(Sandbox->JumpContext);

  if (SetJumpFlag == 0) {
#if defined(__x86_64__)
    EFI_SYSTEM_CONTEXT_X64 Context;
    ZeroMem(&Context, sizeof(EFI_SYSTEM_CONTEXT_X64));

    Context.Rcx = (EFI_VIRTUAL_ADDRESS)Sandbox->ImageData.EntryPoint;
    Context.Rdx = PHYS_TO_VIRT(ReturnTrampoline);
    Context.R8  = PHYS_TO_VIRT(Sandbox->Context.StackBase + DEFAULT_STACK_SIZE);
    Context.R9  = (EFI_PHYSICAL_ADDRESS)Handle;
    Context.Rax = (EFI_VIRTUAL_ADDRESS)Sandbox->ImageData.Info.SystemTable;

    ASSERT(&CoreSandbox == ScheduleToSandboxInternal(Sandbox, TRUE));

    SBDebug("Starting Sandbox %d, PageTable: 0x%lx\n, ImageHandle: 0x%lx, SystemTable: 0x%lx\n", Sandbox->SandboxID, (UINT64)Sandbox->TranslationTable, Context.R9, Context.Rax);

    IretToSandbox(&Context);
#elif defined(__aarch64__)
    EFI_SYSTEM_CONTEXT_AARCH64 Context;

    Context.X0 = (EFI_PHYSICAL_ADDRESS)(Handle);
    /* Sandbox SystemTable pointer has been set to virtual address when creating
     * sandbox */
    Context.X1 = (EFI_VIRTUAL_ADDRESS)Sandbox->ImageData.Info.SystemTable;

    Context.SP = PHYS_TO_VIRT(Sandbox->Context.StackBase + DEFAULT_STACK_SIZE);
    Context.ELR = (EFI_VIRTUAL_ADDRESS)Sandbox->ImageData.EntryPoint;
    Context.SPSR = SPSR_EL1_USER;
    Context.LR = PHYS_TO_VIRT(ReturnTrampoline); // TODO: what now

    DcacheCleanAndInvalidateArea((UINT64)ReturnTrampoline, (UINT64)ReturnTrampoline + 256);
    FlushIcacheRange((UINT64)ReturnTrampoline, (UINT64)ReturnTrampoline + 256);

    DcacheCleanAndInvalidateArea((UINT64)Sandbox->ImageData.Info.ImageBase, Sandbox->ImageData.Info.ImageSize);
    FlushIcacheRange((UINT64)Sandbox->ImageData.Info.ImageBase, Sandbox->ImageData.Info.ImageSize);

    DcacheCleanAndInvalidateArea(Sandbox->Context.StackBase, Sandbox->Context.StackBase + DEFAULT_STACK_SIZE);
 
    SBDebug("Starting Sandbox %d, PageTable: 0x%lx, ImageHandle: 0x%lx, SystemTable: 0x%lx\n", Sandbox->SandboxID, (UINT64)Sandbox->TranslationTable, Context.X0, Context.X1);

    ASSERT(&CoreSandbox == ScheduleToSandboxInternal(Sandbox, TRUE));

    EretToSandbox(&Context);
#endif
  }

  ASSERT(Sandbox == ScheduleToSandboxInternal(&CoreSandbox, TRUE));

  ZeroMem((VOID *)Sandbox->Context.StackBase, DEFAULT_STACK_SIZE);
  FreePool(Sandbox->JumpBuffer);

  Status = SetJumpFlag - 1;

  SBDebug("StartSandbox %d Finished\n", SandboxID);

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

  SBDebug("Sandbox %d closed\n", SandboxID);

  Status = EFI_SUCCESS;
  return Status;
}

EFI_STATUS
EFIAPI
ScheduleToSandbox(IN EFI_SANDBOX_ARCH_PROTOCOL *This, IN OUT UINTN *SandboxID, IN BOOLEAN TplRaise) {
  UefiSandbox *Sandbox;

  if (*SandboxID == 0) {
    Sandbox = &CoreSandbox;
  } else {
    Sandbox = FindSandbox(*SandboxID);
    if (Sandbox == NULL) {
      SBError("Sandbox %d not found\n", *SandboxID);
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

  DebugPrint(DEBUG_INFO, "Frequency: %ld\n", ArmReadCntFrq());

  Status = ArchInit();
  if (EFI_ERROR(Status)) {
    SBError("ArchInit failed: %r\n", Status);
    return Status;
  }

  /*
   * Initialize Protocol Database
   */
  Status = InitProtocolDB();
  if (EFI_ERROR(Status)) {
    SBError("Fail to initialize protocol database\n");
    ASSERT(FALSE);
  }

#if ProtocolDBTest
  struct Protocol *Protocol;
  EFI_GUID Guid1 = EFI_SERIAL_IO_PROTOCOL_GUID;
  Status = GetProtocol(&Guid1, &Protocol);
  if (EFI_ERROR(Status)) {
    SBError("Fail to get SerialIo protocol\n");
    ASSERT(FALSE);
  }

  EFI_GUID Guid2 = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
  Status = GetProtocol(&Guid2, &Protocol);
  if (EFI_ERROR(Status)) {
    SBError("Fail to get SimpleFileSystem protocol\n");
    ASSERT(FALSE);
  }

  EFI_GUID Guid3 = EFI_DISK_IO_PROTOCOL_GUID;
  Status = GetProtocol(&Guid3, &Protocol);
  if (EFI_ERROR(Status)) {
    SBError("Fail to get DiskIo protocol\n");
    ASSERT(FALSE);
  }

  SBDebug("Pass Protocol Analyze Test\n");
#endif

  Status = RegisterSyncExceptionHandler(FALSE);
  ASSERT_EFI_ERROR(Status);

  CoreTranslationTableBase = GetPageTable();
  DEBUG((DEBUG_INFO, "CoreTranslationTableBase: 0x%p\n", CoreTranslationTableBase));

  Status = InitCorePageTable(&CoreTranslationTableBase);
  if (EFI_ERROR(Status)) {
    SBError("Fail to initialize core page table\n");
    return Status;
  }

  /* Initial IdleSandbox */
  CoreSandbox.SandboxID = GetNextSandBoxID();
  CoreSandbox.TranslationTable = CoreTranslationTableBase;

  InitializeListHead(&CoreSandbox.LocatedInterfaces);
  InitializeListHead(&CoreSandbox.InstalledInterfaces);
  InitializeListHead(&CoreSandbox.VMRegions);

  /* TODO: when will this memory be freed? */
  CoreSandbox.MallocManager = AllocatePool(sizeof(struct SandboxMallocManager));
  InitSandboxMallocManager(CoreSandbox.MallocManager);


  for(int i =SLAB_MIN_ORDER;  i <= SLAB_MAX_ORDER; i++) {
    AllocateSandboxMemory(&CoreSandbox, 1 << i);
  }

  CurrentSandbox = &CoreSandbox;

  mSandbox.UserAddressBase = USER_BASE;
  Status = gBS->InstallProtocolInterface(&mSandboxHandle,
                                         &gEfiSandboxArchProtocolGuid,
                                         EFI_NATIVE_INTERFACE, &mSandbox);
  ASSERT_EFI_ERROR(Status);


  return Status;
}
