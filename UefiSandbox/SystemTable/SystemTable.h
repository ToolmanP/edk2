#ifndef SANDBOX_SYSTEM_TABLE_H_
#define SANDBOX_SYSTEM_TABLE_H_

#include "Uefi/UefiSpec.h"
#include "UefiSandbox.h"

EFI_STATUS
InitAndMapSandboxSystemTable (
    IN OUT UEFI_SANDBOX *Sandbox,
    IN EFI_SYSTEM_TABLE *srcST
);

VOID
FreeSandboxSystemTable (
    IN UEFI_SANDBOX *Sandbox
);

#endif
