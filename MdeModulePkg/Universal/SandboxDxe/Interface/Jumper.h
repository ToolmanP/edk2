#ifndef SANDBOX_CALL_INTERFACE_H_
#define SANDBOX_CALL_INTERFACE_H_

#include <Interface/Interface.h>
#include <SandboxDxe.h>

typedef InterfaceContext INTERFACE_CONTEXT;

EFI_STATUS JumpToSandboxFunc(IN UEFI_SANDBOX *CalleeSandbox,
                             IN CONST UINT64 FuncAddress,
                             IN CONST UINT64 *Params);

EFI_STATUS JumpToCoreFunc(IN CONST UINT64 FuncAddress,
                          IN CONST UINT64 *Params);

#endif
