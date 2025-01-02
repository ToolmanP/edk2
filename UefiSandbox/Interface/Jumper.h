#ifndef SANDBOX_CALL_INTERFACE_H_
#define SANDBOX_CALL_INTERFACE_H_

#include "UefiSandbox.h"
#include "Base.h"
#include "Interface.h"
#include "Uefi/UefiBaseType.h"

typedef InterfaceContext INTERFACE_CONTEXT;

EFI_STATUS JumpToSandboxFunc(IN UEFI_SANDBOX *CalleeSandbox,
                             IN CONST LOCATED_INTERFACE *Located, IN CONST UINT64 *Params,
                             IN CONST UINT64 Offset);
EFI_STATUS JumpToCoreFunc(IN CONST LOCATED_INTERFACE *Located,
                          IN CONST UINT64 *Params, IN CONST UINT64 Offset);

#endif
