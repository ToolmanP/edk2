#include "Library/BaseMemoryLib.h"
#include "Library/UefiBootServicesTableLib.h"
#include <Decompiler/Decompiler.h>
#include <Utils/Logger.h>

#define SMC_MASK 0xD4000003
#define MRS_MASK 0xD5300000
#define MSR_MASK 0xD5100000

// TODO: Introduce Access Control Policy on following Sensitive Instructions.
//
//
//

extern VOID ProxySMC(EFI_SYSTEM_CONTEXT_AARCH64 *Context);

STATIC EFI_STATUS AArch64ResolveSMC(EFI_SYSTEM_CONTEXT_AARCH64 *Context) {
  ProxySMC(Context);
  return EFI_SUCCESS;
}

// Clear out the destination register

STATIC UINTN Mrs(UINT32 Instr) {
  Instr &= ~(0x1F << 0);
  UINT32 Template[] = {
      Instr,      // mrs x0, xxx
      0xd65f03c0  // ret
  };
  VOID *Buffer = NULL;
  gBS->AllocatePool(EfiBootServicesCode, sizeof(Template), &Buffer);
  CopyMem(Buffer, Template, sizeof(Template));
  UINT32 Result = ((UINTN(*)())Buffer)();
  gBS->FreePool(Buffer);
  return Result;
}

STATIC VOID Msr(UINT32 Instr, UINTN Source) {
  Instr &= ~(0x1F << 0);
  UINT32 Template[] = {
      Instr,      // msr xxx, x0
      0xd65f03c0  // ret
  };
  VOID *Buffer = NULL;
  gBS->AllocatePool(EfiBootServicesCode, sizeof(Template), &Buffer);
  CopyMem(Buffer, Template, sizeof(Template));
  ((VOID(*)(UINTN))Buffer)(Source);
  gBS->FreePool(Buffer);
}

STATIC EFI_STATUS AArch64ResolveMRS(EFI_SYSTEM_CONTEXT_AARCH64 *Context) {
  UINT32 Instr = *(UINT32 *)(Context->ELR);
  ((UINTN *)Context)[Instr & 0x1F] = Mrs(Instr);
  return EFI_SUCCESS;
}

STATIC EFI_STATUS AArch64ResolveMSR(EFI_SYSTEM_CONTEXT_AARCH64 *Context) {
  UINT32 Instr = *(UINT32 *)(Context->ELR);
  Msr(Instr, ((UINTN *)Context)[(Instr & 0x1F)]);
  return EFI_SUCCESS;
}

STATIC EFI_STATUS AArch64ResolveInstruction(EFI_SYSTEM_CONTEXT Context) {
  EFI_SYSTEM_CONTEXT_AARCH64 *ContextAArch64 = Context.SystemContextAArch64;
  UINT32 Instr = *(UINT32 *)(ContextAArch64->ELR);
  EFI_STATUS Status = EFI_ACCESS_DENIED;
  if ((Instr >> 24) == (SMC_MASK >> 24) && (Instr & 0x3) == 3)
    Status =AArch64ResolveSMC(ContextAArch64);
  else if ((Instr >> 20) == (MRS_MASK >> 20))
    Status = AArch64ResolveMRS(ContextAArch64);
  else if ((Instr >> 20) == (MSR_MASK >> 20))
    Status = AArch64ResolveMSR(ContextAArch64);
  else
    SBWarn("Denied access to uninstruction instruction of 0x%lx", Instr);

  return Status;
}

DECOMPILER mDecompiler = {.Resolve = AArch64ResolveInstruction};
