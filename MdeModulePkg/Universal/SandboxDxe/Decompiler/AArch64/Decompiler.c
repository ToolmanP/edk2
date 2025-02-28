#include <Decompiler/Decompiler.h>
#include <Utils/Logger.h>

#define SMC_MASK 0xD4000003
#define MRS_MASK 0xD5300000
#define MSR_MASK 0xD5000000

// TODO: Introduce Access Control Policy on following Sensitive Instructions.

STATIC EFI_STATUS AArch64ResolveSMC(CONST EFI_SYSTEM_CONTEXT_AARCH64 *Context) {
  UINTN Instr = Context->ELR;
  ((VOID(*)())Instr)();
  return EFI_SUCCESS;
}

STATIC EFI_STATUS AArch64ResolveMRS(CONST EFI_SYSTEM_CONTEXT_AARCH64 *Context) {
  UINTN Instr = Context->ELR;
  ((VOID(*)())Instr)();
  return EFI_SUCCESS;
}

STATIC EFI_STATUS AArch64ResolveMSR(CONST EFI_SYSTEM_CONTEXT_AARCH64 *Context) {
  UINTN Instr = Context->ELR;
  ((VOID(*)())Instr)();
  return EFI_SUCCESS;
}

STATIC EFI_STATUS AArch64ResolveInstruction(CONST EFI_SYSTEM_CONTEXT Context) {
  EFI_SYSTEM_CONTEXT_AARCH64 *ContextAArch64 = Context.SystemContextAArch64;
  UINTN Instr = ContextAArch64->ELR;
  if (Instr & SMC_MASK)
    return AArch64ResolveSMC(ContextAArch64);
  else if (Instr & MRS_MASK)
    return AArch64ResolveMRS(ContextAArch64);
  else if (Instr & MSR_MASK)
    return AArch64ResolveMSR(ContextAArch64);
  else
    SBWarn("Denied access to uninstruction instruction of 0x%lx", Instr);
  return EFI_ACCESS_DENIED;
}

DECOMPILER mDecompiler = {.Resolve = AArch64ResolveInstruction};
