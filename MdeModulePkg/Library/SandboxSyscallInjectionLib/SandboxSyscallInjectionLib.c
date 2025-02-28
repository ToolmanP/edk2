#include "Library/SandboxSyscalls.h"
#include "Library/DebugLib.h"
#include "SandboxSyscallWrapper.h"
#include "Uefi/UefiBaseType.h"
#include "Uefi/UefiSpec.h"

EFI_TPL
EFIAPI
mRaiseTPL(IN EFI_TPL NewTpl) {
  return SandboxSyscall1(SANDBOX_SYS_BS_RAISE_TPL, NewTpl);
}

VOID EFIAPI mRestoreTPL(IN EFI_TPL OldTpl) {
  SandboxSyscall1(SANDBOX_SYS_BS_RESTORE_TPL, OldTpl);
}

EFI_STATUS
EFIAPI
mAllocatePages(IN EFI_ALLOCATE_TYPE Type, IN EFI_MEMORY_TYPE MemoryType,
               IN UINTN Pages, IN OUT EFI_VIRTUAL_ADDRESS *Memory) {
  return SandboxSyscall4(SANDBOX_SYS_BS_ALLOCATE_PAGES, Type, MemoryType, Pages,
                         (UINTN)Memory);
}

EFI_STATUS
EFIAPI
mFreePages(IN EFI_VIRTUAL_ADDRESS Memory, IN UINTN Pages) {
  return SandboxSyscall2(SANDBOX_SYS_BS_FREE_PAGES, Memory, Pages);
}

EFI_STATUS
EFIAPI
mGetMemoryMap(IN OUT UINTN *MemoryMapSize,
              IN OUT EFI_MEMORY_DESCRIPTOR *MemoryMap, OUT UINTN *MapKey,
              OUT UINTN *DescriptorSize, OUT UINT32 *DescriptorVersion) {
  return SandboxSyscall1(SANDBOX_SYS_NULL, SANDBOX_SYS_BS_GET_MEMORY_MAP);
}

EFI_STATUS
EFIAPI
mAllocatePool(IN EFI_MEMORY_TYPE PoolType, IN UINTN Size, OUT VOID **Buffer) {
  return SandboxSyscall3(SANDBOX_SYS_BS_ALLOCATE_POOL, PoolType, Size,
                         (UINTN)Buffer);
}

EFI_STATUS
EFIAPI
mFreePool(IN VOID *Buffer) {
  return SandboxSyscall1(SANDBOX_SYS_BS_FREE_POOL, (UINTN)Buffer);
}

EFI_STATUS
EFIAPI
mCreateEvent(IN UINT32 Type, IN EFI_TPL NotifyTpl,
             IN EFI_EVENT_NOTIFY NotifyFunction, IN VOID *NotifyContext,
             OUT EFI_EVENT *Event) {
  return SandboxSyscall5(SANDBOX_SYS_BS_CREATE_EVENT, Type, NotifyTpl,
                         (UINTN)NotifyFunction, (UINTN)NotifyContext,
                         (UINTN)Event);
}

EFI_STATUS
EFIAPI
mSetTimer(IN EFI_EVENT Event, IN EFI_TIMER_DELAY Type, IN UINTN TriggerTime) {
  return SandboxSyscall3(SANDBOX_SYS_BS_SET_TIMER, (UINTN)Event, Type, TriggerTime);
}

EFI_STATUS
EFIAPI
mWaitForEvent(IN UINTN NumberOfEvents, IN EFI_EVENT *Event, OUT UINTN *Index) {
  return SandboxSyscall3(SANDBOX_SYS_BS_WAIT_FOR_EVENT, NumberOfEvents, (UINTN)Event,
                         (UINTN)Index);
}

EFI_STATUS
EFIAPI
mSignalEvent(IN EFI_EVENT Event) {
  return SandboxSyscall1(SANDBOX_SYS_BS_SIGNAL_EVENT, (UINTN)Event);
}

EFI_STATUS
EFIAPI
mCloseEvent(IN EFI_EVENT Event) {
  return SandboxSyscall1(SANDBOX_SYS_BS_CLOSE_EVENT, (UINTN)Event);
}

EFI_STATUS
EFIAPI
mCheckEvent(IN EFI_EVENT Event) {
  return SandboxSyscall1(SANDBOX_SYS_BS_CHECK_EVENT, (UINTN)Event);
}

EFI_STATUS
EFIAPI
mInstallProtocolInterface(IN OUT EFI_HANDLE *Handle, IN EFI_GUID *Protocol,
                          IN EFI_INTERFACE_TYPE InterfaceType,
                          IN VOID *Interface) {
  return SandboxSyscall4(SANDBOX_SYS_BS_INSTALL_PROTOCOL_INTERFACE,
                         (UINTN)Handle, (UINTN)Protocol, InterfaceType,
                         (UINTN)Interface);
}

EFI_STATUS
EFIAPI
mReinstallProtocolInterface(IN EFI_HANDLE Handle, IN EFI_GUID *Protocol,
                            IN VOID *OldInterface, IN VOID *NewInterface) {
  return SandboxSyscall4(SANDBOX_SYS_BS_REINSTALL_PROTOCOL_INTERFACE,
                         (UINTN)Handle, (UINTN)Protocol, (UINTN)OldInterface,
                         (UINTN)NewInterface);
}

EFI_STATUS
EFIAPI
mUninstallProtocolInterface(IN EFI_HANDLE Handle, IN EFI_GUID *Protocol,
                            IN VOID *Interface) {
  return SandboxSyscall3(SANDBOX_SYS_BS_UNINSTALL_PROTOCOL_INTERFACE,
                         (UINTN)Handle, (UINTN)Protocol, (UINTN)Interface);
}

EFI_STATUS
EFIAPI
mHandleProtocol(IN EFI_HANDLE Handle, IN EFI_GUID *Protocol,
                OUT VOID **Interface) {
  return SandboxSyscall3(SANDBOX_SYS_BS_HANDLE_PROTOCOL, (UINTN)Handle,
                         (UINTN)Protocol, (UINTN)Interface);
}

EFI_STATUS
EFIAPI
mRegisterProtocolNotify(IN EFI_GUID *Protocol, IN EFI_EVENT Event,
                        OUT VOID **Registration) {
  return SandboxSyscall1(SANDBOX_SYS_NULL,
                         SANDBOX_SYS_BS_REGISTER_PROTOCOL_NOTIFY);
}

EFI_STATUS
EFIAPI
mLocateHandle(IN EFI_LOCATE_SEARCH_TYPE SearchType,
              IN EFI_GUID *Protocol OPTIONAL, IN VOID *SearchKey OPTIONAL,
              IN OUT UINTN *BufferSize, OUT EFI_HANDLE *Buffer) {
  return SandboxSyscall1(SANDBOX_SYS_NULL, SANDBOX_SYS_BS_LOCATE_HANDLE);
}

EFI_STATUS
EFIAPI
mLocateDevicePath(IN EFI_GUID *Protocol,
                  IN OUT EFI_DEVICE_PATH_PROTOCOL **DevicePath,
                  OUT EFI_HANDLE *Device) {
  return SandboxSyscall1(SANDBOX_SYS_NULL, SANDBOX_SYS_BS_LOCATE_DEVICE_PATH);
}

EFI_STATUS
EFIAPI
mInstallConfigurationTable(IN EFI_GUID *Guid, IN VOID *Table) {
  return SandboxSyscall1(SANDBOX_SYS_NULL,
                         SANDBOX_SYS_BS_INSTALL_CONFIGURATION_TABLE);
}

EFI_STATUS
EFIAPI
mLoadImage(IN BOOLEAN BootPolicy, IN EFI_HANDLE ParentImageHandle,
           IN EFI_DEVICE_PATH_PROTOCOL *DevicePath, IN VOID *SourceBuffer,
           IN UINTN SourceSize, OUT EFI_HANDLE *ImageHandle) {
  return SandboxSyscall1(SANDBOX_SYS_NULL, SANDBOX_SYS_BS_IMAGE_LOAD);
}

EFI_STATUS
EFIAPI
mStartImage(IN EFI_HANDLE ImageHandle, OUT UINTN *ExitDataSize,
            OUT CHAR16 **ExitData) {
  return SandboxSyscall1(SANDBOX_SYS_NULL, SANDBOX_SYS_BS_IMAGE_START);
}

EFI_STATUS
EFIAPI
mExit(IN EFI_HANDLE ImageHandle, IN EFI_STATUS ExitStatus,
      IN UINTN ExitDataSize, IN CHAR16 *ExitData) {
  return SandboxSyscall1(SANDBOX_SYS_NULL, SANDBOX_SYS_BS_EXIT);
}

EFI_STATUS
EFIAPI
mUnloadImage(IN EFI_HANDLE ImageHandle) {
  return SandboxSyscall1(SANDBOX_SYS_NULL, SANDBOX_SYS_BS_IMAGE_UNLOAD);
}

EFI_STATUS
EFIAPI
mExitBootServices(IN EFI_HANDLE ImageHandle, IN UINTN MapKey) {
  return SandboxSyscall1(SANDBOX_SYS_NULL, SANDBOX_SYS_BS_EXIT_BOOT_SERVICES);
}

EFI_STATUS
EFIAPI
mStall(IN UINTN Microseconds) {
  return SandboxSyscall1(SANDBOX_SYS_BS_STALL, Microseconds);
}

EFI_STATUS
EFIAPI
mSetWatchdogTimer(IN UINTN Timeout, IN UINTN WatchdogCode, IN UINTN DataSize,
                  IN CHAR16 *WatchdogData) {
  return SandboxSyscall1(SANDBOX_SYS_NULL, SANDBOX_SYS_BS_SET_WATCHDOG_TIMER);
}

EFI_STATUS
EFIAPI
mConnectController(IN EFI_HANDLE ControllerHandle,
                   IN EFI_HANDLE *DriverImageHandle OPTIONAL,
                   IN EFI_DEVICE_PATH_PROTOCOL *RemainingDevicePath OPTIONAL,
                   IN BOOLEAN Recursive) {
  return SandboxSyscall1(SANDBOX_SYS_NULL, SANDBOX_SYS_BS_CONNECT_CONTROLLER);
}

EFI_STATUS
EFIAPI
mDisconnectController(IN EFI_HANDLE ControllerHandle,
                      IN EFI_HANDLE DriverImageHandle,
                      IN EFI_HANDLE ChildHandle OPTIONAL) {
  return SandboxSyscall1(SANDBOX_SYS_NULL,
                         SANDBOX_SYS_BS_DISCONNECT_CONTROLLER);
}

EFI_STATUS
EFIAPI
mOpenProtocol(IN EFI_HANDLE Handle, IN EFI_GUID *Protocol, OUT VOID **Interface,
              IN EFI_HANDLE AgentHandle, IN EFI_HANDLE ControllerHandle,
              IN UINT32 Attributes) {

  return SandboxSyscall6(SANDBOX_SYS_BS_OPEN_PROTOCOL, (UINTN)Handle,
                         (UINTN)Protocol, (UINTN)Interface, (UINTN)AgentHandle,
                         (UINTN)ControllerHandle, (UINTN)Attributes);
}

EFI_STATUS
EFIAPI
mCloseProtocol(IN EFI_HANDLE Handle, IN EFI_GUID *Protocol,
               IN EFI_HANDLE AgentHandle, IN EFI_HANDLE ControllerHandle) {

  return SandboxSyscall4(SANDBOX_SYS_BS_CLOSE_PROTOCOL, (UINTN)Handle,
                         (UINTN)Protocol, (UINTN)AgentHandle,
                         (UINTN)ControllerHandle);
}

EFI_STATUS
EFIAPI
mOpenProtocolInformation(IN EFI_HANDLE Handle, IN EFI_GUID *Protocol,
                         OUT EFI_OPEN_PROTOCOL_INFORMATION_ENTRY **EntryBuffer,
                         OUT UINTN *EntryCount) {
  return SandboxSyscall1(SANDBOX_SYS_NULL,
                         SANDBOX_SYS_BS_OPEN_PROTOCOL_INFORMATION);
}

EFI_STATUS
EFIAPI
mProtocolsPerHandle(IN EFI_HANDLE Handle, OUT EFI_GUID ***ProtocolBuffer,
                    OUT UINTN *ProtocolBufferCount) {
  return SandboxSyscall1(SANDBOX_SYS_NULL, SANDBOX_SYS_BS_PROTOCOLS_PER_HANDLE);
}

EFI_STATUS
EFIAPI
mLocateHandleBuffer(IN EFI_LOCATE_SEARCH_TYPE SearchType,
                    IN EFI_GUID *Protocol OPTIONAL, IN VOID *SearchKey OPTIONAL,
                    IN OUT UINTN *NoHandles, OUT EFI_HANDLE **Buffer) {
  return SandboxSyscall5(SANDBOX_SYS_BS_LOCATE_HANDLE_BUFFER, SearchType,
                         (UINTN)Protocol, (UINTN)SearchKey, (UINTN)NoHandles,
                         (UINTN)Buffer);
}

EFI_STATUS
EFIAPI
mLocateProtocol(IN EFI_GUID *Protocol, IN VOID *Registration OPTIONAL,
                OUT VOID **Interface) {
  return SandboxSyscall3(SANDBOX_SYS_BS_LOCATE_PROTOCOL, (UINTN)Protocol,
                         (UINTN)Registration, (UINTN)Interface);
}

EFI_STATUS
EFIAPI
mInstallMultipleProtocolInterfaces(IN OUT EFI_HANDLE *Handle, ...) {
  VA_LIST Args;
  EFI_STATUS Status;
  EFI_GUID *Protocol;
  VOID *Interface;
  EFI_TPL OldTpl;
  UINTN Index;
  EFI_HANDLE OldHandle;

  if (Handle == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Syncronize with notifcations.
  //
  OldTpl = mRaiseTPL(TPL_NOTIFY);
  OldHandle = *Handle;

  //
  // install the protocol interfaces
  //
  VA_START(Args, Handle);
  for (Index = 0, Status = EFI_SUCCESS; !EFI_ERROR(Status); Index++) {
    //
    // If protocol is NULL, then it's the end of the list
    //
    Protocol = VA_ARG(Args, EFI_GUID *);
    if (Protocol == NULL) {
      break;
    }

    Interface = VA_ARG(Args, VOID *);

    DebugPrint(DEBUG_INFO,
               "InstallMultipleProtocolInterfaces: Protocol %g, Interface %p\n",
               Protocol, Interface);

    //
    // Install it
    //
    mInstallProtocolInterface(Handle, Protocol, EFI_NATIVE_INTERFACE,
                              Interface);
  }

  VA_END(Args);

  //
  // If there was an error, remove all the interfaces that were installed
  // without any errors
  //
  if (EFI_ERROR(Status)) {
    //
    // Reset the va_arg back to the first argument.
    //
    VA_START(Args, Handle);
    for (; Index > 1; Index--) {
      Protocol = VA_ARG(Args, EFI_GUID *);
      Interface = VA_ARG(Args, VOID *);
      mUninstallProtocolInterface(Handle, Protocol, Interface);
    }

    VA_END(Args);

    *Handle = OldHandle;
  }

  //
  // Done
  //
  mRestoreTPL(OldTpl);
  return Status;
}

EFI_STATUS
EFIAPI
mUninstallMultipleProtocolInterfaces(IN EFI_HANDLE Handle, ...) {
  // TODO:
  return SandboxSyscall1(SANDBOX_SYS_NULL,
                         SANDBOX_SYS_BS_UNINSTALL_MULTIPLE_PROTOCOL_INTERFACES);
}

VOID EFIAPI mCopyMem(IN VOID *Destination, IN VOID *Source, IN UINTN Length) {
  SandboxSyscall1(SANDBOX_SYS_NULL, SANDBOX_SYS_BS_COPY_MEM);
}

VOID EFIAPI mSetMem(IN VOID *Buffer, IN UINTN Size, IN UINT8 Value) {
  SandboxSyscall1(SANDBOX_SYS_NULL, SANDBOX_SYS_BS_SET_MEM);
}

EFI_STATUS
EFIAPI
mCreateEventEx(IN UINT32 Type, IN EFI_TPL NotifyTpl,
               IN EFI_EVENT_NOTIFY NotifyFunction, IN CONST VOID *NotifyContext,
               IN CONST EFI_GUID *EventGroup, OUT EFI_EVENT *Event) {
  return SandboxSyscall1(SANDBOX_SYS_NULL, SANDBOX_SYS_BS_CREATE_EVENT_EX);
}

/*
 *
 * Runtime Services
 *
 */

EFI_STATUS
EFIAPI
mGetTime(OUT EFI_TIME *Time, OUT EFI_TIME_CAPABILITIES *Capabilities) {
  return SandboxSyscall2(SANDBOX_SYS_RT_GET_TIME, (UINT64)Time, (UINT64)Capabilities);
}

EFI_STATUS
EFIAPI
mSetTime(IN EFI_TIME *Time) {
  return SandboxSyscall1(SANDBOX_SYS_RT_SET_TIME, (UINT64)Time);
}

EFI_STATUS
EFIAPI
mGetWakeupTime(OUT BOOLEAN *Enabled, OUT BOOLEAN *Pending, OUT EFI_TIME *Time) {
  return SandboxSyscall1(SANDBOX_SYS_NULL, SANDBOX_SYS_RT_GET_WAKEUP_TIME);
}

EFI_STATUS
EFIAPI
mSetWakeupTime(IN BOOLEAN Enable, IN EFI_TIME *Time) {
  return SandboxSyscall1(SANDBOX_SYS_NULL, SANDBOX_SYS_RT_SET_WAKEUP_TIME);
}

EFI_STATUS
EFIAPI
mSetVirtualAddressMap(IN UINTN MemoryMapSize, IN UINTN DescriptorSize,
                      IN UINT32 DescriptorVersion,
                      IN EFI_MEMORY_DESCRIPTOR *VirtualMap) {
  return SandboxSyscall1(SANDBOX_SYS_NULL,
                         SANDBOX_SYS_RT_SET_VIRTUAL_ADDRESS_MAP);
}

EFI_STATUS
EFIAPI
mConvertPointer(IN UINTN DebugDisposition, IN OUT VOID **Address) {
  return SandboxSyscall1(SANDBOX_SYS_NULL, SANDBOX_SYS_RT_CONVERT_POINTER);
}

EFI_STATUS
EFIAPI
mGetVariable(IN CHAR16 *VariableName, IN EFI_GUID *VendorGuid,
             OUT UINT32 *Attributes OPTIONAL, IN OUT UINTN *DataSize,
             OUT VOID *Data) {
  return SandboxSyscall5(SANDBOX_SYS_RT_GET_VARIABLE, (UINTN)VariableName,
                         (UINTN)VendorGuid, (UINTN)Attributes, (UINTN)DataSize,
                         (UINTN)Data);
}

EFI_STATUS
EFIAPI
mGetNextVariableName(IN OUT UINTN *VariableNameSize,
                     IN OUT CHAR16 *VariableName, IN OUT EFI_GUID *VendorGuid) {
  return SandboxSyscall1(SANDBOX_SYS_NULL,
                         SANDBOX_SYS_RT_GET_NEXT_VARIABLE_NAME);
}

EFI_STATUS
EFIAPI
mSetVariable(IN CHAR16 *VariableName, IN EFI_GUID *VendorGuid,
             IN UINT32 Attributes, IN UINTN DataSize, IN VOID *Data) {
  return SandboxSyscall1(SANDBOX_SYS_NULL, SANDBOX_SYS_RT_SET_VARIABLE);
}

EFI_STATUS
EFIAPI
mGetNextHighMonotonicCount(OUT UINT32 *HighCount) {
  return SandboxSyscall1(SANDBOX_SYS_NULL,
                         SANDBOX_SYS_RT_GET_NEXT_HIGH_MONO_COUNT);
}

VOID EFIAPI mResetSystem(IN EFI_RESET_TYPE ResetType, IN EFI_STATUS ResetStatus,
                         IN UINTN DataSize, IN VOID *ResetData OPTIONAL) {
  SandboxSyscall1(SANDBOX_SYS_NULL, SANDBOX_SYS_RT_RESET_SYSTEM);
}

EFI_STATUS
EFIAPI
mUpdateCapsule(IN EFI_CAPSULE_HEADER **CapsuleHeaderArray,
               IN UINTN CapsuleCount,
               IN EFI_PHYSICAL_ADDRESS ScatterGatherList OPTIONAL) {
  return SandboxSyscall1(SANDBOX_SYS_NULL, SANDBOX_SYS_RT_UPDATE_CAPSULE);
}

EFI_STATUS
EFIAPI
mQueryCapsuleCapabilities(IN EFI_CAPSULE_HEADER **CapsuleHeaderArray,
                          IN UINTN CapsuleCount, OUT UINTN *MaximumCapsuleSize,
                          OUT EFI_RESET_TYPE *ResetType) {
  return SandboxSyscall1(SANDBOX_SYS_NULL,
                         SANDBOX_SYS_RT_QUERY_CAPSULE_CAPABILITIES);
}

EFI_STATUS
EFIAPI
mQueryVariableInfo(IN UINT32 Attributes, OUT UINTN *MaximumVariableStorageSize,
                   OUT UINTN *RemainingVariableStorageSize,
                   OUT UINTN *MaximumVariableSize) {
  return SandboxSyscall1(SANDBOX_SYS_NULL, SANDBOX_SYS_RT_QUERY_VARIABLE_INFO);
}

EFI_STATUS
EFIAPI
mCalculateCrc32(IN VOID *Data, IN UINTN DataSize, OUT UINT32 *Crc32) {
  return SandboxSyscall3(SANDBOX_SYS_BS_CALCULATE_CRC32, (UINTN)Data,
                         (UINTN)DataSize, (UINTN)Crc32);
}

EFI_STATUS
SetSandboxSystemTable(IN EFI_SYSTEM_TABLE *SystemTable) {
  SystemTable->BootServices->RaiseTPL = mRaiseTPL;
  SystemTable->BootServices->RestoreTPL = mRestoreTPL;
  SystemTable->BootServices->AllocatePages = mAllocatePages;
  SystemTable->BootServices->FreePages = mFreePages;
  SystemTable->BootServices->GetMemoryMap = mGetMemoryMap;
  SystemTable->BootServices->AllocatePool = mAllocatePool;
  SystemTable->BootServices->FreePool = mFreePool;
  SystemTable->BootServices->CreateEvent = mCreateEvent;
  SystemTable->BootServices->SetTimer = mSetTimer;
  SystemTable->BootServices->WaitForEvent = mWaitForEvent;
  SystemTable->BootServices->SignalEvent = mSignalEvent;
  SystemTable->BootServices->CloseEvent = mCloseEvent;
  SystemTable->BootServices->CheckEvent = mCheckEvent;
  // SystemTable->BootServices->InstallProtocolInterface =
  //     mInstallProtocolInterface;
  SystemTable->BootServices->ReinstallProtocolInterface =
      mReinstallProtocolInterface;
  SystemTable->BootServices->UninstallProtocolInterface =
      mUninstallProtocolInterface;
  SystemTable->BootServices->HandleProtocol = mHandleProtocol;
  SystemTable->BootServices->RegisterProtocolNotify = mRegisterProtocolNotify;
  SystemTable->BootServices->LocateHandle = mLocateHandle;
  SystemTable->BootServices->LocateDevicePath = mLocateDevicePath;
  SystemTable->BootServices->InstallConfigurationTable =
      mInstallConfigurationTable;
  SystemTable->BootServices->LoadImage = mLoadImage;
  SystemTable->BootServices->StartImage = mStartImage;
  SystemTable->BootServices->Exit = mExit;
  SystemTable->BootServices->UnloadImage = mUnloadImage;
  SystemTable->BootServices->ExitBootServices = mExitBootServices;
  SystemTable->BootServices->GetNextMonotonicCount = NULL;
  SystemTable->BootServices->Stall = mStall;
  SystemTable->BootServices->SetWatchdogTimer = mSetWatchdogTimer;
  SystemTable->BootServices->ConnectController = mConnectController;
  SystemTable->BootServices->DisconnectController = mDisconnectController;
  SystemTable->BootServices->OpenProtocol = mOpenProtocol;
  SystemTable->BootServices->CloseProtocol = mCloseProtocol;
  SystemTable->BootServices->OpenProtocolInformation = mOpenProtocolInformation;
  SystemTable->BootServices->ProtocolsPerHandle = mProtocolsPerHandle;
  SystemTable->BootServices->LocateHandleBuffer = mLocateHandleBuffer;
  SystemTable->BootServices->LocateProtocol = mLocateProtocol;
  // SystemTable->BootServices->InstallMultipleProtocolInterfaces =
  //     mInstallMultipleProtocolInterfaces;
  // SystemTable->BootServices->UninstallMultipleProtocolInterfaces =
  //     mUninstallMultipleProtocolInterfaces;
  SystemTable->BootServices->CalculateCrc32 = mCalculateCrc32;
  SystemTable->BootServices->CopyMem = mCopyMem;
  SystemTable->BootServices->SetMem = mSetMem;
  SystemTable->BootServices->CreateEventEx = mCreateEventEx;

  SystemTable->RuntimeServices->GetTime = mGetTime;
  SystemTable->RuntimeServices->SetTime = mSetTime;
  SystemTable->RuntimeServices->GetWakeupTime = mGetWakeupTime;
  SystemTable->RuntimeServices->SetWakeupTime = mSetWakeupTime;
  SystemTable->RuntimeServices->SetVirtualAddressMap = mSetVirtualAddressMap;
  SystemTable->RuntimeServices->ConvertPointer = mConvertPointer;
  SystemTable->RuntimeServices->GetVariable = mGetVariable;
  SystemTable->RuntimeServices->GetNextVariableName = mGetNextVariableName;
  SystemTable->RuntimeServices->SetVariable = mSetVariable;
  SystemTable->RuntimeServices->GetNextHighMonotonicCount =
      mGetNextHighMonotonicCount;
  SystemTable->RuntimeServices->ResetSystem = mResetSystem;
  SystemTable->RuntimeServices->UpdateCapsule = mUpdateCapsule;
  SystemTable->RuntimeServices->QueryCapsuleCapabilities =
      mQueryCapsuleCapabilities;
  SystemTable->RuntimeServices->QueryVariableInfo = mQueryVariableInfo;
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
SandboxSyscallInjectionLibConstructor(IN EFI_HANDLE ImageHandle,
                             IN EFI_SYSTEM_TABLE *SystemTable) {

  return EFI_SUCCESS;
}
