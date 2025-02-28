#include <Library/BaseMemoryLib.h>

#include <Interface/Interface.h>
#include <Memory/Malloc.h>
#include <Memory/Memory.h>
#include <SystemTable/Blob.h>
#include <SystemTable/SystemTable.h>
#include <Utils/Logger.h>

STATIC VOID InitializeBootServices(UEFI_SANDBOX *Sandbox,
                                   EFI_BOOT_SERVICES *DstBT) {
  // Task Priority Services
  DstBT->RaiseTPL = PTR_PHYS_TO_VIRT(AllocateSandboxCodeBuffer(
      Sandbox, C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_RAISE_TPL)));
  CopyMem(DstBT->RaiseTPL, C_SYS_BLOB_START(SANDBOX_SYS_BS_RAISE_TPL),
          C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_RAISE_TPL));

  DstBT->RestoreTPL = PTR_PHYS_TO_VIRT(AllocateSandboxCodeBuffer(
      Sandbox, C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_RESTORE_TPL)));
  CopyMem(DstBT->RestoreTPL, C_SYS_BLOB_START(SANDBOX_SYS_BS_RESTORE_TPL),
          C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_RESTORE_TPL));

  // Memory Services
  DstBT->AllocatePages = PTR_PHYS_TO_VIRT(AllocateSandboxCodeBuffer(
      Sandbox, C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_ALLOCATE_PAGES)));
  CopyMem(DstBT->AllocatePages, C_SYS_BLOB_START(SANDBOX_SYS_BS_ALLOCATE_PAGES),
          C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_ALLOCATE_PAGES));

  DstBT->FreePages = PTR_PHYS_TO_VIRT(AllocateSandboxCodeBuffer(
      Sandbox, C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_FREE_PAGES)));
  CopyMem(DstBT->FreePages, C_SYS_BLOB_START(SANDBOX_SYS_BS_FREE_PAGES),
          C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_FREE_PAGES));

  DstBT->GetMemoryMap = PTR_PHYS_TO_VIRT(AllocateSandboxCodeBuffer(
      Sandbox, C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_GET_MEMORY_MAP)));
  CopyMem(DstBT->GetMemoryMap, C_SYS_BLOB_START(SANDBOX_SYS_BS_GET_MEMORY_MAP),
          C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_GET_MEMORY_MAP));

  DstBT->AllocatePool = PTR_PHYS_TO_VIRT(AllocateSandboxCodeBuffer(
      Sandbox, C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_ALLOCATE_POOL)));
  CopyMem(DstBT->AllocatePool, C_SYS_BLOB_START(SANDBOX_SYS_BS_ALLOCATE_POOL),
          C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_ALLOCATE_POOL));

  DstBT->FreePool = PTR_PHYS_TO_VIRT(AllocateSandboxCodeBuffer(
      Sandbox, C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_FREE_POOL)));
  CopyMem(DstBT->FreePool, C_SYS_BLOB_START(SANDBOX_SYS_BS_FREE_POOL),
          C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_FREE_POOL));

  // Event & Timer Services
  DstBT->CreateEvent = PTR_PHYS_TO_VIRT(AllocateSandboxCodeBuffer(
      Sandbox, C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_CREATE_EVENT)));
  CopyMem(DstBT->CreateEvent, C_SYS_BLOB_START(SANDBOX_SYS_BS_CREATE_EVENT),
          C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_CREATE_EVENT));

  DstBT->SetTimer = PTR_PHYS_TO_VIRT(AllocateSandboxCodeBuffer(
      Sandbox, C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_SET_TIMER)));
  CopyMem(DstBT->SetTimer, C_SYS_BLOB_START(SANDBOX_SYS_BS_SET_TIMER),
          C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_SET_TIMER));

  DstBT->WaitForEvent = PTR_PHYS_TO_VIRT(AllocateSandboxCodeBuffer(
      Sandbox, C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_WAIT_FOR_EVENT)));
  CopyMem(DstBT->WaitForEvent, C_SYS_BLOB_START(SANDBOX_SYS_BS_WAIT_FOR_EVENT),
          C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_WAIT_FOR_EVENT));

  DstBT->SignalEvent = PTR_PHYS_TO_VIRT(AllocateSandboxCodeBuffer(
      Sandbox, C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_SIGNAL_EVENT)));
  CopyMem(DstBT->SignalEvent, C_SYS_BLOB_START(SANDBOX_SYS_BS_SIGNAL_EVENT),
          C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_SIGNAL_EVENT));

  DstBT->CloseEvent = PTR_PHYS_TO_VIRT(AllocateSandboxCodeBuffer(
      Sandbox, C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_CLOSE_EVENT)));
  CopyMem(DstBT->CloseEvent, C_SYS_BLOB_START(SANDBOX_SYS_BS_CLOSE_EVENT),
          C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_CLOSE_EVENT));

  DstBT->CheckEvent = PTR_PHYS_TO_VIRT(AllocateSandboxCodeBuffer(
      Sandbox, C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_CHECK_EVENT)));
  CopyMem(DstBT->CheckEvent, C_SYS_BLOB_START(SANDBOX_SYS_BS_CHECK_EVENT),
          C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_CHECK_EVENT));

  // Protocol Handler Services
  DstBT->InstallProtocolInterface = PTR_PHYS_TO_VIRT(AllocateSandboxCodeBuffer(
      Sandbox, C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_INSTALL_PROTOCOL_INTERFACE)));
  CopyMem(DstBT->InstallProtocolInterface,
          C_SYS_BLOB_START(SANDBOX_SYS_BS_INSTALL_PROTOCOL_INTERFACE),
          C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_INSTALL_PROTOCOL_INTERFACE));

  DstBT->ReinstallProtocolInterface =
      PTR_PHYS_TO_VIRT(AllocateSandboxCodeBuffer(
          Sandbox,
          C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_REINSTALL_PROTOCOL_INTERFACE)));
  CopyMem(DstBT->ReinstallProtocolInterface,
          C_SYS_BLOB_START(SANDBOX_SYS_BS_REINSTALL_PROTOCOL_INTERFACE),
          C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_REINSTALL_PROTOCOL_INTERFACE));

  DstBT->UninstallProtocolInterface =
      PTR_PHYS_TO_VIRT(AllocateSandboxCodeBuffer(
          Sandbox,
          C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_UNINSTALL_PROTOCOL_INTERFACE)));
  CopyMem(DstBT->UninstallProtocolInterface,
          C_SYS_BLOB_START(SANDBOX_SYS_BS_UNINSTALL_PROTOCOL_INTERFACE),
          C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_UNINSTALL_PROTOCOL_INTERFACE));

  DstBT->HandleProtocol = PTR_PHYS_TO_VIRT(AllocateSandboxCodeBuffer(
      Sandbox, C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_HANDLE_PROTOCOL)));
  CopyMem(DstBT->HandleProtocol,
          C_SYS_BLOB_START(SANDBOX_SYS_BS_HANDLE_PROTOCOL),
          C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_HANDLE_PROTOCOL));

  DstBT->Reserved = NULL;

  DstBT->RegisterProtocolNotify = PTR_PHYS_TO_VIRT(AllocateSandboxCodeBuffer(
      Sandbox, C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_REGISTER_PROTOCOL_NOTIFY)));
  CopyMem(DstBT->RegisterProtocolNotify,
          C_SYS_BLOB_START(SANDBOX_SYS_BS_REGISTER_PROTOCOL_NOTIFY),
          C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_REGISTER_PROTOCOL_NOTIFY));

  DstBT->LocateHandle = PTR_PHYS_TO_VIRT(AllocateSandboxCodeBuffer(
      Sandbox, C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_LOCATE_HANDLE)));
  CopyMem(DstBT->LocateHandle, C_SYS_BLOB_START(SANDBOX_SYS_BS_LOCATE_HANDLE),
          C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_LOCATE_HANDLE));

  DstBT->LocateDevicePath = PTR_PHYS_TO_VIRT(AllocateSandboxCodeBuffer(
      Sandbox, C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_LOCATE_DEVICE_PATH)));
  CopyMem(DstBT->LocateDevicePath,
          C_SYS_BLOB_START(SANDBOX_SYS_BS_LOCATE_DEVICE_PATH),
          C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_LOCATE_DEVICE_PATH));

  DstBT->InstallConfigurationTable = PTR_PHYS_TO_VIRT(AllocateSandboxCodeBuffer(
      Sandbox, C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_INSTALL_CONFIGURATION_TABLE)));
  CopyMem(DstBT->InstallConfigurationTable,
          C_SYS_BLOB_START(SANDBOX_SYS_BS_INSTALL_CONFIGURATION_TABLE),
          C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_INSTALL_CONFIGURATION_TABLE));

  // Image Services
  DstBT->LoadImage = NULL;
  DstBT->StartImage = NULL;
  DstBT->UnloadImage = NULL;

  DstBT->Exit = PTR_PHYS_TO_VIRT(
      AllocateSandboxCodeBuffer(Sandbox, C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_EXIT)));
  CopyMem(DstBT->Exit, C_SYS_BLOB_START(SANDBOX_SYS_BS_EXIT),
          C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_EXIT));

  DstBT->ExitBootServices = PTR_PHYS_TO_VIRT(AllocateSandboxCodeBuffer(
      Sandbox, C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_EXIT_BOOT_SERVICES)));
  CopyMem(DstBT->ExitBootServices,
          C_SYS_BLOB_START(SANDBOX_SYS_BS_EXIT_BOOT_SERVICES),
          C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_EXIT_BOOT_SERVICES));

  // Miscellaneous Services
  DstBT->GetNextMonotonicCount = PTR_PHYS_TO_VIRT(AllocateSandboxCodeBuffer(
      Sandbox, C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_GET_NEXT_MONOTONIC_COUNT)));
  CopyMem(DstBT->GetNextMonotonicCount,
          C_SYS_BLOB_START(SANDBOX_SYS_BS_GET_NEXT_MONOTONIC_COUNT),
          C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_GET_NEXT_MONOTONIC_COUNT));

  DstBT->Stall = PTR_PHYS_TO_VIRT(AllocateSandboxCodeBuffer(
      Sandbox, C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_STALL)));
  CopyMem(DstBT->Stall, C_SYS_BLOB_START(SANDBOX_SYS_BS_STALL),
          C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_STALL));

  DstBT->SetWatchdogTimer = PTR_PHYS_TO_VIRT(AllocateSandboxCodeBuffer(
      Sandbox, C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_SET_WATCHDOG_TIMER)));
  CopyMem(DstBT->SetWatchdogTimer,
          C_SYS_BLOB_START(SANDBOX_SYS_BS_SET_WATCHDOG_TIMER),
          C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_SET_WATCHDOG_TIMER));

  // DriverSupport Services
  DstBT->ConnectController = PTR_PHYS_TO_VIRT(AllocateSandboxCodeBuffer(
      Sandbox, C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_CONNECT_CONTROLLER)));
  CopyMem(DstBT->ConnectController,
          C_SYS_BLOB_START(SANDBOX_SYS_BS_CONNECT_CONTROLLER),
          C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_CONNECT_CONTROLLER));

  DstBT->DisconnectController = PTR_PHYS_TO_VIRT(AllocateSandboxCodeBuffer(
      Sandbox, C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_DISCONNECT_CONTROLLER)));
  CopyMem(DstBT->DisconnectController,
          C_SYS_BLOB_START(SANDBOX_SYS_BS_DISCONNECT_CONTROLLER),
          C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_DISCONNECT_CONTROLLER));

  // Open and Close Protocol Services
  DstBT->OpenProtocol = PTR_PHYS_TO_VIRT(AllocateSandboxCodeBuffer(
      Sandbox, C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_OPEN_PROTOCOL)));
  CopyMem(DstBT->OpenProtocol, C_SYS_BLOB_START(SANDBOX_SYS_BS_OPEN_PROTOCOL),
          C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_OPEN_PROTOCOL));

  DstBT->CloseProtocol = PTR_PHYS_TO_VIRT(AllocateSandboxCodeBuffer(
      Sandbox, C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_CLOSE_PROTOCOL)));
  CopyMem(DstBT->CloseProtocol, C_SYS_BLOB_START(SANDBOX_SYS_BS_CLOSE_PROTOCOL),
          C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_CLOSE_PROTOCOL));

  DstBT->OpenProtocolInformation = PTR_PHYS_TO_VIRT(AllocateSandboxCodeBuffer(
      Sandbox, C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_OPEN_PROTOCOL_INFORMATION)));
  CopyMem(DstBT->OpenProtocolInformation,
          C_SYS_BLOB_START(SANDBOX_SYS_BS_OPEN_PROTOCOL_INFORMATION),
          C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_OPEN_PROTOCOL_INFORMATION));

  // Library Services
  DstBT->ProtocolsPerHandle = PTR_PHYS_TO_VIRT(AllocateSandboxCodeBuffer(
      Sandbox, C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_PROTOCOLS_PER_HANDLE)));
  CopyMem(DstBT->ProtocolsPerHandle,
          C_SYS_BLOB_START(SANDBOX_SYS_BS_PROTOCOLS_PER_HANDLE),
          C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_PROTOCOLS_PER_HANDLE));

  DstBT->LocateHandleBuffer = PTR_PHYS_TO_VIRT(AllocateSandboxCodeBuffer(
      Sandbox, C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_LOCATE_HANDLE_BUFFER)));
  CopyMem(DstBT->LocateHandleBuffer,
          C_SYS_BLOB_START(SANDBOX_SYS_BS_LOCATE_HANDLE_BUFFER),
          C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_LOCATE_HANDLE_BUFFER));

  DstBT->LocateProtocol = PTR_PHYS_TO_VIRT(AllocateSandboxCodeBuffer(
      Sandbox, C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_LOCATE_PROTOCOL)));
  CopyMem(DstBT->LocateProtocol,
          C_SYS_BLOB_START(SANDBOX_SYS_BS_LOCATE_PROTOCOL),
          C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_LOCATE_PROTOCOL));

  DstBT->InstallMultipleProtocolInterfaces =
      PTR_PHYS_TO_VIRT(AllocateSandboxCodeBuffer(
          Sandbox, C_SYS_BLOB_SIZE(
                       SANDBOX_SYS_BS_INSTALL_MULTIPLE_PROTOCOL_INTERFACES)));
  CopyMem(DstBT->InstallMultipleProtocolInterfaces,
          C_SYS_BLOB_START(SANDBOX_SYS_BS_INSTALL_MULTIPLE_PROTOCOL_INTERFACES),
          C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_INSTALL_MULTIPLE_PROTOCOL_INTERFACES));

  DstBT->UninstallMultipleProtocolInterfaces =
      PTR_PHYS_TO_VIRT(AllocateSandboxCodeBuffer(
          Sandbox, C_SYS_BLOB_SIZE(
                       SANDBOX_SYS_BS_UNINSTALL_MULTIPLE_PROTOCOL_INTERFACES)));
  CopyMem(
      DstBT->UninstallMultipleProtocolInterfaces,
      C_SYS_BLOB_START(SANDBOX_SYS_BS_UNINSTALL_MULTIPLE_PROTOCOL_INTERFACES),
      C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_UNINSTALL_MULTIPLE_PROTOCOL_INTERFACES));

  // 32-bit CRC Services
  DstBT->CalculateCrc32 = PTR_PHYS_TO_VIRT(AllocateSandboxCodeBuffer(
      Sandbox, C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_CALCULATE_CRC32)));
  CopyMem(DstBT->CalculateCrc32,
          C_SYS_BLOB_START(SANDBOX_SYS_BS_CALCULATE_CRC32),
          C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_CALCULATE_CRC32));

  // Miscellaneous Services
  DstBT->CopyMem = PTR_PHYS_TO_VIRT(AllocateSandboxCodeBuffer(
      Sandbox, C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_COPY_MEM)));
  CopyMem(DstBT->CopyMem, C_SYS_BLOB_START(SANDBOX_SYS_BS_COPY_MEM),
          C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_COPY_MEM));

  DstBT->SetMem = PTR_PHYS_TO_VIRT(AllocateSandboxCodeBuffer(
      Sandbox, C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_SET_MEM)));
  CopyMem(DstBT->SetMem, C_SYS_BLOB_START(SANDBOX_SYS_BS_SET_MEM),
          C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_SET_MEM));

  DstBT->CreateEventEx = PTR_PHYS_TO_VIRT(AllocateSandboxCodeBuffer(
      Sandbox, C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_CREATE_EVENT_EX)));
  CopyMem(DstBT->CreateEventEx,
          C_SYS_BLOB_START(SANDBOX_SYS_BS_CREATE_EVENT_EX),
          C_SYS_BLOB_SIZE(SANDBOX_SYS_BS_CREATE_EVENT_EX));
}

STATIC VOID InitializeRuntimeServices(IN UEFI_SANDBOX *Sandbox,
                                      IN EFI_RUNTIME_SERVICES *DstRT) {
  // GetTime
  DstRT->GetTime = PTR_PHYS_TO_VIRT(AllocateSandboxCodeBuffer(
      Sandbox, C_SYS_BLOB_SIZE(SANDBOX_SYS_RT_GET_TIME)));
  CopyMem(DstRT->GetTime, C_SYS_BLOB_START(SANDBOX_SYS_RT_GET_TIME),
          C_SYS_BLOB_SIZE(SANDBOX_SYS_RT_GET_TIME));

  // SetTime
  DstRT->SetTime = PTR_PHYS_TO_VIRT(AllocateSandboxCodeBuffer(
      Sandbox, C_SYS_BLOB_SIZE(SANDBOX_SYS_RT_SET_TIME)));
  CopyMem(DstRT->SetTime, C_SYS_BLOB_START(SANDBOX_SYS_RT_SET_TIME),
          C_SYS_BLOB_SIZE(SANDBOX_SYS_RT_SET_TIME));

  // GetWakeupTime
  DstRT->GetWakeupTime = PTR_PHYS_TO_VIRT(AllocateSandboxCodeBuffer(
      Sandbox, C_SYS_BLOB_SIZE(SANDBOX_SYS_RT_GET_WAKEUP_TIME)));
  CopyMem(DstRT->GetWakeupTime,
          C_SYS_BLOB_START(SANDBOX_SYS_RT_GET_WAKEUP_TIME),
          C_SYS_BLOB_SIZE(SANDBOX_SYS_RT_GET_WAKEUP_TIME));

  // SetWakeupTime
  DstRT->SetWakeupTime = PTR_PHYS_TO_VIRT(AllocateSandboxCodeBuffer(
      Sandbox, C_SYS_BLOB_SIZE(SANDBOX_SYS_RT_SET_WAKEUP_TIME)));
  CopyMem(DstRT->SetWakeupTime,
          C_SYS_BLOB_START(SANDBOX_SYS_RT_SET_WAKEUP_TIME),
          C_SYS_BLOB_SIZE(SANDBOX_SYS_RT_SET_WAKEUP_TIME));

  // SetVirtualAddressMap
  DstRT->SetVirtualAddressMap = PTR_PHYS_TO_VIRT(AllocateSandboxCodeBuffer(
      Sandbox, C_SYS_BLOB_SIZE(SANDBOX_SYS_RT_SET_VIRTUAL_ADDRESS_MAP)));
  CopyMem(DstRT->SetVirtualAddressMap,
          C_SYS_BLOB_START(SANDBOX_SYS_RT_SET_VIRTUAL_ADDRESS_MAP),
          C_SYS_BLOB_SIZE(SANDBOX_SYS_RT_SET_VIRTUAL_ADDRESS_MAP));

  // ConvertPointer
  DstRT->ConvertPointer = PTR_PHYS_TO_VIRT(AllocateSandboxCodeBuffer(
      Sandbox, C_SYS_BLOB_SIZE(SANDBOX_SYS_RT_CONVERT_POINTER)));
  CopyMem(DstRT->ConvertPointer,
          C_SYS_BLOB_START(SANDBOX_SYS_RT_CONVERT_POINTER),
          C_SYS_BLOB_SIZE(SANDBOX_SYS_RT_CONVERT_POINTER));

  // GetVariable
  DstRT->GetVariable = PTR_PHYS_TO_VIRT(AllocateSandboxCodeBuffer(
      Sandbox, C_SYS_BLOB_SIZE(SANDBOX_SYS_RT_GET_VARIABLE)));
  CopyMem(DstRT->GetVariable, C_SYS_BLOB_START(SANDBOX_SYS_RT_GET_VARIABLE),
          C_SYS_BLOB_SIZE(SANDBOX_SYS_RT_GET_VARIABLE));

  // GetNextVariableName
  DstRT->GetNextVariableName = PTR_PHYS_TO_VIRT(AllocateSandboxCodeBuffer(
      Sandbox, C_SYS_BLOB_SIZE(SANDBOX_SYS_RT_GET_NEXT_VARIABLE_NAME)));
  CopyMem(DstRT->GetNextVariableName,
          C_SYS_BLOB_START(SANDBOX_SYS_RT_GET_NEXT_VARIABLE_NAME),
          C_SYS_BLOB_SIZE(SANDBOX_SYS_RT_GET_NEXT_VARIABLE_NAME));

  // SetVariable
  DstRT->SetVariable = PTR_PHYS_TO_VIRT(AllocateSandboxCodeBuffer(
      Sandbox, C_SYS_BLOB_SIZE(SANDBOX_SYS_RT_SET_VARIABLE)));
  CopyMem(DstRT->SetVariable, C_SYS_BLOB_START(SANDBOX_SYS_RT_SET_VARIABLE),
          C_SYS_BLOB_SIZE(SANDBOX_SYS_RT_SET_VARIABLE));

  // GetNextHighMonotonicCount
  DstRT->GetNextHighMonotonicCount = PTR_PHYS_TO_VIRT(AllocateSandboxCodeBuffer(
      Sandbox, C_SYS_BLOB_SIZE(SANDBOX_SYS_RT_GET_NEXT_HIGH_MONO_COUNT)));
  CopyMem(DstRT->GetNextHighMonotonicCount,
          C_SYS_BLOB_START(SANDBOX_SYS_RT_GET_NEXT_HIGH_MONO_COUNT),
          C_SYS_BLOB_SIZE(SANDBOX_SYS_RT_GET_NEXT_HIGH_MONO_COUNT));

  // ResetSystem
  DstRT->ResetSystem = PTR_PHYS_TO_VIRT(AllocateSandboxCodeBuffer(
      Sandbox, C_SYS_BLOB_SIZE(SANDBOX_SYS_RT_RESET_SYSTEM)));
  CopyMem(DstRT->ResetSystem, C_SYS_BLOB_START(SANDBOX_SYS_RT_RESET_SYSTEM),
          C_SYS_BLOB_SIZE(SANDBOX_SYS_RT_RESET_SYSTEM));

  // UpdateCapsule
  DstRT->UpdateCapsule = PTR_PHYS_TO_VIRT(AllocateSandboxCodeBuffer(
      Sandbox, C_SYS_BLOB_SIZE(SANDBOX_SYS_RT_UPDATE_CAPSULE)));
  CopyMem(DstRT->UpdateCapsule, C_SYS_BLOB_START(SANDBOX_SYS_RT_UPDATE_CAPSULE),
          C_SYS_BLOB_SIZE(SANDBOX_SYS_RT_UPDATE_CAPSULE));

  // QueryCapsuleCapabilities
  DstRT->QueryCapsuleCapabilities = PTR_PHYS_TO_VIRT(AllocateSandboxCodeBuffer(
      Sandbox, C_SYS_BLOB_SIZE(SANDBOX_SYS_RT_QUERY_CAPSULE_CAPABILITIES)));
  CopyMem(DstRT->QueryCapsuleCapabilities,
          C_SYS_BLOB_START(SANDBOX_SYS_RT_QUERY_CAPSULE_CAPABILITIES),
          C_SYS_BLOB_SIZE(SANDBOX_SYS_RT_QUERY_CAPSULE_CAPABILITIES));

  // QueryVariableInfo
  DstRT->QueryVariableInfo = PTR_PHYS_TO_VIRT(AllocateSandboxCodeBuffer(
      Sandbox, C_SYS_BLOB_SIZE(SANDBOX_SYS_RT_QUERY_VARIABLE_INFO)));
  CopyMem(DstRT->QueryVariableInfo,
          C_SYS_BLOB_START(SANDBOX_SYS_RT_QUERY_VARIABLE_INFO),
          C_SYS_BLOB_SIZE(SANDBOX_SYS_RT_QUERY_VARIABLE_INFO));
}

VOID SetSystemTableProtocolInterface(IN UEFI_SANDBOX *Sandbox,
                                     IN EFI_SYSTEM_TABLE *DstST,
                                     IN EFI_SYSTEM_TABLE *SrcST) {
  LOCATED_INTERFACE *Located;
  Located = NULL;
  UNUSED(Located);
  /* ConOut*/
  if (SrcST->ConOut != NULL) {
    DstST->ConOut = NULL; // FIXME: Fix this src
  }
  InitializeBootServices(Sandbox, DstST->BootServices);
  InitializeRuntimeServices(Sandbox, DstST->RuntimeServices);
  // You can repeat the same pattern for all other Boot Services functions you
  // want to fill in.
}
