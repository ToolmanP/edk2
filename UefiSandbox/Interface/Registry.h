#ifndef SANDBOX_PROTOCOL_H_
#define SANDBOX_PROTOCOL_H_

#include "Base.h"
#include "Interface.h"
#include "Jumper.h"
#include "Magisk.h"
#include "PointerList.h"
#include "ProcessorBind.h"
#include "Reflect.h"
#include "Uefi/UefiBaseType.h"
#include "UefiSandbox.h"
/*
 * InterfaceSandbox is a copy of the real Interface, the content is same.
 * The difference is that InterfaceSandbox is allocataed in sandbox shared
 * memory, so every sandbox and the core can access its variables.
 *
 * For CoreInterface, the variables (function pointer & data pointer) are kernel
 * address, call to these functions from sandbox drivers will cause exception.
 *
 * For SandboxInterface, the variables are user address, calling from other
 * sandboxes or from the core will cause exception as the memory is either not
 * mapped or not executable.
 */

/* Used by Sandbox to reference sandboxes they have opened or installed */
typedef struct {
  /* Collection Node */
  LIST_ENTRY CollectionNode;
  /* Sandbox Node */
  LIST_ENTRY SandboxNode;
  /* Protocol ID  Kept as a Copy for checking reinstall.*/
  const EFI_GUID *ID;
  /* Sandbox Interface*/
  SANDBOX_INTERFACE Sandboxed;
  const REFLECT_PROTOCOL *Desc;
} InterfaceRegistryEntry;

typedef struct {
  LIST_ENTRY ListNode;
  LIST_ENTRY EntryList;
  EFI_GUID ID;
  const REFLECT_PROTOCOL *Desc;
} InterfaceRegistryCollection;

typedef InterfaceRegistryEntry INTERFACE_REGISTRY_ENTRY;
typedef InterfaceRegistryCollection INTERFACE_REGISTRY_COLLECTION;

VOID InitSandboxRegistry(VOID);

EFI_STATUS InstallSandboxInterface(IN UEFI_SANDBOX *Sandbox,
                                   IN CONST EFI_GUID *ProtocolID,
                                   IN EFI_HANDLE Handle,
                                   IN VOID *Opaque);
EFI_STATUS ReinstallSandboxInterface(IN UEFI_SANDBOX *Sandbox,
                                     IN CONST EFI_GUID *ProtocolID,
                                     IN VOID *Handle,
                                     IN VOID *OldInterface,
                                     IN VOID *NewInterface);
EFI_STATUS UninstallSandboxInterface(IN UEFI_SANDBOX *Sandbox,
                                     IN CONST EFI_GUID *ProtocolID,
                                     IN VOID *Handle,
                                     IN VOID *Interface);
EFI_STATUS LocateSandboxInterface(IN UEFI_SANDBOX *Sandbox,
                                  IN EFI_HANDLE Handle,
                                  IN EFI_GUID *ProtocolID,
                                  IN BOOLEAN ForCore,
                                  OUT LOCATED_INTERFACE **LocatedInterface);

EFI_STATUS OpenSandboxInterface(IN UEFI_SANDBOX *Sandbox, IN EFI_HANDLE Handle,
                                IN EFI_GUID *ProtocolID,
                                IN EFI_HANDLE AgentHandle,
                                IN EFI_HANDLE ControllerHandle,
                                IN UINT32 Attributes,
                                OUT LOCATED_INTERFACE **LocatedInterface);

EFI_STATUS CloseSandboxInterface(
    IN UEFI_SANDBOX *Sandbox, IN EFI_HANDLE Handle, IN EFI_GUID *ProtocolID,
    IN EFI_HANDLE AgentHandle, IN EFI_HANDLE ControllerHandle);

VOID FreeSandboxInterfaces(IN UEFI_SANDBOX *Sandbox);

#endif
