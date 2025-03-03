#include <Library/BaseLib.h>

#include <BinaryGen/BinaryGen.h>
#include <Interface/Workaround.h>
#include <Memory/Memory.h>

STATIC BOOLEAN CheckContext(IN CONST REFLECT_FUNC_TYPE *Func,
                            IN CONST REFLECT_PARAM *Param) {

  return AsciiStrStr(Param->ParamName, "Context") != NULL;
}

STATIC EFI_STATUS CopyContext(IN DUPLICATE_CTX *Ctx,
                              IN CONST REFLECT_FUNC_TYPE *Function,
                              IN CONST REFLECT_PARAM *Param,
                              IN CONST UINT64 *Src, OUT UINT64 *Dst,
                              UINTN Index) {

  Dst[Index] = Src[Index];
  return EFI_SUCCESS;
}

STATIC BOOLEAN CheckCallback(IN CONST REFLECT_FUNC_TYPE *Func,
                             IN CONST REFLECT_PARAM *Param) {
  return AsciiStrStr(Param->ParamName, "CallBack") != NULL;
}

STATIC EFI_STATUS CopyCallback(IN DUPLICATE_CTX *Ctx,
                               IN CONST REFLECT_FUNC_TYPE *Function,
                               IN CONST REFLECT_PARAM *Param,
                               IN CONST UINT64 *Src, OUT UINT64 *Dst,
                               UINTN Index) {

  Dst[Index] = TO_VIRT_ADDR((EFI_PHYSICAL_ADDRESS)CreateCallbackTrampoline(
      Ctx->PointerList->SrcSandbox, (UINTN)Param->ParamType->Function,
      Ctx->PointerList->DstSandbox->SandboxID, Src[Index]));
  return InsertPointerRecordList(Ctx->PointerList, Param->ParamType, Src[Index],
                                 Dst[Index], (UINTN)&Src[Index], sizeof(UINTN),
                                 FALSE);
}

CONST PARAM_WORKAROUND mContextWorkaround = {
    .Copy = CopyContext,
    .Check = CheckContext,
    .Sync = NULL,
};

CONST PARAM_WORKAROUND mCallbackWorkaround = {
    .Copy = CopyCallback,
    .Check = CheckCallback,
    .Sync = NULL,
};
