#include "Registry.h"
#include "Base.h"
#include "Interface/PointerList.h"
#include "Interface/Reflect.h"
#include "Library/BaseLib.h"
#include "Library/BaseMemoryLib.h"
#include "Library/MemoryAllocationLib.h"

#include "Library/UefiBootServicesTableLib.h"
#include "Library/UefiLib.h"
#include "Magisk.h"
#include "Memory.h"
#include "Print.h"
#include "ProcessorBind.h"
#include "Protocol/DiskIo.h"
#include "Protocol/SerialIo.h"
#include "Proxy/ProtocolProxy.h"
#include "Uefi/UefiBaseType.h"
#include "Uefi/UefiSpec.h"
#include "UefiSandbox.h"

LIST_ENTRY mInstalledInterfaceCollectionRegistry =
    INITIALIZE_LIST_HEAD_VARIABLE(mInstalledInterfaceCollectionRegistry);

VOID InitSandboxRegistry() {}

STATIC SANDBOX_INTERFACE *
GetInstalledSandboxInterface(IN CONST UEFI_SANDBOX *Sandbox,
                             IN CONST EFI_GUID *ProtocolID,
                             IN CONST VOID *Handle, IN CONST VOID *Opaque) {
  LIST_ENTRY *Link;
  INTERFACE_REGISTRY_ENTRY *Entry;
  BASE_LIST_FOR_EACH(Link, &Sandbox->InstalledInterfaces) {
    Entry = BASE_CR(Link, INTERFACE_REGISTRY_ENTRY, SandboxNode);
    if (Entry->Sandboxed.Handle == Handle &&
        Entry->Sandboxed.Opaque == Opaque &&

        CompareGuid(Entry->ID, ProtocolID)) {
      return &Entry->Sandboxed;
    }
  }
  return NULL;
}

// Get or Allocate a INTERFACE_COLLECTION
STATIC INTERFACE_REGISTRY_COLLECTION *
MustGetInterfaceCollection(IN CONST EFI_GUID *ProtocolID) {
  LIST_ENTRY *Link;
  INTERFACE_REGISTRY_COLLECTION *Collection;
  REFLECT_PROTOCOL *Desc;

  BASE_LIST_FOR_EACH(Link, &mInstalledInterfaceCollectionRegistry) {
    Collection = BASE_CR(Link, INTERFACE_REGISTRY_COLLECTION, ListNode);
    if (CompareGuid(&Collection->ID, ProtocolID)) {
      return Collection;
    }
  }

  Collection = AllocatePool(sizeof(*Collection));
  CopyGuid(&Collection->ID, ProtocolID);
  GetProtocol(&Collection->ID, &Desc);
  Collection->Desc = Desc;
  InitializeListHead(&Collection->EntryList);
  InsertTailList(&mInstalledInterfaceCollectionRegistry, &Collection->ListNode);
  return Collection;
}

EFI_STATUS InstallSandboxInterface(IN UEFI_SANDBOX *Sandbox,
                                   IN CONST EFI_GUID *ProtocolID,
                                   IN EFI_HANDLE Handle, IN VOID *Interface) {
  INTERFACE_REGISTRY_COLLECTION *Collection;
  INTERFACE_REGISTRY_ENTRY *Entry;
  EFI_STATUS Status;
  REFLECT_PROTOCOL *Desc;

  if (Interface == NULL) {
    SBError("Sandbox: %d, Protocol: %g, try to install NULL Interface\n",
            Sandbox->SandboxID, ProtocolID);
    Status = EFI_INVALID_PARAMETER;
    goto err;
  }

  Status = GetProtocol(ProtocolID, &Desc);

  if (EFI_ERROR(Status)) {
    goto err;
  }

  if (GetInstalledSandboxInterface(Sandbox, ProtocolID, Handle, Interface) !=
      NULL) {
    SBError("Sandbox: %d, Protocol: %g, Interface already installed\n",
            Sandbox->SandboxID, ProtocolID);
    Status = EFI_INVALID_PARAMETER;
    goto err;
  }

  Collection = MustGetInterfaceCollection(ProtocolID);

  ASSERT(Collection != NULL);

  /* Add the installed interface to the sandbox's installed interface list */
  Entry = AllocatePool(sizeof(*Entry));
  Entry->ID = &Collection->ID;
  Entry->Sandboxed.Handle = Handle;
  Entry->Sandboxed.Opaque = Interface;
  Entry->Sandboxed.SandboxID = Sandbox->SandboxID;
  Entry->Sandboxed.RefCount = 0;
  Entry->Sandboxed.OpenCount = 0;
  Entry->Sandboxed.Desc = Collection->Desc;

  InitializeListHead(&Entry->Sandboxed.LocatedList);

  InsertTailList(&Collection->EntryList, &Entry->CollectionNode);
  InsertTailList(&Sandbox->InstalledInterfaces, &Entry->SandboxNode);

  SBDebug("Sandbox: %d, Protocol: %g, Handle: 0x%p "
          "Interface: 0x%p\n",
          Sandbox->SandboxID, ProtocolID, Handle, Interface);

  return EFI_SUCCESS;
err:
  ASSERT(0);
  return Status;
}

EFI_STATUS ReinstallSandboxInterface(IN UEFI_SANDBOX *Sandbox,
                                     CONST EFI_GUID *ProtocolID,
                                     IN VOID *Handle, VOID *OldInterface,
                                     VOID *NewInterface) {
  LIST_ENTRY *Link;
  INTERFACE_REGISTRY_ENTRY *Entry;
  SANDBOX_INTERFACE *SI;

  if (NewInterface == NULL) {
    SBError("Sandbox: %d, Protocol: %g, try to reinstall NULL Interface\n",
            Sandbox->SandboxID, ProtocolID);
    return EFI_INVALID_PARAMETER;
  }

  BASE_LIST_FOR_EACH(Link, &Sandbox->InstalledInterfaces) {
    Entry = BASE_CR(Link, INTERFACE_REGISTRY_ENTRY, SandboxNode);
    SI = &Entry->Sandboxed;
    if (SI->Opaque == OldInterface && SI->Handle == Handle &&
        CompareGuid(Entry->ID, ProtocolID)) {
      SI->Opaque = NewInterface;
      return EFI_SUCCESS;
    }
  }
  return EFI_NOT_FOUND;
}

STATIC EFI_STATUS
FreeSandboxInterfaceEntry(IN INTERFACE_REGISTRY_ENTRY *Entry) {

  if (Entry->Sandboxed.RefCount != 0) {
    ASSERT(0);
  }

  /* Remove SANDBOX_INTERFACE from global list */
  RemoveEntryList(&Entry->SandboxNode);
  /* Remove InterfaceProxy from sandbox's installed interface list */
  RemoveEntryList(&Entry->CollectionNode);
  /* Free Pool */
  FreePool(Entry);

  return EFI_SUCCESS;
}

EFI_STATUS UninstallSandboxInterface(IN UEFI_SANDBOX *Sandbox,
                                     IN CONST EFI_GUID *ProtocolID,
                                     IN VOID *Handle, IN VOID *Interface) {
  LIST_ENTRY *Link;
  INTERFACE_REGISTRY_ENTRY *Cursor, *Entry;

  Entry = NULL;
  /* search the SANDBOX_INTERFACE from sandbox's installed interfaces list */
  BASE_LIST_FOR_EACH(Link, &Sandbox->InstalledInterfaces) {
    Cursor = BASE_CR(Link, INTERFACE_REGISTRY_ENTRY, SandboxNode);
    if (Cursor->Sandboxed.Handle == Handle &&
        Cursor->Sandboxed.Opaque == Interface &&
        CompareGuid(Cursor->ID, ProtocolID) == TRUE) {
      Entry = Cursor;
      break;
    }
  }
  if (Entry == NULL) {
    return EFI_NOT_FOUND;
  }
  if (Interface != NULL) {
    return FreeSandboxInterfaceEntry(Entry);
  }
  return EFI_INVALID_PARAMETER;
}

STATIC VOID InitLocatedSandboxInterface(
    IN UEFI_SANDBOX *Sandbox, IN LOCATED_INTERFACE *Located,
    IN CONST EFI_GUID *ProtocolID, IN SANDBOX_INTERFACE *Sandboxed,
    IN EFI_HANDLE AgentHandle, IN EFI_HANDLE ControllerHandle,
    IN UINT32 Attributes, IN BOOLEAN ForCore) {

  REFLECT_PROTOCOL *Protocol;
  CONST VOID *Delegated;

  GetProtocol(ProtocolID, &Protocol);
  InitPointerRecordList(&Located->Magisk.PointerList, NULL, Sandbox);

  Located->ID = &Protocol->Guid;
  Located->SandboxID = Sandbox->SandboxID;
  Located->Desc = Protocol;
  Located->Sandboxed = Sandboxed;
  Located->Sandboxed->RefCount += 1;
  Located->AgentHandle = AgentHandle;
  Located->ControllerHandle = ControllerHandle;
  Delegated = Located->Sandboxed->Opaque;

  ASSERT(CreateInterfaceMagisk(Protocol, (const EFI_VIRTUAL_ADDRESS)Delegated,
                               ForCore, &Located->Magisk) == EFI_SUCCESS);
}

VOID FreeLocatedSandboxInterface(IN LOCATED_INTERFACE *Located) {
  Located->Sandboxed->RefCount--;
  FreePointerRecordList(&Located->Magisk.PointerList, POINTER_SYNC_TYPE_NONE);
  FreePool(Located);
}

STATIC EFI_STATUS FindInstalledSandboxInterfaceUnique(
    IN UEFI_SANDBOX *Sandbox, IN EFI_HANDLE Handle,
    IN INTERFACE_REGISTRY_COLLECTION *Collection,
    OUT INTERFACE_REGISTRY_ENTRY **Entry) {

  LIST_ENTRY *Link;
  INTERFACE_REGISTRY_ENTRY *EntryCursor;
  LOCATED_INTERFACE *LocatedCursor;

  if (Handle) {
    BASE_LIST_FOR_EACH(Link, &Collection->EntryList) {
      EntryCursor = BASE_CR(Link, INTERFACE_REGISTRY_ENTRY, CollectionNode);
      if (Handle == EntryCursor->Sandboxed.Handle) {
        *Entry = EntryCursor;
        return EFI_SUCCESS;
      }
    }
  } else {
    BASE_LIST_FOR_EACH(Link, &Collection->EntryList) {
      EntryCursor = BASE_CR(Link, INTERFACE_REGISTRY_ENTRY, CollectionNode);
      LIST_ENTRY *LocateLink = NULL;
      BOOLEAN Found = FALSE;
      BASE_LIST_FOR_EACH(LocateLink, &EntryCursor->Sandboxed.LocatedList) {
        LocatedCursor = BASE_CR(LocateLink, LOCATED_INTERFACE, RegistryNode);
        if (LocatedCursor->SandboxID == Sandbox->SandboxID) {
          Found = TRUE;
          break;
        }
      }
      if (!Found) {
        *Entry = EntryCursor;
        return EFI_SUCCESS;
      }
    }
  }
  return EFI_NOT_FOUND;
}

STATIC CONFLICT_GROUP Groups[] = {{
    .Target = EFI_SERIAL_IO_PROTOCOL_GUID,
    .Policy = CONFLICT_GROUP_CONFLICT_SET,
    .ConflictCount = 1,
    .Conflicts =
        {
            EFI_DISK_IO_PROTOCOL_GUID,
        },
}};

STATIC BOOLEAN CheckProtocolConflict(IN UEFI_SANDBOX *Sandbox,
                                     IN CONST EFI_GUID *Target,
                                     IN CONST EFI_GUID *Candidate) {

  CONFLICT_GROUP *Group = NULL;
  for (UINTN i = 0; i < sizeof(Groups) / sizeof(CONFLICT_GROUP); i++) {
    if (CompareGuid(&Groups[i].Target, Target)) {
      Group = &Groups[i];
      break;
    }
  }

  if (!Group)
    return FALSE;

  switch (Group->Policy) {
  case CONFLICT_GROUP_CONFLICT_ANY:
    return TRUE;
  case CONFLICT_GROUP_CONFLICT_NONE:
    return FALSE;
  case CONFLICT_GROUP_CONFLICT_SET:
    for (UINTN i = 0; i < Group->ConflictCount; i++) {
      if (CompareGuid(Candidate, &Group->Conflicts[i])) {
        return TRUE;
      }
    }
    return FALSE;
  default:
    return TRUE;
  }
}

STATIC EFI_STATUS ValidateProtocolAccessControl(IN UEFI_SANDBOX *Sandbox,
                                                IN EFI_GUID *RequiredGUID) {

  LIST_ENTRY *Link;

  if (Sandbox == &CoreSandbox)
    return EFI_SUCCESS;

  BASE_LIST_FOR_EACH(Link, &Sandbox->LocatedInterfaces) {
    LOCATED_INTERFACE *Located = BASE_CR(Link, LOCATED_INTERFACE, SandboxNode);
    if (!Located->Used)
      continue;
    if (CheckProtocolConflict(Sandbox, &Located->Desc->Guid, RequiredGUID)) {
      return EFI_ACCESS_DENIED;
    }
  }
  return EFI_SUCCESS;
}

EFI_STATUS LocateSandboxInterface(IN UEFI_SANDBOX *Sandbox,
                                  IN OPTIONAL EFI_HANDLE Handle,
                                  IN EFI_GUID *ProtocolID, IN BOOLEAN ForCore,
                                  OUT LOCATED_INTERFACE **LocatedInterface) {
  LIST_ENTRY *Link;
  LOCATED_INTERFACE *Located = NULL, *LocCursor = NULL;
  INTERFACE_REGISTRY_COLLECTION *Collection = NULL, *CollectionCursor = NULL;
  INTERFACE_REGISTRY_ENTRY *Entry = NULL;
  VOID *Opaque;
  EFI_STATUS Status;
  /* Try to find in Sandbox's UsedInterfaces list */

  Status = ValidateProtocolAccessControl(Sandbox, ProtocolID);

  if (EFI_ERROR(Status))
    return Status;

  BASE_LIST_FOR_EACH(Link, &Sandbox->LocatedInterfaces) {

    LocCursor = BASE_CR(Link, LOCATED_INTERFACE, SandboxNode);

    if (CompareGuid(LocCursor->ID, ProtocolID)) {

      if (Handle && LocCursor->Sandboxed->Handle != Handle)
        continue;

      if (LocCursor->Used)
        continue;

      LocCursor->Used = TRUE;
      *LocatedInterface = LocCursor;
      return EFI_SUCCESS;
    }
  }

repeat:
  BASE_LIST_FOR_EACH(Link, &mInstalledInterfaceCollectionRegistry) {
    CollectionCursor = BASE_CR(Link, INTERFACE_REGISTRY_COLLECTION, ListNode);
    if (CompareGuid(&CollectionCursor->ID, ProtocolID)) {
      Collection = CollectionCursor;
      break;
    }
  }

  Located = AllocatePool(sizeof(*Located));

  if (Collection) {
    FindInstalledSandboxInterfaceUnique(Sandbox, Handle, Collection, &Entry);

    if (Entry)
      InitLocatedSandboxInterface(Sandbox, Located, ProtocolID,
                                  &Entry->Sandboxed, NULL, NULL, 0, ForCore);
  }

  if (!Entry) {

    if (Handle == NULL) {
      Status = gBS->LocateProtocol(ProtocolID, NULL, (VOID **)&Opaque);

      if (EFI_ERROR(Status))
        return Status;

    } else {

      Status = gBS->HandleProtocol(Handle, ProtocolID, (VOID **)&Opaque);

      if (EFI_ERROR(Status))
        return Status;
    }

    InstallSandboxInterface(&CoreSandbox, ProtocolID, Handle, (VOID *)Opaque);

    goto repeat;
  }

  Located->Used = TRUE;
  Located->Sandboxed->OpenCount += 1;
  *LocatedInterface = Located;

  InsertTailList(&Sandbox->LocatedInterfaces, &Located->SandboxNode);
  InsertTailList(&Located->Sandboxed->LocatedList, &Located->RegistryNode);
  return EFI_SUCCESS;
}

EFI_STATUS OpenSandboxInterface(IN UEFI_SANDBOX *Sandbox, IN EFI_HANDLE Handle,
                                IN EFI_GUID *ProtocolID,
                                IN EFI_HANDLE AgentHandle,
                                IN EFI_HANDLE ControllerHandle,
                                IN UINT32 Attributes,
                                OUT LOCATED_INTERFACE **LocatedInterface) {

  INTERFACE_REGISTRY_COLLECTION *Collection = NULL, *CollectionCursor = NULL;
  INTERFACE_REGISTRY_ENTRY *Entry = NULL;
  LOCATED_INTERFACE *Located = NULL, *LocCursor = NULL;
  EFI_STATUS Status;
  LIST_ENTRY *Link;
  VOID *Opaque;
  /* Try to find in Sandbox's UsedInterfaces list */

  Status = ValidateProtocolAccessControl(Sandbox, ProtocolID);

  if (EFI_ERROR(Status))
    return Status;

  BASE_LIST_FOR_EACH(Link, &Sandbox->LocatedInterfaces) {
    LocCursor = BASE_CR(Link, LOCATED_INTERFACE, SandboxNode);
    if (CompareGuid(LocCursor->ID, ProtocolID) &&
        Handle == LocCursor->Sandboxed->Handle) {

      if (LocCursor->Used)
        continue;

      if (Attributes & EFI_OPEN_PROTOCOL_EXCLUSIVE) {
        if (LocCursor->Sandboxed->OpenCount != 0) {
          return EFI_ACCESS_DENIED;
        }
      }

      if (Attributes == EFI_OPEN_PROTOCOL_TEST_PROTOCOL)
        return EFI_SUCCESS;

      LocCursor->Used = TRUE;
      *LocatedInterface = LocCursor;
      return EFI_SUCCESS;
    }
  }

repeat:
  BASE_LIST_FOR_EACH(Link, &mInstalledInterfaceCollectionRegistry) {
    CollectionCursor = BASE_CR(Link, INTERFACE_REGISTRY_COLLECTION, ListNode);
    if (CompareGuid(&CollectionCursor->ID, ProtocolID)) {
      Collection = CollectionCursor;
      break;
    }
  }

  Located = AllocatePool(sizeof(*Located));

  if (Collection) {
    FindInstalledSandboxInterfaceUnique(Sandbox, Handle, Collection, &Entry);
    if (Entry) {
      if (Attributes == EFI_OPEN_PROTOCOL_TEST_PROTOCOL)
        return EFI_SUCCESS;
      InitLocatedSandboxInterface(Sandbox, Located, ProtocolID,
                                  &Entry->Sandboxed, AgentHandle,
                                  ControllerHandle, Attributes, FALSE);
    }
  }

  if (!Entry) {

    Status = gBS->OpenProtocol(Handle, ProtocolID, &Opaque, AgentHandle,
                               ControllerHandle, Attributes);
    if (EFI_ERROR(Status))
      return Status;

    if (Attributes == EFI_OPEN_PROTOCOL_TEST_PROTOCOL)
      return Status;

    InstallSandboxInterface(&CoreSandbox, ProtocolID, Handle, Opaque);

    goto repeat;
  }

  if (Attributes & EFI_OPEN_PROTOCOL_EXCLUSIVE) {
    if (Entry->Sandboxed.OpenCount != 0) {
      return EFI_ACCESS_DENIED;
    }
  }

  Located->Used = TRUE;
  Located->Sandboxed->OpenCount += 1;
  *LocatedInterface = Located;

  InsertTailList(&Sandbox->LocatedInterfaces, &Located->SandboxNode);

  InsertTailList(&Located->Sandboxed->LocatedList, &Located->RegistryNode);
  return EFI_SUCCESS;
}

EFI_STATUS
CloseSandboxInterface(IN UEFI_SANDBOX *Sandbox, IN EFI_HANDLE Handle,
                      IN EFI_GUID *ProtocolID, IN EFI_HANDLE AgentHandle,
                      IN EFI_HANDLE ControllerHandle) {

  LOCATED_INTERFACE *Located;
  EFI_STATUS Status;
  LIST_ENTRY *Link;
  VOID *Opaque;

  BASE_LIST_FOR_EACH(Link, &Sandbox->LocatedInterfaces) {
    Located = BASE_CR(Link, LOCATED_INTERFACE, SandboxNode);

    if (Located->ID == ProtocolID && Located->Sandboxed->Handle == Handle) {
      if (!(Located->Attributes & EFI_OPEN_PROTOCOL_BY_DRIVER)) {
        Located->Sandboxed->OpenCount -= 1;
        Located->Used = FALSE;
        return EFI_SUCCESS;
      }
      break;
    }
  }

  Opaque = Located->Sandboxed->Opaque;
  Status = gBS->CloseProtocol(Handle, Opaque, AgentHandle, ControllerHandle);

  if (EFI_ERROR(Status))
    return Status;

  Located->Sandboxed->OpenCount -= 1;
  Located->Sandboxed->RefCount -= 1;

  FreeSandboxInterfaceEntry(
      BASE_CR(Located->Sandboxed, INTERFACE_REGISTRY_ENTRY, Sandboxed));
  RemoveEntryList(&Located->SandboxNode);
  RemoveEntryList(&Located->RegistryNode);
  FreePool(Located);

  return EFI_SUCCESS;
}

VOID FreeSandboxInterfaces(IN UEFI_SANDBOX *Sandbox) {
  LIST_ENTRY *Link, *Next;
  INTERFACE_REGISTRY_ENTRY *Entry;
  LOCATED_INTERFACE *Located;

  BASE_LIST_FOR_EACH_SAFE(Link, Next, &Sandbox->LocatedInterfaces) {
    Located = BASE_CR(Link, LOCATED_INTERFACE, SandboxNode);
    Located->Sandboxed->RefCount--;
    RemoveEntryList(&Located->SandboxNode);
    RemoveEntryList(&Located->RegistryNode);
    FreeLocatedSandboxInterface(Located);
  }

  BASE_LIST_FOR_EACH_SAFE(Link, Next, &Sandbox->InstalledInterfaces) {
    Entry = BASE_CR(Link, INTERFACE_REGISTRY_ENTRY, CollectionNode);
    FreeSandboxInterfaceEntry(Entry);
  }
}
