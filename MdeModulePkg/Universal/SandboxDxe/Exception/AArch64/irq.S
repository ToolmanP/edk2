#include <Chipset/AArch64.h>
#include <AsmMacroIoLibV8.h>

/*
  This is the stack constructed by the exception handler (low address to high address).
  X0 to FAR makes up the EFI_SYSTEM_CONTEXT for AArch64.

  UINT64  X0;     0x000
  UINT64  X1;     0x008
  UINT64  X2;     0x010
  UINT64  X3;     0x018
  UINT64  X4;     0x020
  UINT64  X5;     0x028
  UINT64  X6;     0x030
  UINT64  X7;     0x038
  UINT64  X8;     0x040
  UINT64  X9;     0x048
  UINT64  X10;    0x050
  UINT64  X11;    0x058
  UINT64  X12;    0x060
  UINT64  X13;    0x068
  UINT64  X14;    0x070
  UINT64  X15;    0x078
  UINT64  X16;    0x080
  UINT64  X17;    0x088
  UINT64  X18;    0x090
  UINT64  X19;    0x098
  UINT64  X20;    0x0a0
  UINT64  X21;    0x0a8
  UINT64  X22;    0x0b0
  UINT64  X23;    0x0b8
  UINT64  X24;    0x0c0
  UINT64  X25;    0x0c8
  UINT64  X26;    0x0d0
  UINT64  X27;    0x0d8
  UINT64  X28;    0x0e0
  UINT64  FP;     0x0e8   // x29 - Frame Pointer
  UINT64  LR;     0x0f0   // x30 - Link Register
  UINT64  SP;     0x0f8   // x31 - Stack Pointer

  // FP/SIMD Registers. 128bit if used as Q-regs.
  UINT64  V0[2];  0x100
  UINT64  V1[2];  0x110
  UINT64  V2[2];  0x120
  UINT64  V3[2];  0x130
  UINT64  V4[2];  0x140
  UINT64  V5[2];  0x150
  UINT64  V6[2];  0x160
  UINT64  V7[2];  0x170
  UINT64  V8[2];  0x180
  UINT64  V9[2];  0x190
  UINT64  V10[2]; 0x1a0
  UINT64  V11[2]; 0x1b0
  UINT64  V12[2]; 0x1c0
  UINT64  V13[2]; 0x1d0
  UINT64  V14[2]; 0x1e0
  UINT64  V15[2]; 0x1f0
  UINT64  V16[2]; 0x200
  UINT64  V17[2]; 0x210
  UINT64  V18[2]; 0x220
  UINT64  V19[2]; 0x230
  UINT64  V20[2]; 0x240
  UINT64  V21[2]; 0x250
  UINT64  V22[2]; 0x260
  UINT64  V23[2]; 0x270
  UINT64  V24[2]; 0x280
  UINT64  V25[2]; 0x290
  UINT64  V26[2]; 0x2a0
  UINT64  V27[2]; 0x2b0
  UINT64  V28[2]; 0x2c0
  UINT64  V29[2]; 0x2d0
  UINT64  V30[2]; 0x2e0
  UINT64  V31[2]; 0x2f0

  // System Context
  UINT64  ELR;    0x300   // Exception Link Register
  UINT64  SPSR;   0x308   // Saved Processor Status Register
  UINT64  FPSR;   0x310   // Floating Point Status Register
  UINT64  ESR;    0x318   // Exception syndrome register
  UINT64  FAR;    0x320   // Fault Address Register
  UINT64  Padding;0x328   // Required for stack alignment
*/

#define GP_CONTEXT_SIZE    (32 *  8)
#define FP_CONTEXT_SIZE    (32 * 16)
#define SYS_CONTEXT_SIZE   ( 6 *  8) // 5 SYS regs + Alignment requirement (ie: the stack must be aligned on 0x10)

/* void DoSyscall(EFI_SYSTEM_CONTEXT Context, VOID **SyscallTable) */
ASM_FUNC(DoSyscall)

  stp x29, x30, [sp, #-0x10]!
  stp x19, x20, [sp, #-0x10]!

  mov x19, x0
  mov x20, x1

  ldp	x0, x1, [x19, #16 * 0]
  ldp	x2, x3, [x19, #16 * 1]
  ldp	x4, x5, [x19, #16 * 2]
  ldp	x6, x7, [x19, #16 * 3]

  ldr	x8, [x19, #16 * 4]
  ldr	x8, [x20, x8, lsl #3]		// find the syscall entry

  blr	x8
  str x0, [x19, #16 * 0]

  ldp x19, x20, [sp], #0x10
  ldp x29, x30, [sp], #0x10
  ret




/* void EretToSandbox(EFI_SYSTEM_CONTEXT_AARCH64 *) */
ASM_FUNC(EretToSandbox)
  dmb ish

  // Assume that x27 is always 0
  mov      x27, x0
  add      x28, x0, #GP_CONTEXT_SIZE

  // Pop as many GP regs
  ldp      x2,  x3,  [x27, #0x10]
  ldp      x4,  x5,  [x27, #0x20]
  ldp      x6,  x7,  [x27, #0x30]
  ldp      x8,  x9,  [x27, #0x40]
  ldp      x10, x11, [x27, #0x50]
  ldp      x12, x13, [x27, #0x60]
  ldp      x14, x15, [x27, #0x70]
  ldp      x16, x17, [x27, #0x80]
  ldp      x18, x19, [x27, #0x90]
  ldp      x20, x21, [x27, #0xa0]
  ldp      x22, x23, [x27, #0xb0]
  ldp      x24, x25, [x27, #0xc0]
  ldr      x26, [x27, #0xd0]
  ldp      x0,  x1,  [x27], #0xe0

  // Pop FP regs
  ldp      q2,  q3,  [x28, #0x20]
  ldp      q4,  q5,  [x28, #0x40]
  ldp      q6,  q7,  [x28, #0x60]
  ldp      q8,  q9,  [x28, #0x80]
  ldp      q10, q11, [x28, #0xa0]
  ldp      q12, q13, [x28, #0xc0]
  ldp      q14, q15, [x28, #0xe0]
  ldp      q16, q17, [x28, #0x100]
  ldp      q18, q19, [x28, #0x120]
  ldp      q20, q21, [x28, #0x140]
  ldp      q22, q23, [x28, #0x160]
  ldp      q24, q25, [x28, #0x180]
  ldp      q26, q27, [x28, #0x1a0]
  ldp      q28, q29, [x28, #0x1c0]
  ldp      q30, q31, [x28, #0x1e0]
  ldp      q0,  q1,  [x28], #FP_CONTEXT_SIZE

  // Pop the SYS regs we need
  ldp      x29, x30, [x28]
  ldr      x28, [x28, #0x10]
  msr      fpsr, x28

  //
  // Disable interrupt(IRQ and FIQ) before restoring context,
  // or else the context will be corrupted by interrupt reentrance.
  // Interrupt mask will be restored from spsr by hardware when we call eret
  //
  msr   daifset, #3
  isb

  EL1_OR_EL2_OR_EL3(x28)
1:msr      elr_el1, x29  // Exception Link Register
  msr      spsr_el1, x30 // Saved Processor Status Register 32bit
  b        4f
2:msr      elr_el2, x29  // Exception Link Register
  msr      spsr_el2, x30 // Saved Processor Status Register 32bit
  b        4f
3:msr      elr_el3, x29  // Exception Link Register
  msr      spsr_el3, x30 // Saved Processor Status Register 32bit
4:

  // pop remaining GP regs and sp_el0
  ldp      x30, x28, [x27, #0xf0 - 0xe0]
  msr      sp_el0, x28
  ldp      x28, x29, [x27]

  // set x27 to 0
  mov      x27, xzr

  eret
