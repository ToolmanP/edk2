#ifndef SANDBOX_SYSTEM_TABLE_H_
#define SANDBOX_SYSTEM_TABLE_H_

#include <SandboxDxe.h>

EFI_STATUS
InitAndMapSandboxSystemTable(IN OUT UEFI_SANDBOX *Sandbox,
                             IN EFI_SYSTEM_TABLE *srcST);

VOID FreeSandboxSystemTable(IN UEFI_SANDBOX *Sandbox);

VOID SetSystemTableProtocolInterface(IN UEFI_SANDBOX *Sandbox,
                                     IN EFI_SYSTEM_TABLE *DstST,
                                     IN EFI_SYSTEM_TABLE *SrcST);
#endif
