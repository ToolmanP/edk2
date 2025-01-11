#include "Event.h"
#include "Interface/Duplicate.h"
#include "Interface/Registry.h"
#include "Library/BaseCounterLib/Counter.h"
#include "Library/BaseLib.h"
#include "Library/DebugLib.h"
#include "Library/MemoryAllocationLib.h"
#include "Library/SandboxSyscallLib.h"
#include "Library/UefiBootServicesTableLib.h"
#include "Library/UefiRuntimeServicesTableLib.h"
#include "Memory/Malloc.h"
#include "Memory/Memory.h"
#include "PointerList.h"
#include "Print.h"
#include "ProcessorBind.h"
#include "Sched.h"
#include "Uefi/UefiBaseType.h"
#include "Uefi/UefiSpec.h"
#include "UefiSandbox.h"

EFI_STATUS
SandboxSyscallNull(IN UINTN SyscallNumber) {
  SBError("CurrentSandbox: %d, Syscall %u not implemented\n",
          CurrentSandbox->SandboxID, SyscallNumber);
  ASSERT(FALSE);
  return EFI_UNSUPPORTED;
}

EFI_TPL
SandboxRaiseTpl(IN EFI_TPL NewTpl) { return gBS->RaiseTPL(NewTpl); }

VOID SandboxRestoreTpl(IN EFI_TPL OldTpl) { gBS->RestoreTPL(OldTpl); }

EFI_STATUS
SandboxAllocatePages(IN EFI_ALLOCATE_TYPE Type, IN EFI_MEMORY_TYPE MemoryType,
                     IN UINTN PageNum, IN OUT EFI_VIRTUAL_ADDRESS *Memory) {
  struct SandboxPages *Pages;

  Pages =
      AllocateSandboxPages(CurrentSandbox, Type, MemoryType, PageNum, Memory);
  if (Pages == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  *Memory = TO_VIRT_ADDR(*Memory);

  return EFI_SUCCESS;
}

EFI_STATUS
SandboxFreePages(IN EFI_VIRTUAL_ADDRESS Memory, IN UINTN Pages) {
  return FreeSandboxPages(CurrentSandbox, VIRT_TO_PHYS(Memory), Pages, NULL);
}

EFI_STATUS
SandboxGetMemoryMap(IN OUT UINTN *MemoryMapSize,
                    IN OUT EFI_MEMORY_DESCRIPTOR *MemoryMap, OUT UINTN *MapKey,
                    OUT UINTN *DescriptorSize, OUT UINT32 *DescriptorVersion) {
  return gBS->GetMemoryMap(
      PTR_VIRT_TO_PHYS(MemoryMapSize), PTR_VIRT_TO_PHYS(MemoryMap),
      PTR_VIRT_TO_PHYS(MapKey), PTR_VIRT_TO_PHYS(DescriptorSize),
      PTR_VIRT_TO_PHYS(DescriptorVersion));
}

EFI_STATUS SandboxAllocatePool(IN EFI_MEMORY_TYPE PoolType, IN UINTN Size,
                               OUT VOID **Buffer) {
  EFI_STATUS Status;
  EFI_PHYSICAL_ADDRESS Address;

  Status = AllocateSandboxPool(CurrentSandbox, PoolType, Size, &Address);

  *Buffer = (VOID *)TO_VIRT_ADDR(Address);

  return Status;
}

EFI_STATUS
SandboxFreePool(IN VOID *Buffer) {
  return FreeSandboxPool(CurrentSandbox, VIRT_TO_PHYS(Buffer));
}

EFI_STATUS
SandboxCreateEventEx(IN UINT32 Type, IN EFI_TPL NotifyTpl,
                     IN EFI_EVENT_NOTIFY NotifyFunction, IN VOID *NotifyContext,
                     IN EFI_GUID *EventGroup, OUT EFI_EVENT *Event) {
  EFI_STATUS Status;
  Status =
      gBS->CreateEventEx(Type, NotifyTpl, SandboxGenericNotifyFunction,
                         NotifyContext, EventGroup, PTR_VIRT_TO_PHYS(Event));
  return Status;
}

EFI_STATUS
SandboxCreateEvent(IN UINT32 Type, IN EFI_TPL NotifyTpl,
                   IN EFI_EVENT_NOTIFY NotifyFunction, IN VOID *NotifyContext,
                   OUT EFI_EVENT *Event) {

  return SandboxCreateEventEx(Type, NotifyTpl, NotifyFunction, NotifyContext,
                              NULL, PTR_VIRT_TO_PHYS(Event));
}

EFI_STATUS
SandboxSetTimer(IN EFI_EVENT Event, IN EFI_TIMER_DELAY Type,
                IN UINT64 TriggerTime) {
  return gBS->SetTimer(Event, Type, TriggerTime);
}

EFI_STATUS
SandboxWaitForEvent(IN UINTN NumberOfEvents, IN EFI_EVENT *Event,
                    OUT UINTN *Index) {

  return gBS->WaitForEvent(NumberOfEvents, PTR_VIRT_TO_PHYS(Event),
                           PTR_VIRT_TO_PHYS(Index));
}

EFI_STATUS
SandboxSignalEvent(IN EFI_EVENT Event) { return gBS->SignalEvent(Event); }

EFI_STATUS
SandboxCloseEvent(IN EFI_EVENT Event) { return gBS->CloseEvent(Event); }

EFI_STATUS
SandboxCheckEvent(IN EFI_EVENT Event) { return gBS->CheckEvent(Event); }

EFI_STATUS
SandboxInstallProtocolInterface(IN OUT EFI_HANDLE *Handle,
                                IN EFI_GUID *ProtocolID,
                                IN EFI_INTERFACE_TYPE InterfaceType,
                                IN VOID *Interface) {
  EFI_STATUS Status;
  LOCATED_INTERFACE *Located;

  UEFI_SANDBOX *CallerSandbox;

  CallerSandbox = ScheduleToSandboxInternal(&CoreSandbox, TRUE);

  Status =
      InstallSandboxInterface(CallerSandbox, ProtocolID, *Handle, Interface);

  if (EFI_ERROR(Status))
    return Status;

  Status = LocateSandboxInterface(&CoreSandbox, *Handle, ProtocolID, TRUE, &Located);

  SBPrint("In Core: %d, Protocol: %g, Handle: 0x%p "
          "Interface: 0x%p Magisk Interface: 0x%p\n",
          CoreSandbox.SandboxID, ProtocolID, *Handle, Interface,
          Located->Magisk.Interface);

  if (EFI_ERROR(Status))
    goto err_out;

  // PointerRecordSiteToPhys(&Located->Magisk.PointerList);
  Status = gBS->InstallProtocolInterface(
      Handle, ProtocolID, InterfaceType,
      (VOID *)TO_PHYS_ADDR((EFI_VIRTUAL_ADDRESS)(Located->Magisk.Interface)));

  if (EFI_ERROR(Status))
    goto err_out;

  ASSERT(ScheduleToSandboxInternal(CallerSandbox, TRUE) == &CoreSandbox);

  return Status;

err_out:
  ASSERT(ScheduleToSandboxInternal(CallerSandbox, TRUE) == &CoreSandbox);
  Status =
      UninstallSandboxInterface(CurrentSandbox, ProtocolID, Handle, Interface);
  return Status;
}

EFI_STATUS
SandboxReinstallProtocolInterface(IN EFI_HANDLE Handle, IN EFI_GUID *ProtocolID,
                                  IN VOID *OldInterface,
                                  IN VOID *NewInterface) {
  __unimplemented();
  return EFI_SUCCESS;
  // EFI_STATUS Status;
  // Status = ReinstallSandboxInterface(CurrentSandbox, ProtocolID, Handle,
  //                                    OldInterface, NewInterface);
  // if (EFI_ERROR(Status)) {
  //   return EFI_NOT_FOUND;
  // }
  //
  // Status = gBS->ReinstallProtocolInterface(Handle, ProtocolID, OldInterface,
  //                                          NewInterface);
  // if (EFI_ERROR(Status)) {
  //   ASSERT(0);
  // }
  //
  // return Status;
}

EFI_STATUS
SandboxUninstallProtocolInterface(IN EFI_HANDLE Handle, IN EFI_GUID *Protocol,
                                  IN VOID *Interface) {
  EFI_STATUS Status;
  Status = gBS->UninstallProtocolInterface(Handle, Protocol, Interface);
  if (EFI_ERROR(Status)) {
    return Status;
  }

  UninstallSandboxInterface(CurrentSandbox, Interface, Protocol, NULL);

  return Status;
}

EFI_STATUS
SandboxHandleProtocol(IN EFI_HANDLE Handle, IN EFI_GUID *Protocol,
                      OUT VOID **Interface) {
  EFI_STATUS Status;
  UEFI_SANDBOX *CallerSandbox;
  LOCATED_INTERFACE *LocatedInterface;
  CallerSandbox = ScheduleToSandboxInternal(&CoreSandbox, TRUE);

  SBPrint("HandleProtocol: %g Handle: 0x%p\n", Protocol, Handle);

  Status = LocateSandboxInterface(CallerSandbox, Handle, Protocol,
                                  FALSE, &LocatedInterface);

  if (EFI_ERROR(Status))
    goto out;

  *(VOID **)TO_PHYS_ADDR((EFI_VIRTUAL_ADDRESS)Interface) =
      LocatedInterface->Magisk.Interface;

out:
  ScheduleToSandboxInternal(CallerSandbox, TRUE);
  return EFI_SUCCESS;
}

EFI_STATUS
SandboxRegisterProtocolNotify(IN EFI_GUID *Protocol, IN EFI_EVENT Event,
                              OUT VOID **Registration) {
  __unimplemented();
  return EFI_SUCCESS;
}

EFI_STATUS
SandboxLocateHandle(IN EFI_LOCATE_SEARCH_TYPE SearchType,
                    IN EFI_GUID *Protocol OPTIONAL, IN VOID *SearchKey OPTIONAL,
                    IN OUT UINTN *BufferSize, OUT EFI_HANDLE *Buffer) {
  __unimplemented();
  return EFI_SUCCESS;
}

EFI_STATUS
SandboxLocateDevicePath(IN EFI_GUID *Protocol,
                        IN OUT EFI_DEVICE_PATH_PROTOCOL **DevicePath,
                        OUT EFI_HANDLE *Device) {
  __unimplemented();
  return EFI_SUCCESS;
}

EFI_STATUS
SandboxInstallConfigurationTable(IN EFI_GUID *Guid, IN VOID *Table) {
  __unimplemented();
  return EFI_SUCCESS;
}

EFI_STATUS
SandboxLoadImage(IN BOOLEAN BootPolicy, IN EFI_HANDLE ParentImageHandle,
                 IN EFI_DEVICE_PATH_PROTOCOL *DevicePath, IN VOID *SourceBuffer,
                 IN UINTN SourceSize, OUT EFI_HANDLE *ImageHandle) {
  __unimplemented();
  return EFI_SUCCESS;
}

EFI_STATUS
SandboxStartImage(IN EFI_HANDLE ImageHandle, OUT UINTN *ExitDataSize,
                  OUT CHAR16 **ExitData) {
  __unimplemented();
  return EFI_SUCCESS;
}

EFI_STATUS
SandboxExit(IN EFI_HANDLE ImageHandle, IN EFI_STATUS ExitStatus,
            IN UINTN ExitDataSize, IN CHAR16 *ExitData) {
  __unimplemented();
  return EFI_SUCCESS;
}

EFI_STATUS
SandboxUnloadImage(IN EFI_HANDLE ImageHandle) {
  __unimplemented();
  return EFI_SUCCESS;
}

EFI_STATUS
SandboxExitBootServices(IN EFI_HANDLE ImageHandle, IN UINTN MapKey) {
  __unimplemented();
  return EFI_SUCCESS;
}

EFI_STATUS
SandboxStall(IN UINTN Microseconds) { return gBS->Stall(Microseconds); }

EFI_STATUS
SandboxSetWatchdogTimer(IN UINTN Timeout, IN UINT64 WatchdogCode,
                        IN UINTN DataSize, IN CHAR16 *WatchdogData) {
  return EFI_UNSUPPORTED;
}

EFI_STATUS
SandboxConnectController(
    IN EFI_HANDLE ControllerHandle, IN EFI_HANDLE *DriverImageHandle OPTIONAL,
    IN EFI_DEVICE_PATH_PROTOCOL *RemainingDevicePath OPTIONAL,
    IN BOOLEAN Recursive) {
  __unimplemented();
  return EFI_SUCCESS;
}

EFI_STATUS
SandboxDisconnectController(IN EFI_HANDLE ControllerHandle,
                            IN EFI_HANDLE DriverImageHandle,
                            IN EFI_HANDLE ChildHandle OPTIONAL) {
  __unimplemented();
  return EFI_SUCCESS;
}

EFI_STATUS
SandboxOpenProtocol(IN EFI_HANDLE Handle, IN EFI_GUID *Protocol,
                    OUT VOID **Interface, IN EFI_HANDLE AgentHandle,
                    IN EFI_HANDLE ControllerHandle, IN UINT32 Attributes) {
  LOCATED_INTERFACE *LocatedInterface;
  UEFI_SANDBOX *CallerSandbox;
  EFI_STATUS Status;

  CallerSandbox = ScheduleToSandboxInternal(&CoreSandbox, TRUE);

  Status =
      OpenSandboxInterface(CallerSandbox, Handle, Protocol, AgentHandle,
                           ControllerHandle, Attributes, &LocatedInterface);

  if (EFI_ERROR(Status))
    goto out;

  if (Attributes == EFI_OPEN_PROTOCOL_TEST_PROTOCOL) {
    Status = EFI_SUCCESS;
    goto out;
  }

  SBPrint("Sandbox: %d, Protocol: %g, Handle: 0x%p "
          "Ptr: 0x%p Interface: 0x%p Magisk Interface: 0x%p\n",
          CallerSandbox->SandboxID, Protocol, Handle, Interface,
          LocatedInterface->Sandboxed->Opaque,
          LocatedInterface->Magisk.Interface);

  *(VOID **)TO_PHYS_ADDR((EFI_VIRTUAL_ADDRESS)Interface) =
      LocatedInterface->Magisk.Interface;

out:
  ScheduleToSandboxInternal(CallerSandbox, TRUE);
  return Status;
}

EFI_STATUS
SandboxCloseProtocol(IN EFI_HANDLE Handle, IN EFI_GUID *Protocol,
                     IN EFI_HANDLE AgentHandle,
                     IN EFI_HANDLE ControllerHandle) {

  UEFI_SANDBOX *CallerSandbox;
  EFI_STATUS Status;

  CallerSandbox = ScheduleToSandboxInternal(&CoreSandbox, TRUE);
  Status = CloseSandboxInterface(CallerSandbox, Handle, Protocol, AgentHandle,
                                 ControllerHandle);

  ScheduleToSandboxInternal(CallerSandbox, TRUE);

  return Status;
}

EFI_STATUS
SandboxOpenProtocolInformation(
    IN EFI_HANDLE Handle, IN EFI_GUID *Protocol,
    OUT EFI_OPEN_PROTOCOL_INFORMATION_ENTRY **EntryBuffer,
    OUT UINTN *EntryCount) {
  __unimplemented();
  return EFI_SUCCESS;
}

EFI_STATUS
SandboxProtocolsPerHandle(IN EFI_HANDLE Handle, OUT EFI_GUID ***ProtocolBuffer,
                          OUT UINTN *ProtocolBufferCount) {
  __unimplemented();
  return EFI_SUCCESS;
}

EFI_STATUS
SandboxLocateHandleBuffer(IN EFI_LOCATE_SEARCH_TYPE SearchType,
                          IN EFI_GUID *Protocol OPTIONAL,
                          IN VOID *SearchKey OPTIONAL, IN OUT UINTN *NumHandles,
                          OUT EFI_HANDLE **Buffer) {

  UEFI_SANDBOX *CallerSandbox;
  VOID *BufferPtr = NULL;
  EFI_STATUS Status;
  UINTN BufferSize = 0;
  CallerSandbox = ScheduleToSandboxInternal(&CoreSandbox, TRUE);

  Status = gBS->LocateHandle(SearchType, Protocol, SearchKey, &BufferSize,
                             BufferPtr);
  SBPrint("Status = %r\n", Status);

  ASSERT(Status == EFI_BUFFER_TOO_SMALL);

  BufferPtr = AllocateSandboxMemory(CallerSandbox, BufferSize);

  Status = gBS->LocateHandle(SearchType, Protocol, SearchKey, &BufferSize,
                             BufferPtr);
  if (EFI_ERROR(Status))
    goto out;

  *(UINTN *)TO_PHYS_ADDR((EFI_VIRTUAL_ADDRESS)NumHandles) =
      BufferSize / sizeof(EFI_HANDLE);
  *(EFI_HANDLE **)TO_PHYS_ADDR((EFI_VIRTUAL_ADDRESS)Buffer) =
      (EFI_HANDLE *)TO_VIRT_ADDR((EFI_PHYSICAL_ADDRESS)BufferPtr);
out:

  ScheduleToSandboxInternal(CallerSandbox, TRUE);
  return EFI_SUCCESS;
}

EFI_STATUS
SandboxLocateProtocol(IN EFI_GUID *ProtocolID, IN VOID *Registration OPTIONAL,
                      OUT VOID **InterfaceUserPtr) {
  EFI_STATUS Status;
  LocatedInterface *LocatedInterface;
  UEFI_SANDBOX *CallerSandbox;

  CallerSandbox = ScheduleToSandboxInternal(&CoreSandbox, TRUE);

  Status = LocateSandboxInterface(CallerSandbox, NULL, ProtocolID,
                                  FALSE, &LocatedInterface);

  if (EFI_ERROR(Status))
    goto out;

  *(VOID **)TO_PHYS_ADDR((EFI_VIRTUAL_ADDRESS)InterfaceUserPtr) =
      LocatedInterface->Magisk.Interface;

out:
  ScheduleToSandboxInternal(CallerSandbox, TRUE);

  return Status;
}

EFI_STATUS
SandboxInstallMultipleProtocolInterfaces(IN OUT EFI_HANDLE *Handle, ...) {
  __unimplemented();
  ASSERT(FALSE);
  return EFI_UNSUPPORTED;
}

EFI_STATUS
SandboxUninstallMultipleProtocolInterfaces(IN EFI_HANDLE Handle, ...) {
  __unimplemented();
  ASSERT(FALSE);
  return EFI_UNSUPPORTED;
}

VOID SandboxCopyMem(IN VOID *Destination, IN VOID *Source, IN UINTN Length) {
  __unimplemented();
  gBS->CopyMem(
      IS_VIRT_ADDR(Destination) ? PTR_VIRT_TO_PHYS(Destination) : Destination,
      IS_VIRT_ADDR(Source) ? PTR_VIRT_TO_PHYS(Source) : Source, Length);
}

VOID SandboxSetMem(IN VOID *Buffer, IN UINTN Size, IN UINT8 Value) {
  __unimplemented();
}

/*
 *
 * Runtime Services
 *
 */

EFI_STATUS
SandboxGetTime(OUT EFI_TIME *Time, OUT EFI_TIME_CAPABILITIES *Capabilities) {
  return gRT->GetTime(Time, Capabilities);
}

EFI_STATUS
SandboxSetTime(IN EFI_TIME *Time) {
  return gRT->SetTime(Time);
}

EFI_STATUS
SandboxGetWakeupTime(OUT BOOLEAN *Enabled, OUT BOOLEAN *Pending,
                     OUT EFI_TIME *Time) {
  __unimplemented();
  return EFI_SUCCESS;
}

EFI_STATUS
SandboxSetWakeupTime(IN BOOLEAN Enable, IN EFI_TIME *Time) {
  __unimplemented();
  return EFI_SUCCESS;
}

EFI_STATUS
SandboxSetVirtualAddressMap(IN UINTN MemoryMapSize, IN UINTN DescriptorSize,
                            IN UINT32 DescriptorVersion,
                            IN EFI_MEMORY_DESCRIPTOR *VirtualMap) {
  __unimplemented();
  return EFI_SUCCESS;
}

EFI_STATUS
SandboxConvertPointer(IN UINTN DebugDisposition, IN OUT VOID **Address) {
  __unimplemented();
  return EFI_SUCCESS;
}

EFI_STATUS
SandboxGetVariable(IN CHAR16 *VariableName, IN EFI_GUID *VendorGuid,
                   OUT UINT32 *Attributes OPTIONAL, IN OUT UINTN *DataSize,
                   OUT VOID *Data) {

  UEFI_SANDBOX *CallerSandbox;
  EFI_STATUS Status;
  CallerSandbox = ScheduleToSandboxInternal(&CoreSandbox, TRUE);
  Status = gST->RuntimeServices->GetVariable(VariableName, VendorGuid,
                                             Attributes, DataSize, Data);
  ScheduleToSandboxInternal(CallerSandbox, TRUE);
  return Status;
}

EFI_STATUS
SandboxGetNextVariableName(IN OUT UINTN *VariableNameSize,
                           IN OUT CHAR16 *VariableName,
                           IN OUT EFI_GUID *VendorGuid) {
  __unimplemented();
  return EFI_SUCCESS;
}

EFI_STATUS
SandboxSetVariable(IN CHAR16 *VariableName, IN EFI_GUID *VendorGuid,
                   IN UINT32 Attributes, IN UINTN DataSize, IN VOID *Data) {

  __unimplemented();
  return EFI_SUCCESS;
}

EFI_STATUS
SandboxGetNextHighMonotonicCount(OUT UINT32 *HighCount) {
  __unimplemented();
  return EFI_SUCCESS;
}

VOID SandboxResetSystem(IN EFI_RESET_TYPE ResetType, IN EFI_STATUS ResetStatus,
                        IN UINTN DataSize, IN VOID *ResetData OPTIONAL) {

  __unimplemented();
}

EFI_STATUS
SandboxUpdateCapsule(IN EFI_CAPSULE_HEADER **CapsuleHeaderArray,
                     IN UINTN CapsuleCount,
                     IN EFI_PHYSICAL_ADDRESS ScatterGatherList OPTIONAL) {
  __unimplemented();
  return EFI_SUCCESS;
}

EFI_STATUS
SandboxQueryCapsuleCapabilities(IN EFI_CAPSULE_HEADER **CapsuleHeaderArray,
                                IN UINTN CapsuleCount,
                                OUT UINT64 *MaximumCapsuleSize,
                                OUT EFI_RESET_TYPE *ResetType) {
  __unimplemented();
  return EFI_SUCCESS;
}

EFI_STATUS
SandboxQueryVariableInfo(IN UINT32 Attributes,
                         OUT UINT64 *MaximumVariableStorageSize,
                         OUT UINT64 *RemainingVariableStorageSize,
                         OUT UINT64 *MaximumVariableSize) {
  __unimplemented();
}

EFI_STATUS
SandboxInterfaceCall(IN LocatedInterface *Located, IN UINT64 Offset,
                     IN UINT64 *CallSiteParams) {

  UEFI_SANDBOX *CallerSandbox, *CalleeSandbox;
  REFLECT_FUNC_TYPE *Func;
  POINTER_LIST PointerList;
  EFI_STATUS Status = EFI_SUCCESS;
  DUPLICATE_CTX Ctx;
  UINT64 Params[8];

  CallerSandbox = ScheduleToSandboxInternal(&CoreSandbox, TRUE);

#if SANDBOX_PERF_INTERFACE_CALL
  DebugPrint(DEBUG_INFO, "Enter InterfaceCall Counter: %ld\n", ReadCounter());
#endif

  CalleeSandbox = (Located->Sandboxed->SandboxID == 0)
                      ? &CoreSandbox
                      : FindSandbox(Located->Sandboxed->SandboxID);

  InitPointerRecordList(&PointerList, CalleeSandbox);
  Ctx = (DUPLICATE_CTX){.PointerList = &PointerList,
                        .CurrentType = NULL,
                        .InUnion = FALSE,
                        .Syncable = TRUE};

#if SANDBOX_PERF_INTERFACE_CALL
  UINTN GetFunctionStart, GetFunctionEnd;
  GetFunctionStart = ReadCounter();
  DebugPrint(DEBUG_INFO, "DBQuery Start: %ld\n", GetFunctionStart);
#endif

  Status = GetFunctionByOffset(Located->Desc, Offset, &Func);

#if SANDBOX_PERF_INTERFACE_CALL
  GetFunctionEnd = ReadCounter();
  DebugPrint(DEBUG_INFO, "DBQuery End: %ld\n", GetFunctionEnd);
#endif

  ASSERT_EFI_ERROR(Status);

#if SANDBOX_PERF_INTERFACE_CALL
  UINTN CopyStart, CopyEnd;
  CopyStart = ReadCounter();
  DebugPrint(DEBUG_INFO, "Copy Input Start: %ld\n", CopyStart);
#endif

  CopyInterfaceCallParams(&Ctx, Located->Sandboxed->Opaque, Func,
                                   CallSiteParams, Params);

#if SANDBOX_PERF_INTERFACE_CALL
  CopyEnd = ReadCounter();
  DebugPrint(DEBUG_INFO, "Copy Input End: %ld\n", CopyEnd);
#endif

  if (EFI_ERROR(Status)) {
    FreePointerRecordList(&PointerList, POINTER_SYNC_TYPE_NONE);
    ScheduleToSandboxInternal(CallerSandbox, TRUE);
    return Status;
  }

#if SANDBOX_PERF_INTERFACE_CALL
  UINTN ContextSwitchBegin, ContextSwitchEnd;
  ContextSwitchBegin = ReadCounter();
  DebugPrint(DEBUG_INFO, "CallSandbox Start: %ld\n", ContextSwitchBegin);
#endif

  if (CalleeSandbox == &CoreSandbox) {
    PointerRecordSiteToPhys(&PointerList); // We should change the virtual
                                           // address to physical address
    Status = JumpToCoreFunc(Located, Params, Offset);
  } else {
    ASSERT(Offset < Located->Sandboxed->Desc->ProtocolSize);
    Status = JumpToSandboxFunc(CalleeSandbox, Located, Params, Offset);
  }

#if SANDBOX_PERF_INTERFACE_CALL
  ContextSwitchEnd = ReadCounter();
  DebugPrint(DEBUG_INFO, "CallSandbox End: %ld\n",
             ContextSwitchEnd);
#endif

#if SANDBOX_PERF_INTERFACE_CALL
  UINTN CopyBackStart, CopyBackEnd;
  CopyBackStart = ReadCounter();
  DebugPrint(DEBUG_INFO, "Copy Output Start: %ld\n", CopyBackStart);
#endif

  // SyncInterfaceMagisk(&Located->Magisk);
  if (!EFI_ERROR(Status))
    SyncInterfaceCallParams(CallerSandbox, CalleeSandbox, Func, Params,
                            CallSiteParams);
  FreePointerRecordList(&PointerList, POINTER_SYNC_DST_TO_SRC);

#if SANDBOX_PERF_INTERFACE_CALL
  CopyBackEnd = ReadCounter();
  DebugPrint(DEBUG_INFO, "Copy Output End: %lu\n", CopyBackEnd);
#endif

#if SANDBOX_PERF_INTERFACE_CALL
  DebugPrint(DEBUG_INFO, "Exit InterfaceCall Counter: %lu\n", ReadCounter());
#endif
  ScheduleToSandboxInternal(CallerSandbox, TRUE);

  return Status;
}

EFI_STATUS
SandboxReturnFromSandbox(IN BASE_LIBRARY_JUMP_BUFFER *JumpBuffer,
                         EFI_STATUS Status) {

  if (Status == (1UL << sizeof(EFI_STATUS)))
    Status = 0;
  LongJump(JumpBuffer, Status + 1);
  ASSERT(0);
  return EFI_SUCCESS;
}

EFI_STATUS
SandboxCalculateCrc32(IN VOID *Data, IN UINTN DataSize, OUT UINT32 *Crc32) {
  UEFI_SANDBOX *CallerSandbox;
  EFI_STATUS Status;

  CallerSandbox = ScheduleToSandboxInternal(&CoreSandbox, TRUE);

  Status = gBS->CalculateCrc32(Data, DataSize, Crc32);

  ScheduleToSandboxInternal(CallerSandbox, TRUE);
  return Status;
}

const VOID *SandboxServicesManager[NR_SYSCALL] = {
    [SANDBOX_SYS_BS_RAISE_TPL] = SandboxRaiseTpl,           // RaiseTPL
    [SANDBOX_SYS_BS_RESTORE_TPL] = SandboxRestoreTpl,       // RestoreTPL
    [SANDBOX_SYS_BS_ALLOCATE_PAGES] = SandboxAllocatePages, // AllocatePages
    [SANDBOX_SYS_BS_FREE_PAGES] = SandboxFreePages,         // FreePages
    [SANDBOX_SYS_BS_GET_MEMORY_MAP] = SandboxGetMemoryMap,  // GetMemoryMap
    [SANDBOX_SYS_BS_ALLOCATE_POOL] = SandboxAllocatePool,   // AllocatePool
    [SANDBOX_SYS_BS_FREE_POOL] = SandboxFreePool,           // FreePool
    [SANDBOX_SYS_BS_CREATE_EVENT] = SandboxCreateEvent,     // CreateEvent
    [SANDBOX_SYS_BS_SET_TIMER] = SandboxSetTimer,           // SetTimer
    [SANDBOX_SYS_BS_WAIT_FOR_EVENT] = SandboxWaitForEvent,  // WaitForEvent
    [SANDBOX_SYS_BS_SIGNAL_EVENT] = SandboxSignalEvent,     // SignalEvent
    [SANDBOX_SYS_BS_CLOSE_EVENT] = SandboxCloseEvent,       // CloseEvent
    [SANDBOX_SYS_BS_CHECK_EVENT] = SandboxCheckEvent,       // CheckEvent
    [SANDBOX_SYS_BS_INSTALL_PROTOCOL_INTERFACE] =
        SandboxInstallProtocolInterface, // InstallProtocolInterface
    [SANDBOX_SYS_BS_REINSTALL_PROTOCOL_INTERFACE] =
        SandboxReinstallProtocolInterface, // ReinstallProtocolInterface
    [SANDBOX_SYS_BS_UNINSTALL_PROTOCOL_INTERFACE] =
        SandboxUninstallProtocolInterface, // UninstallProtocolInterface
    [SANDBOX_SYS_BS_HANDLE_PROTOCOL] = SandboxHandleProtocol, // HandleProtocol
    [SANDBOX_SYS_BS_RESERVED] = NULL,                         // Reserved
    [SANDBOX_SYS_BS_REGISTER_PROTOCOL_NOTIFY] =
        SandboxRegisterProtocolNotify, // RegisterProtocolNotify
    [SANDBOX_SYS_BS_LOCATE_HANDLE] = SandboxLocateHandle, // LocateHandle
    [SANDBOX_SYS_BS_LOCATE_DEVICE_PATH] =
        SandboxLocateDevicePath, // LocateDevicePath
    [SANDBOX_SYS_BS_INSTALL_CONFIGURATION_TABLE] =
        SandboxInstallConfigurationTable,           // InstallConfigurationTable
    [SANDBOX_SYS_BS_IMAGE_LOAD] = SandboxLoadImage, // LoadImage
    [SANDBOX_SYS_BS_IMAGE_START] = SandboxStartImage,   // StartImage
    [SANDBOX_SYS_BS_EXIT] = SandboxExit,                // Exit
    [SANDBOX_SYS_BS_IMAGE_UNLOAD] = SandboxUnloadImage, // UnloadImage
    [SANDBOX_SYS_BS_EXIT_BOOT_SERVICES] =
        SandboxExitBootServices, // ExitBootServices
    [SANDBOX_SYS_BS_GET_NEXT_MONOTONIC_COUNT] =
        SandboxGetNextHighMonotonicCount,  // GetNextHighMonotonicCount
    [SANDBOX_SYS_BS_STALL] = SandboxStall, // Stall
    [SANDBOX_SYS_BS_SET_WATCHDOG_TIMER] =
        SandboxSetWatchdogTimer, // SetWatchdogTimer
    [SANDBOX_SYS_BS_CONNECT_CONTROLLER] =
        SandboxConnectController, // ConnectController
    [SANDBOX_SYS_BS_DISCONNECT_CONTROLLER] =
        SandboxDisconnectController, // DisconnectController
    [SANDBOX_SYS_BS_OPEN_PROTOCOL] = SandboxOpenProtocol,   // OpenProtocol
    [SANDBOX_SYS_BS_CLOSE_PROTOCOL] = SandboxCloseProtocol, // CloseProtocol
    [SANDBOX_SYS_BS_OPEN_PROTOCOL_INFORMATION] =
        SandboxOpenProtocolInformation, // OpenProtocolInformation
    [SANDBOX_SYS_BS_PROTOCOLS_PER_HANDLE] =
        SandboxProtocolsPerHandle, // ProtocolsPerHandle
    [SANDBOX_SYS_BS_LOCATE_HANDLE_BUFFER] =
        SandboxLocateHandleBuffer, // LocateHandleBuffer
    [SANDBOX_SYS_BS_LOCATE_PROTOCOL] = SandboxLocateProtocol, // LocateProtocol
    [SANDBOX_SYS_BS_INSTALL_MULTIPLE_PROTOCOL_INTERFACES] =
        SandboxInstallMultipleProtocolInterfaces, // InstallMultipleProtocolInterfaces
    [SANDBOX_SYS_BS_UNINSTALL_MULTIPLE_PROTOCOL_INTERFACES] =
        SandboxUninstallMultipleProtocolInterfaces, // UninstallMultipleProtocolInterfaces
    [SANDBOX_SYS_BS_CALCULATE_CRC32] = SandboxCalculateCrc32, // CalculateCrc32
    [SANDBOX_SYS_BS_COPY_MEM] = SandboxCopyMem,               // CopyMem
    [SANDBOX_SYS_BS_SET_MEM] = SandboxSetMem,                 // SetMem
    [SANDBOX_SYS_BS_CREATE_EVENT_EX] = SandboxCreateEventEx,  // CreateEventEx

    [SANDBOX_SYS_RT_GET_TIME] = SandboxGetTime,              // GetTime
    [SANDBOX_SYS_RT_SET_TIME] = SandboxSetTime,              // SetTime
    [SANDBOX_SYS_RT_GET_WAKEUP_TIME] = SandboxGetWakeupTime, // GetWakeupTime
    [SANDBOX_SYS_RT_SET_WAKEUP_TIME] = SandboxSetWakeupTime, // SetWakeupTime
    [SANDBOX_SYS_RT_SET_VIRTUAL_ADDRESS_MAP] =
        SandboxSetVirtualAddressMap, // SetVirtualAddressMap
    [SANDBOX_SYS_RT_CONVERT_POINTER] = SandboxConvertPointer, // ConvertPointer
    [SANDBOX_SYS_RT_GET_VARIABLE] = SandboxGetVariable,       // GetVariable
    [SANDBOX_SYS_RT_GET_NEXT_VARIABLE_NAME] =
        SandboxGetNextVariableName,                     // GetNextVariableName
    [SANDBOX_SYS_RT_SET_VARIABLE] = SandboxSetVariable, // SetVariable
    [SANDBOX_SYS_RT_GET_NEXT_HIGH_MONO_COUNT] =
        SandboxGetNextHighMonotonicCount, // GetNextHighMonotonicCount
    [SANDBOX_SYS_RT_RESET_SYSTEM] = SandboxResetSystem,     // ResetSystem
    [SANDBOX_SYS_RT_UPDATE_CAPSULE] = SandboxUpdateCapsule, // UpdateCapsule
    [SANDBOX_SYS_RT_QUERY_CAPSULE_CAPABILITIES] =
        SandboxQueryCapsuleCapabilities, // QueryCapsuleCapabilities
    [SANDBOX_SYS_RT_QUERY_VARIABLE_INFO] =
        SandboxQueryVariableInfo, // QueryVariableInfo
    [SANDBOX_SYS_RT_INTERFACE_CALL] = SandboxInterfaceCall,
    [SANDBOX_SYS_RT_SANDBOX_RETURN] = SandboxReturnFromSandbox,
    [SANDBOX_SYS_NULL... NR_SYSCALL - 1] = SandboxSyscallNull,
};
