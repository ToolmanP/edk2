#include "PointerList.h"
#include "Library/BaseLib.h"
#include "Library/BaseMemoryLib.h"
#include "Library/MemoryAllocationLib.h"
#include "Memory/Malloc.h"
#include "Memory/Memory.h"
#include "Print.h"

VOID InitPointerRecordList(IN POINTER_LIST *PointerList,
                           IN UEFI_SANDBOX *SrcSandbox,
                           IN UEFI_SANDBOX *DstSandbox) {
  InitializeListHead(&PointerList->Head);
  PointerList->SrcSandbox = SrcSandbox;
  PointerList->DstSandbox = DstSandbox;
}

EFI_STATUS InsertPointerRecordList(IN POINTER_LIST *PointerList,
                                   IN CONST REFLECT_TYPE *Type,
                                   IN CONST EFI_VIRTUAL_ADDRESS Src,
                                   IN CONST EFI_VIRTUAL_ADDRESS Dst,
                                   IN CONST UINT64 Site, IN CONST UINTN Size,
                                   IN CONST BOOLEAN Syncable) {

  POINTER_LIST_ENTRY *Entry;

  Entry = (POINTER_LIST_ENTRY *)AllocatePool(sizeof(POINTER_LIST_ENTRY));
  Entry->Type = Type;
  Entry->Size = Size;
  Entry->Dst = Dst;
  Entry->Src = Src;
  Entry->Site = Site;
  Entry->Syncable = Syncable;
  InsertHeadList(&PointerList->Head, &Entry->NextLink);

  if (PointerList->SrcSandbox != NULL && PointerList->SrcSandbox != &CoreSandbox)
    return ValidateVMRegion(PointerList->SrcSandbox, Src, Size);
  else
    return EFI_SUCCESS;
}

STATIC VOID ShallowCopy(IN CONST REFLECT_TYPE *Type, IN EFI_VIRTUAL_ADDRESS Src,
                        IN EFI_VIRTUAL_ADDRESS Dst, IN UINTN Size) {
  LIST_ENTRY *Link;
  REFLECT_FIELD *Field;
  EFI_VIRTUAL_ADDRESS CursorSrc = Src;
  EFI_VIRTUAL_ADDRESS CursorDst = Dst;
  UINTN FieldSize;
  if (Type == NULL)
    return;
  if (Type->Kind == BasicTypeKind) {
    CopyMem((VOID *)TO_PHYS_ADDR(Dst), (VOID *)TO_PHYS_ADDR(Src), Size);
  } else if (Type->Kind == CustomTypeKind) {
    for (; CursorSrc < Src + Size && CursorDst < Dst + Size;
         CursorSrc += Type->CustomType->TypeSize,
         CursorDst += Type->CustomType->TypeSize) {
      BASE_LIST_FOR_EACH(Link, &Type->CustomType->Fields) {
        Field = BASE_CR(Link, REFLECT_FIELD, FieldNode);
        FieldSize = (Field->FieldType->Kind == BasicTypeKind)
                        ? Field->FieldType->BasicType->TypeSize
                        : Field->FieldType->CustomType->TypeSize;
        if (Field->PointerLevel == 0) {

          if (Field->ArraySize == 0)
            ShallowCopy(Field->FieldType, CursorSrc + Field->Offset,
                        CursorDst + Field->Offset, FieldSize);
          else
            ShallowCopy(Field->FieldType, CursorSrc + Field->Offset,
                        CursorDst + Field->Offset,
                        FieldSize * Field->ArraySize);
        }
      }
    }
  }
}

VOID FreePointerRecordList(IN POINTER_LIST *PointerList,
                           IN POINTER_SYNC_TYPE SyncType) {
  LIST_ENTRY *Link;
  LIST_ENTRY *Next;
  POINTER_LIST_ENTRY *Entry;
  BASE_LIST_FOR_EACH_SAFE(Link, Next, &PointerList->Head) {
    Entry = BASE_CR(Link, POINTER_LIST_ENTRY, NextLink);
    if (Entry->Syncable) {
      switch (SyncType) {
      case POINTER_SYNC_TYPE_NONE:
        break;
      case POINTER_SYNC_SRC_TO_DST:
        ShallowCopy(Entry->Type, Entry->Src, Entry->Dst, Entry->Size);
        break;
      case POINTER_SYNC_DST_TO_SRC:
        ShallowCopy(Entry->Type, Entry->Dst, Entry->Src, Entry->Size);
        break;
      }
    }
    FreeSandboxPool(PointerList->DstSandbox, TO_PHYS_ADDR(Entry->Dst));
    FreePool(Entry);
  }
}

VOID SyncPointerRecordList(IN POINTER_LIST *PointerList,
                           IN POINTER_SYNC_TYPE SyncType) {

  LIST_ENTRY *Link;
  POINTER_LIST_ENTRY *Entry;
  BASE_LIST_FOR_EACH(Link, &PointerList->Head) {
    Entry = BASE_CR(Link, POINTER_LIST_ENTRY, NextLink);
    if (Entry->Syncable) {
      switch (SyncType) {
      case POINTER_SYNC_TYPE_NONE:
        break;
      case POINTER_SYNC_SRC_TO_DST:
        ShallowCopy(Entry->Type, Entry->Src, Entry->Dst, Entry->Size);
        break;
      case POINTER_SYNC_DST_TO_SRC:
        ShallowCopy(Entry->Type, Entry->Dst, Entry->Src, Entry->Size);
        break;
      }
    }
  }
}

VOID PointerRecordSiteToPhys(IN POINTER_LIST *PointerList) {
  LIST_ENTRY *Link;
  POINTER_LIST_ENTRY *Entry;
  BASE_LIST_FOR_EACH(Link, &PointerList->Head) {
    Entry = BASE_CR(Link, POINTER_LIST_ENTRY, NextLink);
    *(EFI_PHYSICAL_ADDRESS *)(Entry->Site) =
        TO_PHYS_ADDR(*(EFI_VIRTUAL_ADDRESS *)(Entry->Site));
  }
}
