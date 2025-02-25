#ifndef __BINARY_GEN_H__
#define __BINARY_GEN_H__

#include <SandboxDxe.h>

VOID *CreateInterfaceEntryPointTrampoline(IN UEFI_SANDBOX *Sandbox,
                                            IN CONST UINT64 Located,
                                            IN CONST UINT64 Offset,
                                            IN BOOLEAN ForCore);

VOID *CreateSandboxReturnTrampoline(IN UEFI_SANDBOX *Sandbox,
                                      IN CONST UINT64 JumpContext);
#endif
