#ifndef __WORKAROUND_H__
#define __WORKAROUND_H__

#include <Interface/Duplicate.h>
#include <Interface/Reflect.h>

#include "Workaround/Virtio.h"

typedef struct {
  BOOLEAN (*Check)(IN CONST REFLECT_FUNC_TYPE *Func, IN CONST REFLECT_PARAM *Param);
  EFI_STATUS (*Copy)(IN DUPLICATE_CTX *Ctx,
                     IN CONST REFLECT_FUNC_TYPE *Function,
                     IN CONST REFLECT_PARAM *Param, IN CONST UINT64 *Src,
                     OUT UINT64 *Dst, UINTN Index);
  EFI_STATUS (*Sync)(IN DUPLICATE_CTX *Ctx,
                     IN CONST REFLECT_FUNC_TYPE *Function, IN CONST UINT64 *Dst,
                     OUT UINT64 *Src, UINTN Index);
} PARAM_WORKAROUND;

typedef struct {
  CHAR8 *Name;
  CONST PARAM_WORKAROUND *Params;
} FUNC_WORKAROUND;

EFI_STATUS ApplyAvailableParamCopyWorkaround(
    IN DUPLICATE_CTX *Ctx, IN CONST REFLECT_FUNC_TYPE *Function,
    IN CONST REFLECT_PARAM *Param, IN CONST UINT64 *Src, OUT UINT64 *Dst,
    UINTN Index
);

#endif
