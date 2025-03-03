#include "Workaround/Callback.h"
#include "Workaround/Name.h"
#include <Interface/Workaround.h>

STATIC CONST PARAM_WORKAROUND *mWorkarounds[] = {
    &mStringWorkaround, &mCallbackWorkaround, &mContextWorkaround
};

EFI_STATUS ApplyAvailableParamCopyWorkaround(
    IN DUPLICATE_CTX *Ctx, IN CONST REFLECT_FUNC_TYPE *Function,
    IN CONST REFLECT_PARAM *Param, IN CONST UINT64 *Src, OUT UINT64 *Dst,
    UINTN Index

) {

  for (UINTN i = 0; i < sizeof(mWorkarounds) / sizeof(UINTN); i++) {
    if (mWorkarounds[i]->Check(Function, Param) &&
        mWorkarounds[i]->Copy != NULL) {

      return mWorkarounds[i]->Copy(Ctx, Function, Param, Src, Dst, Index);
    }
  }
  return EFI_NOT_FOUND;
}
