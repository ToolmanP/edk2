#include <AArch64.h>
#include <AsmMacroLib.h>

/*
 * EFI_STATUS CallCoreInterfaceImpl(UINT64 Registers[8], UINT64 FunctionAddress)
 */
// Callee saved registers: x19-x29, x30
ASM_FUNC(CallCoreFunc)

    // Save the callee saved registers

    stp x29, x30, [sp, #-0x10]!
    stp x25, x26, [sp, #-0x10]!

    mov x26, x0
    mov x25, x1

    ldp x0, x1, [x26]
    ldp x2, x3, [x26, #0x10]
    ldp x4, x5, [x26, #0x20]
    ldp x6, x7, [x26, #0x30]

    blr x25

    ldp x25, x26, [sp], #0x10
    ldp x29, x30, [sp], #0x10
    ret


/* void CallSandboxFunc(TrampolineParams *) */
ASM_FUNC(CallSandboxFunc)
  dmb ish

  mov      x25, x0

  ldp x0, x1, [x25]
  ldp x2, x3, [x25, #0x10]
  ldp x4, x5, [x25, #0x20]
  ldp x6, x7, [x25, #0x30]


  // Pop the SYS regs we need
  ldp x12, x13, [x25, #0x40]
  ldp x14, x15, [x25, #0x50]

  msr fpsr, xzr

  //
  // Disable interrupt(IRQ and FIQ) before restoring context,
  // or else the context will be corrupted by interrupt reentrance.
  // Interrupt mask will be restored from spsr by hardware when we call eret
  //

  msr   daifset, #3
  isb

  msr elr_el1, x14  // Exception Link Register
  msr spsr_el1, x15 // Saved Processor Status Register 32bit

  mov x30, x12
  msr sp_el0, x13

  mov x12, xzr
  mov x13, xzr
  mov x14, xzr
  mov x15, xzr

  eret
