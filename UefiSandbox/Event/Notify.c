#include "BinaryGen/BinaryGen.h"
#include "Core/Dxe/Event/Event.h"
#include "Event.h"
#include "Exception/Exception.h"
#include "Library/BaseLib.h"
#include "Library/BaseMemoryLib.h"
#include "Library/DebugLib.h"
#include "Library/MemoryAllocationLib.h"
#include "Malloc.h"
#include "Memory.h"
#include "Print.h"
#include "Sched.h"
#include "Uefi/UefiBaseType.h"

extern EFI_STATUS CallSandboxFunc(IN CONST INTERFACE_PARAMS *Params);
LIST_ENTRY mEventRegistry = INITIALIZE_LIST_HEAD_VARIABLE(mEventRegistry);

VOID EFIAPI SandboxGenericNotifyFunction(IN EFI_EVENT Event, IN VOID *Context) {

  BASE_LIBRARY_JUMP_BUFFER *JumpContext;
  INTERFACE_PARAMS Params;
  UINTN SetJumpFlag;
  IEVENT *IEvent;
  IEvent = Event;

  VOID *JumpBuffer;
  VOID *StackBuffer;
  UINT32 *ReturnTrampoline;

  ZeroMem(&Params, sizeof(Params));
  JumpBuffer = AllocatePool(sizeof(BASE_LIBRARY_JUMP_BUFFER) +
                            BASE_LIBRARY_JUMP_BUFFER_ALIGNMENT);
  JumpContext = ALIGN_POINTER(JumpBuffer, BASE_LIBRARY_JUMP_BUFFER_ALIGNMENT);

  ReturnTrampoline = CreateSandboxReturnTrampoline(CurrentSandbox,
                                                   (UINTN)JumpContext);
  StackBuffer =
      AllocateSandboxMemory(CurrentSandbox, DEFAULT_STACK_SIZE);
  SetJumpFlag = SetJump(JumpContext);

  if (SetJumpFlag == 0) {
#if defined(__x86_64__)
    Params.R0 = (UINT64)Event;
    Params.R1 = (UINT64)Context;
    Params.RCX = (UINT64)IEvent->NotifyFunction;
    Params.RDX = (EFI_VIRTUAL_ADDRESS)PHYS_TO_VIRT(ReturnTrampoline);
    Params.RSP = (UINT64)StackBuffer + DEFAULT_STACK_SIZE;
#elif defined(__aarch64__)
    Params.X0 = (UINT64)Event;
    Params.X1 = (UINT64)Context;
    Params.LR = (EFI_VIRTUAL_ADDRESS)PHYS_TO_VIRT(ReturnTrampoline);
    Params.SP = (UINT64)StackBuffer + DEFAULT_STACK_SIZE;
    Params.SPSR = SPSR_EL1_USER;
    Params.ELR = (UINT64)IEvent->NotifyFunction;
#endif
    CallSandboxFunc(&Params);
    ASSERT(0);
  }

  FreeSandboxPool(CurrentSandbox,
                  (EFI_PHYSICAL_ADDRESS)ReturnTrampoline);
  FreePool(StackBuffer);
  FreePool(JumpBuffer);
  FreePool(Context);
}
