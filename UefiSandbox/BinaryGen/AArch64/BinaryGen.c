#include "BinaryGen.h"
#include "Library/BaseMemoryLib.h"
#include "Library/SandboxSyscallLib.h"
#include "Memory.h"
#include "Memory/Malloc.h"
#include "ProcessorBind.h"

/// The transparency proxy to the protocol interface.

CONST UINT64 ENTRY_TRAMPOLINE_SIZE = 64 * sizeof(UINT32);
CONST UINT64 RETURN_TRAMPOLINE_SIZE = 64 * sizeof(UINT32);

STATIC UINT32 TrampolinePrelude(IN UINT32 *Cursor) {
  STATIC CONST UINT32 Ins[] = {
      0xa9bf7bfd, // stp x29, x30, [sp, #-16]!
      0x910003fd  // mov x29, sp
  };
  CopyMem(Cursor, Ins, 8);
  return sizeof(Ins) / sizeof(UINT32);
}

STATIC UINT32 TrampolinePostlude(IN UINT32 *Cursor) {
  STATIC CONST UINT32 Ins[] = {
      0xa8c17bfd, // ldp x29, x30, [sp], #16
      0xd65f03c0  // ret
  };
  CopyMem(Cursor, Ins, 8);
  return sizeof(Ins) / sizeof(UINT32);
}

STATIC UINT32 TrampolinePreserveContext(IN UINT32 *Cursor) {
  STATIC CONST UINT32 Ins[] = {
      0xd10403ff, // sub	sp, sp, #0x100
      0xa90007e0, // stp x0, x1, [sp]
      0xa9010fe2, // stp x2, x3, [sp, #0x10]
      0xa90217e4, // stp x4, x5, [sp, #0x20]
      0xa9031fe6, // stp x6, x7, [sp, #0x30]
      0xa90427e8, // stp x8, x9, [sp, #0x40]
      0xa9052fea, // stp x10, x11, [sp, #0x50]
      0xa90637ec, // stp x12, x13, [sp, #0x60]
      0xa9073fee, // stp x14, x15, [sp, #0x70]
      0xa90847f0, // stp x16, x17, [sp, #0x80]
      0xa9094ff2, // stp x18, x19, [sp, #0x90]
      0xa90a57f4, // stp x20, x21, [sp, #0xa0]
      0xa90b5ff6, // stp x22, x23, [sp, #0xb0]
      0xa90c67f8, // stp x24, x25, [sp, #0xc0]
      0xa90d6ffa, // stp x26, x27, [sp, #0xd0]
      0xa90e77fc, // stp x28, x29, [sp, #0xe0]
      0xf9007bfe  // str x30, [sp, #0xf0]
  };
  CopyMem(Cursor, Ins, sizeof(Ins));
  return sizeof(Ins) / sizeof(UINT32);
}

STATIC UINT32 TrampolineRestoreContext(IN UINT32 *Cursor) {
  STATIC CONST UINT32 Ins[] = {
      0xf94007e1, // ldr x1, [sp, #0x8]
      0xa9410fe2, // ldp x2, x3, [sp, #0x10]
      0xa94217e4, // ldp x4, x5, [sp, #0x20]
      0xa9431fe6, // ldp x6, x7, [sp, #0x30]
      0xa94427e8, // ldp x8, x9, [sp, #0x40]
      0xa9452fea, // ldp x10, x11, [sp, #0x50]
      0xa94637ec, // ldp x12, x13, [sp, #0x60]
      0xa9473fee, // ldp x14, x15, [sp, #0x70]
      0xa94847f0, // ldp x16, x17, [sp, #0x80]
      0xa9494ff2, // ldp x18, x19, [sp, #0x90]
      0xa94a57f4, // ldp x20, x21, [sp, #0xa0]
      0xa94b5ff6, // ldp x22, x23, [sp, #0xb0]
      0xa94c67f8, // ldp x24, x25, [sp, #0xc0]
      0xa94d6ffa, // ldp x26, x27, [sp, #0xd0]
      0xa94e77fc, // ldp x28, x29, [sp, #0xe0]
      0xf9407bfe, // ldr x30, [sp, #0xf0]
      0x910403ff  // add sp, sp, #0x200
  };
  CopyMem(Cursor, Ins, sizeof(Ins));
  return sizeof(Ins) / sizeof(UINT32);
}

STATIC UINT32 TrampolineMovImmediateReg(IN UINT32 *Cursor,
                                        IN CONST UINT64 Value,
                                        IN UINT32 CONST Reg) {

  UINT16 Chunk;
  if (Reg < 0 || Reg > 30) {
    return -1; // Invalid Register index
  }

  for (int i = 0; i < 4; i++) {
    Chunk = (Value >> (16 * i)) & 0xFFFF;
    if (i == 0) {
      Cursor[i] = 0xd2800000 | (Chunk << 5) | Reg; // movz reg, #chunk
    } else {
      Cursor[i] =
          0xf2800000 | (Chunk << 5) | (i << 21) | Reg; // movk reg, lsl #offset
    }
  }
  return 4;
}

STATIC UINT32 TrampolineCopyStackPointer(IN UINT32 *Cursor,
                                         IN CONST UINT32 Reg) {
  *Cursor = 0x910003e0 | Reg; // mov reg, sp
  return 1;
}

STATIC UINT32 TrampolineSVC(IN UINT32 *Cursor) {
  *Cursor = 0xd4000001; // svc #0
  return 1;
}

STATIC UINT32 TrampolineMoveRegister(IN UINT32 *Cursor, IN CONST UINT32 Src,
                                     IN CONST UINT32 Dst) {
  *Cursor = 0xaa0003e0 | (Src << 5) | Dst; // mov dst, src
  return 1;
}

// A Transparent Trampoline SVC that is injects into the original interace call.
// Token (Interface ID) -> Sandbox's Protocol Interface
VOID *CreateInterfaceEntryPointTrampoline(IN UEFI_SANDBOX *Sandbox,
                                            IN CONST UINT64 Located,
                                            IN CONST UINT64 Offset,
                                            IN BOOLEAN ForCore) {
  UINT32 Count;
  UINT32 *Trampoline;
  Count = 0;
  Trampoline = AllocateSandboxCodeBuffer(Sandbox, ENTRY_TRAMPOLINE_SIZE);
  Count += TrampolinePrelude(Trampoline + Count);
  Count += TrampolinePreserveContext(Trampoline + Count);
  Count += TrampolineMovImmediateReg(Trampoline + Count, Located, 0);
  Count += TrampolineMovImmediateReg(Trampoline + Count, Offset, 1);
  Count += TrampolineCopyStackPointer(Trampoline + Count, 2);
  Count += TrampolineMovImmediateReg(Trampoline + Count,
                                     SANDBOX_SYS_RT_INTERFACE_CALL, 8);
  Count += TrampolineSVC(Trampoline + Count);
  Count += TrampolineRestoreContext(Trampoline + Count);
  Count += TrampolinePostlude(Trampoline + Count);

  DcacheCleanAndInvalidateArea((UINT64)Trampoline, (UINT64)Trampoline + ENTRY_TRAMPOLINE_SIZE);
  FlushIcacheRange((UINT64)Trampoline, (UINT64)Trampoline + ENTRY_TRAMPOLINE_SIZE);
  return Trampoline;
}

VOID *CreateSandboxReturnTrampoline(IN UEFI_SANDBOX *Sandbox,
                                      IN CONST UINT64 JumpContext) {
  UINT32 Count;
  UINT32 *Trampoline;
  Count = 0;
  Trampoline = AllocateSandboxCodeBuffer(Sandbox, RETURN_TRAMPOLINE_SIZE);
  Count += TrampolinePrelude(Trampoline + Count);
  Count += TrampolineMoveRegister(Trampoline + Count, 0, 1);
  Count += TrampolineMovImmediateReg(Trampoline + Count, JumpContext, 0);
  Count += TrampolineMovImmediateReg(Trampoline + Count,
                                     SANDBOX_SYS_RT_SANDBOX_RETURN, 8);
  Count += TrampolineSVC(Trampoline + Count);
  Count += TrampolinePostlude(Trampoline + Count); // FIXME: Psuedo Return Required by QEMU So that the boundary check is correct?
  return Trampoline;
}
