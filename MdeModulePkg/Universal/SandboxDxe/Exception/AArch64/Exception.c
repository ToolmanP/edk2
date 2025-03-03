#include "Library/DebugLib.h"
#include "Sched.h"
#include <Library/DefaultExceptionHandlerLib.h>
#include <Library/SandboxSyscalls.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/Cpu.h>

#include <Decompiler/Decompiler.h>
#include <Exception/Exception.h>
#include <Interface/Jumper.h>
#include <Interface/Registry.h>
#include <Memory/Memory.h>
#include <SandboxDxe.h>
#include <Utils/Logger.h>

extern const VOID *SandboxServicesManager[NR_SYSCALL];

void DoSyscall(EFI_SYSTEM_CONTEXT Context, const VOID **ServiceTable);

STATIC INT32 InstructionOrDataAbort(IN UINTN AbortType, IN UINTN Iss) {
  CHAR8 *AbortCause;

  switch (Iss & 0x3f) {
  case 0x0:
    AbortCause = "Address size fault, zeroth level of translation or "
                 "translation table base register";
    break;
  case 0x1:
    AbortCause = "Address size fault, first level";
    break;
  case 0x2:
    AbortCause = "Address size fault, second level";
    break;
  case 0x3:
    AbortCause = "Address size fault, third level";
    break;

  /*
   * Translation fault, zeroth level, maybe triggered when sandbox returns
   */
  case 0x4:
    AbortCause = "Translation fault, zeroth level";
    if (AbortType == EXCEPTION_INSTRUCTION_ABORT) {
      return EXCEPTION_SANDBOX_RETURN;
    }
    break;

  case 0x5:
    AbortCause = "Translation fault, first level";
    break;
  case 0x6:
    AbortCause = "Translation fault, second level";
    break;
  case 0x7:
    AbortCause = "Translation fault, third level";
    break;
  case 0x9:
    AbortCause = "Access flag fault, first level";
    break;
  case 0xa:
    AbortCause = "Access flag fault, second level";
    break;
  case 0xb:
    AbortCause = "Access flag fault, third level";
    break;

  /*
   * Instruction Abort, Permission faults can be caused by Protocol calls
   */
  case 0xd:
    AbortCause = "Permission fault, first level";
    return EXCEPTION_INSTRUCTION_PERMISSION_FAULT;
  case 0xe:
    AbortCause = "Permission fault, second level";
    return EXCEPTION_INSTRUCTION_PERMISSION_FAULT;
  case 0xf:
    AbortCause = "Permission fault, third level";
    return EXCEPTION_INSTRUCTION_PERMISSION_FAULT;

  case 0x10:
    AbortCause = "Synchronous external abort";
    break;
  case 0x18:
    AbortCause = "Synchronous parity error on memory access";
    break;
  case 0x11:
    AbortCause = "Asynchronous external abort";
    break;
  case 0x19:
    AbortCause = "Asynchronous parity error on memory access";
    break;
  case 0x14:
    AbortCause =
        "Synchronous external abort on translation table walk, zeroth level";
    break;
  case 0x15:
    AbortCause =
        "Synchronous external abort on translation table walk, first level";
    break;
  case 0x16:
    AbortCause =
        "Synchronous external abort on translation table walk, second level";
    break;
  case 0x17:
    AbortCause =
        "Synchronous external abort on translation table walk, third level";
    break;
  case 0x1c:
    AbortCause = "Synchronous parity error on memory access on translation "
                 "table walk, zeroth level";
    break;
  case 0x1d:
    AbortCause = "Synchronous parity error on memory access on translation "
                 "table walk, first level";
    break;
  case 0x1e:
    AbortCause = "Synchronous parity error on memory access on translation "
                 "table walk, second level";
    break;
  case 0x1f:
    AbortCause = "Synchronous parity error on memory access on translation "
                 "table walk, third level";
    break;
  case 0x21:
    AbortCause = "Alignment fault";
    break;
  case 0x22:
    AbortCause = "Debug event";
    break;
  case 0x30:
    AbortCause = "TLB conflict abort";
    break;
  case 0x33:
  case 0x34:
    AbortCause = "IMPLEMENTATION DEFINED";
    break;
  case 0x35:
  case 0x36:
    AbortCause = "Domain fault";
    break;
  default:
    AbortCause = "";
    break;
  }

  return -1;
}

STATIC INT32 ExceptionSyndrome(IN UINT32 Esr) {
  CHAR8 *Message;
  UINTN Ec;
  UINTN Iss;

  Ec = Esr >> 26;
  Iss = Esr & 0x00ffffff;

  switch (Ec) {
  case 0x0:
    return EXCEPTION_SYSTEM_ERROR;
  case 0x15:
    Message = "SVC executed in AArch64";
    return 0;
  case 0x20:
  case 0x21:
    return InstructionOrDataAbort(EXCEPTION_INSTRUCTION_ABORT, Iss);
  case 0x22:
    Message = "PC alignment fault";
    break;
  case 0x23:
    Message = "SP alignment fault";
    break;
  case 0x24:
  case 0x25:
    // TODO: Data abort caused by accessing Protocol Data Pointer
    InstructionOrDataAbort(EXCEPTION_DATA_ABORT, Iss);
    return -1;
  default:
    return -1;
  }

  return -1;
}

STATIC VOID HandleSyscall(IN EFI_SYSTEM_CONTEXT SystemContext) {
  UINT64 SyscallNumber;
  UINT64 ReturnValue;

  SyscallNumber = SystemContext.SystemContextAArch64->X8;
  if (SyscallNumber >= NR_SYSCALL) {
    SBError("Invalid syscall number: %d\n", SyscallNumber);
    ReturnValue = EFI_UNSUPPORTED;
    SystemContext.SystemContextAArch64->X0 = ReturnValue;
  }

  DoSyscall(SystemContext, SandboxServicesManager);
}

STATIC VOID HandleSyncException(IN CONST EFI_EXCEPTION_TYPE InterruptType,
                                IN OUT CONST EFI_SYSTEM_CONTEXT SystemContext) {

  UEFI_SANDBOX *PrevSandbox;
  UINTN ExceptionType;
  ExceptionType = ExceptionSyndrome(SystemContext.SystemContextAArch64->ESR);

  switch (ExceptionType) {
  case EXCEPTION_SYSTEM_CALL:
    HandleSyscall(SystemContext);
    break;
  case EXCEPTION_SYSTEM_ERROR: {
    PrevSandbox = ScheduleToSandboxInternal(&CoreSandbox, FALSE);
    if (EFI_ERROR(mDecompiler.Resolve(SystemContext)))
      DefaultExceptionHandler(InterruptType, SystemContext);
    else
      SystemContext.SystemContextAArch64->ELR += 4;
    ScheduleToSandboxInternal(PrevSandbox, FALSE);
    break;
  }
  default:
    /* Assert */
    DefaultExceptionHandler(InterruptType, SystemContext);
  }
}

EFI_STATUS
RegisterSyncExceptionHandler(BOOLEAN Unregister) {
  EFI_CPU_ARCH_PROTOCOL *Cpu;
  EFI_STATUS Status;

  // Get the CPU protocol that this driver requires.
  Status = gBS->LocateProtocol(&gEfiCpuArchProtocolGuid, NULL, (VOID **)&Cpu);
  if (EFI_ERROR(Status)) {
    return Status;
  }

  // Register to receive system calls or Unregister handler
  if (Unregister) {
    Status = Cpu->RegisterInterruptHandler(
        Cpu, EXCEPT_AARCH64_SYNCHRONOUS_EXCEPTIONS, NULL);
  } else {
    Status = Cpu->RegisterInterruptHandler(
        Cpu, EXCEPT_AARCH64_SYNCHRONOUS_EXCEPTIONS, HandleSyncException);
  }

  if (EFI_ERROR(Status)) {
    SBError("Cpu->RegisterInterruptHandler() - %r\n", Status);
  }

  return Status;
}
