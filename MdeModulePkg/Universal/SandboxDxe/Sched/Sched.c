#include <Library/UefiBootServicesTableLib.h>

#include <Memory/Memory.h>
#include <SandboxDxe.h>
#include <Sched/Sched.h>
#include <Utils/Logger.h>

UEFI_SANDBOX *ScheduleToSandboxInternal(UEFI_SANDBOX *Sandbox,
                                        BOOLEAN TplRaise) {
  UEFI_SANDBOX *PrevSandbox;
  EFI_TPL Tpl;

  if (CurrentSandbox == Sandbox)
    return Sandbox;

  if(TplRaise)
    Tpl = gBS->RaiseTPL(TPL_HIGH_LEVEL);
  else
    DisableInterrupts();

  PrevSandbox = CurrentSandbox;
  CurrentSandbox = Sandbox;

  SetPageTable((VOID *)Sandbox->TranslationTable);

  if(TplRaise)
    gBS->RestoreTPL(Tpl);
  else
    EnableInterrupts();

  return PrevSandbox;
}
