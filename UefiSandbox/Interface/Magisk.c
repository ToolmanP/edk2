#include "Magisk.h"
#include "BinaryGen/BinaryGen.h"
#include "Duplicate.h"
#include "Interface.h"
#include "Library/BaseMemoryLib.h"
#include "Library/DevicePathLib.h"
#include "Memory/Malloc.h"
#include "Memory/Memory.h"
#include "Print.h"
#include "ProcessorBind.h"

extern CONST UINT64 ENTRY_TRAMPOLINE_SIZE;

STATIC EFI_STATUS CreateInterfaceGenericMagisk(
    IN REFLECT_PROTOCOL *Protocol, IN CONST EFI_VIRTUAL_ADDRESS Delegated,
    IN BOOLEAN ForCore,
    IN OUT INTERFACE_MAGISK *Magisk) {

  EFI_PHYSICAL_ADDRESS PhysMagisk, PhysFieldDst, PhysFieldSrc, Cursor;
  EFI_VIRTUAL_ADDRESS VirtMagisk, VirtFieldDst, VirtFieldSrc;
  DUPLICATE_CTX Ctx;
  REFLECT_PROTOCOL_FIELD *Field;
  LOCATED_INTERFACE *Located;
  LIST_ENTRY *Link;
  UEFI_SANDBOX *Owner;

  Ctx = (DUPLICATE_CTX){.PointerList = &Magisk->PointerList,
                        .CurrentType = NULL,
                        .InUnion = FALSE,
                        .Syncable = TRUE};

  Located = BASE_CR(Magisk, LOCATED_INTERFACE, Magisk);
  Owner = Magisk->PointerList.Owner;
  PhysMagisk = (EFI_PHYSICAL_ADDRESS)AllocateSandboxMemory(
      Owner, Protocol->ProtocolSize);
  VirtMagisk = TO_VIRT_ADDR(PhysMagisk);
  CopyMem((VOID *)PhysMagisk,
          (VOID *)TO_PHYS_ADDR((EFI_VIRTUAL_ADDRESS)Delegated),
          Protocol->ProtocolSize);

  BASE_LIST_FOR_EACH(Link, &Protocol->FieldsList) {
    Field = BASE_CR(Link, REFLECT_PROTOCOL_FIELD, ProtocolFieldNode);

    Cursor = PhysMagisk + Field->Offset;

    VirtFieldSrc = *(EFI_VIRTUAL_ADDRESS *)(Cursor);

    PhysFieldSrc = TO_PHYS_ADDR(VirtFieldSrc);

    if (AsciiStrCmp(Field->Variable->VariableName, "SupportedLanguages") == 0) {
      UINTN Size = AsciiStrSize((CHAR8 *)PhysFieldSrc);
      PhysFieldDst = (EFI_PHYSICAL_ADDRESS)AllocateSandboxMemory(
          Magisk->PointerList.Owner, Size);
      CopyMem((VOID *)PhysFieldDst, (VOID *)PhysFieldSrc, Size);
      VirtFieldDst = PHYS_TO_VIRT(PhysFieldDst);
      *(EFI_VIRTUAL_ADDRESS *)Cursor = VirtFieldDst;
      InsertPointerRecordList(&Magisk->PointerList, NULL, VirtFieldSrc,
                              VirtFieldDst, Cursor, Size, FALSE);
    }

    if (Field->IsFunction) {
      PhysFieldDst = (EFI_PHYSICAL_ADDRESS)CreateInterfaceEntryPointTrampoline(
          Owner, (UINT64)Located, Field->Offset, ForCore);
      VirtFieldDst = PHYS_TO_VIRT(PhysFieldDst);
      *(EFI_VIRTUAL_ADDRESS *)Cursor = VirtFieldDst;
      InsertPointerRecordList(&Magisk->PointerList, NULL, VirtFieldSrc,
                              VirtFieldDst, Cursor, ENTRY_TRAMPOLINE_SIZE,
                              FALSE);

    } else {
      Ctx.CurrentType = Field->Variable->VariableType;
      if (Field->Variable->VariableType->Kind == CustomTypeKind) {
        if (Field->Variable->ArraySize != 0)
          EagerDuplicateCustomTypeField(&Ctx, VirtMagisk, Field->Offset,
                                        Field->Variable->ArraySize, 0,
                                        Field->Variable->PointerLevel);
        else
          EagerDuplicateCustomTypeField(
              &Ctx, VirtMagisk, Field->Offset, 0,
              SpeculateProtocolFieldArraySize(VirtMagisk, Protocol,
                                              Field->Variable->VariableName),
              Field->Variable->PointerLevel);
      }
    }
  }
  Magisk->Interface = (VOID *)VirtMagisk;
  return EFI_SUCCESS;
}

EFI_STATUS CreateInterfaceMagisk(IN REFLECT_PROTOCOL *Protocol,
                                 IN CONST EFI_VIRTUAL_ADDRESS Delegated,
                                 IN BOOLEAN ForCore,
                                 IN OUT INTERFACE_MAGISK *Magisk) {

  UINTN Size;
  if (CompareGuid(&Protocol->Guid, &gEfiDevicePathProtocolGuid)) {
    Size = GetDevicePathSize((VOID *)VIRT_TO_PHYS(Delegated));
    Magisk->Interface = (VOID *)PHYS_TO_VIRT(
        AllocateSandboxMemory(Magisk->PointerList.Owner, Size));
    CopyMem((VOID *)VIRT_TO_PHYS(Magisk->Interface),
            (VOID *)VIRT_TO_PHYS(Delegated), Size);
    return EFI_SUCCESS;
  }
  return CreateInterfaceGenericMagisk(Protocol, Delegated, ForCore, Magisk);
}

EFI_STATUS FreeInterfaceMagisk(IN CONST EFI_GUID *ProtocolID,
                               IN OUT INTERFACE_MAGISK *Magisk) {

  FreeSandboxPool(Magisk->PointerList.Owner,
                  VIRT_TO_PHYS((EFI_VIRTUAL_ADDRESS)(Magisk->Interface)));

  if (CompareGuid(ProtocolID, &gEfiDevicePathProtocolGuid)) {
    return EFI_SUCCESS;
  }

  FreePointerRecordList(&Magisk->PointerList, FALSE);
  return EFI_SUCCESS;
}

EFI_STATUS SyncInterfaceMagisk(IN INTERFACE_MAGISK *Magisk) {
  LIST_ENTRY *Link;
  LOCATED_INTERFACE *LocatedCursor, *Located;
  Located = BASE_CR(Magisk, LOCATED_INTERFACE, Magisk);
  SyncPointerRecordList(&Located->Magisk.PointerList, POINTER_SYNC_DST_TO_SRC);
  BASE_LIST_FOR_EACH(Link, &Located->Sandboxed->LocatedList) {
    LocatedCursor = BASE_CR(Link, LOCATED_INTERFACE, RegistryNode);
    if (LocatedCursor == Located)
      continue;
    if (LocatedCursor->Used) {
      SyncPointerRecordList(&LocatedCursor->Magisk.PointerList,
                            POINTER_SYNC_SRC_TO_DST);
    }
  }
  return EFI_SUCCESS;
}
