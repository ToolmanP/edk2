#include <Chipset/AArch64.h>
#include <AsmMacroIoLibV8.h>

/*
 * EFI_STATUS CallCoreInterfaceImpl(UINT64 Registers[8], UINT64 FunctionAddress)
 */
// Callee saved registers: x19-x29, x30
ASM_FUNC(CallCoreFunc)
    stp x29, x30, [sp, #-0x10]!
    stp x19, x20, [sp, #-0x10]!

    mov x19, x0
    mov x20, x1

    ldp x0, x1, [x19]
    ldp x2, x3, [x19, #0x10]
    ldp x4, x5, [x19, #0x20]
    ldp x6, x7, [x19, #0x30]

    blr x20

    ldp x19, x20, [sp], #0x10
    ldp x29, x30, [sp], #0x10
    ret


/* void CallSandboxFunc(TrampolineParams *) */
ASM_FUNC(CallSandboxFunc)
  dmb ish

  mov      x15, x0

  ldp x0, x1, [x15]
  ldp x2, x3, [x15, #0x10]
  ldp x4, x5, [x15, #0x20]
  ldp x6, x7, [x15, #0x30]


  // Pop the SYS regs we need
  ldp x12, x13, [x15, #0x40]
  ldp x14, x15, [x15, #0x50]

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
