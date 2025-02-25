#include <Library/UefiBootServicesTableLib.h>

#include <Memory/Memory.h>
#include <SandboxDxe.h>
#include <Sched/Sched.h>
#include <Utils/Logger.h>

UEFI_SANDBOX *ScheduleToSandboxInternal(UEFI_SANDBOX *Sandbox,
                                        BOOLEAN TplRaise) {
  EFI_TPL Tpl;
  UEFI_SANDBOX *PrevSandbox;

  if (CurrentSandbox == Sandbox)
    return Sandbox;

  if (TplRaise) {
    Tpl = gBS->RaiseTPL(TPL_HIGH_LEVEL);
  }

  PrevSandbox = CurrentSandbox;
  CurrentSandbox = Sandbox;

  SetPageTable((VOID *)Sandbox->TranslationTable);

  if (TplRaise) {
    gBS->RestoreTPL(Tpl); // what would happen if event is notified here.?
  }

  return PrevSandbox;
}
