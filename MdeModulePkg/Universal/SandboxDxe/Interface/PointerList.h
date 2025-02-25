#ifndef __POINTER_LIST_H__
#define __POINTER_LIST_H__

#include <Interface/Reflect.h>
#include <SandboxDxe.h>

/*Record the allocated summa */
typedef struct {
  LIST_ENTRY NextLink;
  CONST REFLECT_TYPE *Type;
  EFI_PHYSICAL_ADDRESS Src; // Should not be freed;
  EFI_PHYSICAL_ADDRESS Dst; // Should be freed Always;
  UINT64 Site;              // The Site of the Magisk to Modify
  UINT64 Size;              // The size to be copied;
  BOOLEAN Syncable;         // Should We sync the memory from Dst to Src ?;

} POINTER_LIST_ENTRY __attribute__((aligned(8)));

typedef struct {
  LIST_ENTRY Head;
  UEFI_SANDBOX *SrcSandbox;
  UEFI_SANDBOX *DstSandbox;
} POINTER_LIST;

typedef enum {
  POINTER_SYNC_TYPE_NONE,
  POINTER_SYNC_SRC_TO_DST,
  POINTER_SYNC_DST_TO_SRC
} POINTER_SYNC_TYPE;

VOID InitPointerRecordList(IN POINTER_LIST *PointerList,
                           IN UEFI_SANDBOX *SrcSandbox,
                           IN UEFI_SANDBOX *DstSandbox);
EFI_STATUS InsertPointerRecordList(IN POINTER_LIST *PointerList,
                                   IN CONST REFLECT_TYPE *Type,
                                   IN CONST EFI_VIRTUAL_ADDRESS Src,
                                   IN CONST EFI_VIRTUAL_ADDRESS Dst,
                                   IN CONST UINT64 Site, IN CONST UINTN Size,
                                   IN CONST BOOLEAN Syncable);

VOID FreePointerRecordList(IN POINTER_LIST *PointerList,
                           IN POINTER_SYNC_TYPE SyncType);
VOID SyncPointerRecordList(IN POINTER_LIST *PointerList,
                           IN POINTER_SYNC_TYPE SyncType);
VOID PointerRecordSiteToPhys(IN POINTER_LIST *PointerList);

#endif
