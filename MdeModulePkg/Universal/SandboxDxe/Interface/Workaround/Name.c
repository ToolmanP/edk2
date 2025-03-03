#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>

#include <Interface/Duplicate.h>
#include <Interface/Workaround.h>
#include <Memory/Malloc.h>
#include <Memory/Memory.h>
#include <Utils/Logger.h>

STATIC EFI_STATUS CopyStringName(IN DUPLICATE_CTX *Ctx,
                                 IN CONST REFLECT_FUNC_TYPE *Function,
                                 IN CONST REFLECT_PARAM *Param,
                                 IN CONST UINT64 *Src, OUT UINT64 *Dst,
                                 IN CONST UINT64 Index) {

  UINTN MemSize;
  BOOLEAN Syncable;
  ASSERT(Param->ParamType->Kind == BasicTypeKind);
  if (AsciiStrCmp(Param->ParamType->BasicType->TypeName, "CHAR16") == 0) {
    MemSize = StrSize((CHAR16 *)TO_PHYS_ADDR(Src[Index]));
    Dst[Index] = TO_VIRT_ADDR((EFI_PHYSICAL_ADDRESS)AllocateSandboxMemory(
        Ctx->PointerList->DstSandbox, MemSize));
    CopyMem((VOID *)TO_PHYS_ADDR(Dst[Index]), (VOID *)Src[Index], MemSize);

  } else if (AsciiStrCmp(Param->ParamType->BasicType->TypeName, "CHAR8") == 0) {

    MemSize = AsciiStrSize((CHAR8 *)TO_PHYS_ADDR(Src[Index]));
    Dst[Index] = TO_VIRT_ADDR((EFI_PHYSICAL_ADDRESS)AllocateSandboxMemory(
        Ctx->PointerList->DstSandbox, MemSize));
    CopyMem((VOID *)TO_PHYS_ADDR(Dst[Index]), (VOID *)Src[Index], MemSize);

  } else {
    __unimplemented("Str Type: %a\n", Param->ParamType->BasicType->TypeName);
  }
  Syncable = Ctx->Syncable;
  return InsertPointerRecordList(Ctx->PointerList, Param->ParamType, Src[Index],
                                 Dst[Index], (UINT64)&Dst[Index], MemSize,
                                 Syncable);
}

STATIC BOOLEAN CheckStringName(IN CONST REFLECT_FUNC_TYPE *Func,
                               IN CONST REFLECT_PARAM *Param) {
  return AsciiStrStr(Param->ParamName, "Str") ||
         AsciiStrStr(Param->ParamName, "Name");
}

CONST PARAM_WORKAROUND mStringWorkaround = {
    .Copy = CopyStringName,
    .Check = CheckStringName,
    .Sync = NULL,
};
