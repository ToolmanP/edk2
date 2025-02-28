#ifndef __INTERFACE_DUPLICATOR_H__
#define __INTERFACE_DUPLICATOR_H__

#include <Interface/PointerList.h>
#include <Interface/Reflect.h>
#include <SandboxDxe.h>

typedef struct _DuplicateCtx {
  POINTER_LIST *PointerList;
  CONST REFLECT_TYPE *CurrentType;
  BOOLEAN InUnion;
  BOOLEAN Optional;
  BOOLEAN Syncable;
} DUPLICATE_CTX;

EFI_STATUS EagerDuplicateCustomType(IN DUPLICATE_CTX *Ctx,
                                    IN CONST EFI_VIRTUAL_ADDRESS VirtTypeBase);
EFI_STATUS EagerDuplicateCustomTypeField(
    IN DUPLICATE_CTX *Ctx, IN CONST EFI_VIRTUAL_ADDRESS VirtTypeBase,
    IN CONST EFI_VIRTUAL_ADDRESS Offset, IN CONST UINTN TypeArraySize,
    IN CONST UINTN SpeculatedArraySize, IN CONST UINTN PointerLevel);

EFI_STATUS EagerDuplicateTypeMultiPointer(IN DUPLICATE_CTX *Ctx,
                                          IN CONST EFI_VIRTUAL_ADDRESS Src,
                                          OUT EFI_VIRTUAL_ADDRESS *Dst,
                                          IN CONST UINTN PointerLevels);
EFI_STATUS CopyCalloutParams(IN DUPLICATE_CTX *Ctx, IN CONST VOID *Opaque,
                                   IN CONST REFLECT_FUNC_TYPE *Function,
                                   IN CONST UINT64 *Src, OUT UINT64 *Dst);

VOID SyncCalloutParams(IN UEFI_SANDBOX *CallerSandbox,
                             IN UEFI_SANDBOX *CalleeSandbox,
                             IN CONST REFLECT_FUNC_TYPE *Function,
                             IN CONST UINT64 *Dst, OUT UINT64 *Src);

UINT64 SpeculateTypeFieldArraySize(IN CONST EFI_VIRTUAL_ADDRESS VirtTypeBase,
                                   IN CONST REFLECT_TYPE *Type,
                                   CONST CHAR8 *FieldName);
UINT64
SpeculateProtocolFieldArraySize(IN CONST EFI_VIRTUAL_ADDRESS VirtTypeBase,
                                IN CONST REFLECT_PROTOCOL *Protocol,
                                CONST CHAR8 *FieldName);
UINTN SpeculateFunctionParamArraySize(IN CONST UINT64 *Params,
                                      IN CONST REFLECT_FUNC_TYPE *Function,
                                      IN CONST REFLECT_PARAM *PointerParam);
#endif
