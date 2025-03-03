#include <AsmMacroIoLib.h>

ASM_FUNC(ProxySMC)
  stp x29, x30, [sp, #-0x10]!
  stp x19, x20, [sp, #-0x10]!
  mov x19, x0 // x19 = context

  ldp x0, x1, [x19] // x0 = context->SmcId
  ldp x2, x3, [x19, #0x10] // x2 = context->Arg0
  ldp x4, x5, [x19, #0x20]
  ldp x6, x7, [x19, #0x30]
  ldp x8, x9, [x19, #0x40]
  ldp x10, x11, [x19, #0x50]
  ldp x12, x13, [x19, #0x60]
  ldp x14, x15, [x19, #0x70]
  ldp x16, x17, [x19, #0x80]
  smc #0
  stp x0, x1, [x19] // x0 = context->SmcId
  stp x2, x3, [x19, #0x10] // x2 = context->Arg0
  stp x4, x5, [x19, #0x20]
  stp x6, x7, [x19, #0x30]
  stp x8, x9, [x19, #0x40]
  stp x10, x11, [x19, #0x50]
  stp x12, x13, [x19, #0x60]
  stp x14, x15, [x19, #0x70]
  stp x16, x17, [x19, #0x80]

  ldp x19, x20, [sp], #0x10
  ldp x29, x30, [sp], #0x10
  ret
