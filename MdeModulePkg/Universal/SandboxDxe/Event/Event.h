#ifndef __SANDBOX_EVENT_H
#define __SANDBOX_EVENT_H

#include <Core/Dxe/Event/Event.h>
#include <Library/BaseLib.h>

#include <Interface/Interface.h>
#include <SandboxDxe.h>


typedef struct _UEFI_SANDBOX_EVENT_CONTEXT {
  UEFI_SANDBOX *Sandbox;
  VOID *Payload;
  EFI_EVENT_NOTIFY NotifyFunction;
} UEFI_SANDBOX_EVENT_CONTEXT;

EFI_STATUS CreateSandboxEventContext(IN UEFI_SANDBOX *Sandbox, IN VOID *Payload,
                                     IN EFI_EVENT_NOTIFY NotifyFunction,
                                     OUT UEFI_SANDBOX_EVENT_CONTEXT **Context);
VOID EFIAPI SandboxGenericNotifyFunction(IN EFI_EVENT Event, IN VOID *Context);

#endif
