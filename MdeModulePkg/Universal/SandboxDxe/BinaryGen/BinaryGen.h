#ifndef __BINARY_GEN_H__
#define __BINARY_GEN_H__

#include <SandboxDxe.h>

VOID *CreateInterfaceEntryPointTrampoline(IN UEFI_SANDBOX *Sandbox,
                                            IN CONST UINT64 Located,
                                            IN CONST UINT64 Offset,
                                            IN BOOLEAN ForCore);

VOID *CreateSandboxReturnTrampoline(IN UEFI_SANDBOX *Sandbox,
                                      IN CONST UINT64 JumpContext);

VOID *CreateCallbackTrampoline(IN UEFI_SANDBOX *Sandbox,
                               IN CONST UINT64 ReflectFunc,
                               IN CONST UINT64 CalleeID,
                               IN CONST UINT64 FuncAddress);

extern CONST UINT64 ENTRY_TRAMPOLINE_SIZE;
extern CONST UINT64 RETURN_TRAMPOLINE_SIZE;
extern CONST UINT64 CALLBACK_TRAMPOLINE_SIZE;
#endif
