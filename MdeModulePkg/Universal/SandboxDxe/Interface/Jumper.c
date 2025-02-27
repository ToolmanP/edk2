#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>

#include <BinaryGen/BinaryGen.h>
#include <Exception/Exception.h>
#include <Interface/Interface.h>
#include <Interface/Jumper.h>
#include <Memory/Malloc.h>
#include <Memory/Memory.h>
#include <SandboxDxe.h>
#include <Sched/Sched.h>
#include <Utils/Logger.h>

extern EFI_STATUS CallCoreFunc(IN CONST UINT64 *Registers,
                               IN CONST EFI_PHYSICAL_ADDRESS Func);

extern EFI_STATUS CallSandboxFunc(IN CONST INTERFACE_PARAMS *Params);

__attribute__((unused)) STATIC VOID
DumpInterfaceContext(INTERFACE_CONTEXT *Context) {
  SBPrint("InterfaceContext: %p\n", Context);
  SBPrint("JumpBuffer: %p\n", Context->JumpBuffer);
  SBPrint("JumpContext: %p\n", Context->JumpContext);
  SBPrint("StackBase: %p\n", Context->StackBase);
  SBPrint("ReturnTrampoline: %p\n", Context->ReturnTrampoline);
  for (INTN i = 0; i < 8; i++) {
    SBPrint("Param[%ld] = 0x%lx\n", i, ((UINTN *)&Context->EntryParams)[i]);
  }

#if defined(__aarch64__)
  SBPrint("LR: %p\n", Context->EntryParams.LR);
  SBPrint("SP: %p\n", Context->EntryParams.SP);
#endif
}

EFI_STATUS JumpToCoreFunc(IN CONST LOCATED_INTERFACE *Located,
                          IN CONST UINT64 *Params, IN CONST UINT64 Offset) {
  EFI_STATUS Status;
  UEFI_SANDBOX *CallerSandbox;
  CallerSandbox = ScheduleToSandboxInternal(&CoreSandbox, TRUE);

  Status = CallCoreFunc(
      Params,
      *(EFI_PHYSICAL_ADDRESS *)(TO_PHYS_ADDR((EFI_VIRTUAL_ADDRESS)
                                                 Located->Sandboxed->Opaque) +
                                Offset));
  ScheduleToSandboxInternal(CallerSandbox, TRUE);
  return Status;
}

/*
 * CallSandboxInterfaceFunc calls functions implemented in sandbox drivers.
 * Enter the driver to execute the function, then return the result to the
 * caller.
 */
EFI_STATUS JumpToSandboxFunc(IN UEFI_SANDBOX *CalleeSandbox,
                             IN CONST LOCATED_INTERFACE *Located,
                             IN CONST UINT64 *Params, IN CONST UINT64 Offset) {

  INTERFACE_CONTEXT Context;
  UINTN SetJumpFlag;

  Context.CallerSandbox = CurrentSandbox;
  Context.CalleeSandbox = CalleeSandbox;

  if (CalleeSandbox == NULL)
    __unreachable("Invalid sandbox\n");

  /* Allocate stack */
  struct SandboxPages *sbPages = AllocateSandboxPages(
      Context.CalleeSandbox, AllocateAnyPages, EfiBootServicesData,
      DEFAULT_STACK_SIZE / PAGE_SIZE, &Context.StackBase);

  Context.JumpBuffer = AllocatePool(sizeof(BASE_LIBRARY_JUMP_BUFFER) +
                                    BASE_LIBRARY_JUMP_BUFFER_ALIGNMENT);
  Context.JumpContext =
      ALIGN_POINTER(Context.JumpBuffer, BASE_LIBRARY_JUMP_BUFFER_ALIGNMENT);
  Context.ReturnTrampoline = CreateSandboxReturnTrampoline(
      Context.CalleeSandbox, (UINT64)Context.JumpContext);

  if (Context.StackBase == 0)
    __unreachable("Failed to allocate stack for sandbox function\n");

  SetJumpFlag = SetJump(Context.JumpContext);
  if (SetJumpFlag == 0) {
#if defined(__x86_64__)
    // At most 6 parameters can be passed through registers,
    // we don't consider more than 6 parameters here.
    CopyMem(&Context.EntryParams, Params, 6 * sizeof(UINT64));
    Context.EntryParams.RCX = *(UINTN *)(Located->Sandboxed->Opaque + Offset);
    Context.EntryParams.RDX =
        TO_VIRT_ADDR((EFI_PHYSICAL_ADDRESS)(Context.ReturnTrampoline));
    Context.EntryParams.RSP =
        TO_VIRT_ADDR(Context.StackBase + DEFAULT_STACK_SIZE);
#elif defined(__aarch64__)
    CopyMem(&Context.EntryParams, Params, 8 * sizeof(UINT64));
    Context.EntryParams.LR =
        TO_VIRT_ADDR((EFI_PHYSICAL_ADDRESS)(Context.ReturnTrampoline));
    Context.EntryParams.ELR = *(UINTN *)(Located->Sandboxed->Opaque + Offset);
    Context.EntryParams.SPSR = SPSR_EL1_USER;
    Context.EntryParams.SP =
        TO_VIRT_ADDR(Context.StackBase + DEFAULT_STACK_SIZE);
#endif
    ScheduleToSandboxInternal(Context.CalleeSandbox, TRUE);
    CallSandboxFunc(&Context.EntryParams);
    __unreachable("Should not reach here\n");
  }

  ScheduleToSandboxInternal(Context.CallerSandbox, TRUE);
  FreeSandboxPages(Context.CalleeSandbox, Context.StackBase,
                   DEFAULT_STACK_SIZE / PAGE_SIZE, sbPages);
  FreePool(Context.JumpBuffer);
  FreeSandboxPool(Context.CalleeSandbox,
                  (EFI_PHYSICAL_ADDRESS)Context.ReturnTrampoline);
  return SetJumpFlag - 1;
}
