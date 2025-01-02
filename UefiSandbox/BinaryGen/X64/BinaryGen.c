#include "BinaryGen.h"
#include "Library/BaseMemoryLib.h"
#include "Library/DebugLib.h"
#include "Library/SandboxSyscallLib.h"
#include "Memory/Malloc.h"
#include "ProcessorBind.h"
#include "UefiSandbox.h"

/// The transparency proxy to the protocol interface.

CONST UINT64 ENTRY_TRAMPOLINE_SIZE = 256 * sizeof(UINT8);
CONST UINT64 RETURN_TRAMPOLINE_SIZE = 256 * sizeof(UINT8);

EFI_STATUS
extern SandboxInterfaceCall(IN VOID *Located, IN UINT64 Offset, IN UINT64 *CallSiteParams);

STATIC UINT8 TrampolinePrelude(IN UINT8 *Cursor) {
  static const UINT8 Ins[] = {
      0xF3, 0x0F, 0x1E, 0xFA, // endbr64
      0x55,             // push rbp
      0x48, 0x89, 0xE5  // mov %rsp, %rbp
  };
  CopyMem(Cursor, Ins, sizeof(Ins));
  return sizeof(Ins);
}

STATIC UINT8 TrampolinePostlude(IN UINT8 *Cursor) {
  Cursor[0] = 0xC9; // leave
  Cursor[1] = 0xC3; // ret
  return 2;
}

STATIC UINT8 TrampolinePreserveContext(IN UINT8 *Cursor) {
  static const UINT8 Ins[] = {
    0x57,                   // push rdi
    0x56,                   // push rsi
    0x41, 0x54,             // push r12
    0x41, 0x55,             // push r13
    0x41, 0x56,             // push r14
    0x41, 0x57              // push r15
  };
  CopyMem(Cursor, Ins, sizeof(Ins));
  return sizeof(Ins);
}

STATIC UINT8 TrampolineRestoreContext(IN UINT8 *Cursor) {
  static const UINT8 Ins[] = {
    0x41, 0x5F, // pop r15
    0x41, 0x5E, // pop r14
    0x41, 0x5D, // pop r13
    0x41, 0x5C, // pop r12
    0x5E,       // pop rsi
    0x5F        // pop rdi
  };
  CopyMem(Cursor, Ins, sizeof(Ins));
  return sizeof(Ins);
}

// Register index 0-15: RAX, RBX, RCX, RDX, RBP, RSP, RSI, RDI, R8-R15
STATIC UINT32 TrampolineMovImmediateReg(IN UINT8 *Cursor,
                                        IN CONST UINT64 Value,
                                        IN UINT32 CONST Reg) {
  if (Reg < 0 || Reg > 15) {
    return -1; // Invalid register index
  }

  // REX prefix for 64-bit registers
  UINT8 rex_prefix = 0x48;
  if (Reg >= 8) { // High registers require the R bit in the REX prefix
    rex_prefix |= 0x01;
  }

  // MOV opcode for immediate to register (0xB8 to 0xBF for RAX to R15)
  UINT8 opcode = 0xB8 + (Reg & 0x07);

  // Write REX prefix
  Cursor[0] = rex_prefix;

  // Write opcode
  Cursor[1] = opcode;

  // Write 64-bit immediate value (little-endian)
  for (int i = 0; i < 8; i++) {
    Cursor[2 + i] = (Value >> (8 * i)) & 0xFF;
  }

  // Total instruction size: 1 (REX) + 1 (opcode) + 8 (immediate) = 10 bytes
  return 10;
}

STATIC UINT8 TrampolineSetParams(IN UINT8 *Cursor) {
  static const UINT8 Ins[] = {
    0x48, 0x89, 0x4D, 0x10,       // mov %rcx, 0x10(%rbp)
    0x48, 0x89, 0x55, 0x18,       // mov %rdx, 0x18(%rbp)
    0x49, 0x89, 0x45, 0x20,       // mov %r8, 0x20(%rbp)
    0x49, 0x89, 0x4D, 0x28,       // mov %r9, 0x28(%rbp)
    0x48, 0x8B, 0xD5,             // mov rbp, %rdx
    0x48, 0x83, 0xC2, 0x10        // add $0x10, %rdx
  };
  CopyMem(Cursor, Ins, sizeof(Ins));
  return sizeof(Ins); 
}

STATIC UINT8 TrampolineSVC(IN UINT8 *Cursor) {
  // syscall opcode is 0x0F 0x05 in x86_64
  Cursor[0] = 0x0F; // First byte of syscall opcode
  Cursor[1] = 0x05; // Second byte of syscall opcode
  return 2;         // Instruction is 2 bytes long
}

STATIC UINT8 TrampolineCallFunc(IN UINT8 *Cursor, IN CONST UINT64 Func) {
  static const UINT8 Ins_mov_rax[] = { 0x48, 0xB8 }; // mov rax, imm64
  static const UINT8 Ins_call_rax[] = { 0xFF, 0xD0 }; // call rax

  // 复制 'mov rax, <Func>' 指令前缀
  CopyMem(Cursor, Ins_mov_rax, sizeof(Ins_mov_rax));
  Cursor += sizeof(Ins_mov_rax);

  // 复制 <Func> 作为立即数
  CopyMem(Cursor, &Func, sizeof(Func));
  Cursor += sizeof(Func);

  // 复制 'call rax' 指令
  CopyMem(Cursor, Ins_call_rax, sizeof(Ins_call_rax));
  Cursor += sizeof(Ins_call_rax);

  // 返回写入的总字节数
  return sizeof(Ins_mov_rax) + sizeof(Func) + sizeof(Ins_call_rax); // 2 + 8 + 2 = 12
}

STATIC UINT8 TrampolineMoveRegister(IN UINT8 *Cursor, IN CONST UINT32 Src,
                                     IN CONST UINT32 Dst) {
  if (Src > 15 || Dst > 15) {
    return -1; // Invalid register index
  }

  // REX prefix for 64-bit registers
  UINT8 rex_prefix = 0x48;
  if (Src >= 8) rex_prefix |= 0x01; // High src register requires R bit
  if (Dst >= 8) rex_prefix |= 0x04; // High dst register requires B bit

  // Opcode for mov reg, reg
  UINT8 opcode = 0x89;

  // ModR/M byte: encodes register-register operation
  UINT8 modrm = 0xC0 | ((Src & 0x07) << 3) | (Dst & 0x07);

  // Write to Cursor
  Cursor[0] = rex_prefix;
  Cursor[1] = opcode;
  Cursor[2] = modrm;

  return 3; // Instruction is 3 bytes long
}

/*
 *  Create a trampoline that jumps to the original interface call.
 *  If the trampoline is used by a sandbox, it uses syscall,
 *  or if it is used by the core, it uses a direct call. 
 *
 *  The protocol function is EFIAPI and called with ms_abi, the first four arguments
 *  are passed in registers (RCX, RDX, R8, R9), and the rest are passed on the stack.
 *
 *  Layout of the stack:
  * +----------------------+
 * |   Stack Parameters    |
 * +-----------------------+
 * | Shadow Space (4 * 8)  |
 * +-----------------------+
 * | Caller Return Address | 
 * +-----------------------+ <- RSP
 *
 * Case 1: Used by sandbox
 *  A Transparent Trampoline SVC that is injects into the original interace call.
 *  Token (Interface ID) -> Sandbox's Protocol Interface
 *  Syscall number in %rax, and the syscall arguments are in %rdi, %rsi, %rdx, %r10, %r8, %r9
 * 
 *  Assembly code (AT&T syntax):
 *    endbr64
 *    push %rbp
 *    mov %rsp, %rbp
 *    push %rdi
 *    push %rsi
 *    push %r12
 *    push %r13
 *    push %r14
 *    push %r15
 *    mov %rcx, 0x10(%rbp) 
 *    mov %rdx, 0x18(%rbp)
 *    mov %r8, 0x20(%rbp)
 *    mov %r9, 0x28(%rbp)
 *    mov %rbp, %rdx
 *    add $0x10, %rdx
 *    movimm $Located, %rdi
 *    movimm $Offset, %rsi
 *    movimm $SANDBOX_SYS_RT_INTERFACE_CALL, %rax
 *    syscall
 *    pop %r15
 *    pop %r14
 *    pop %r13
 *    pop %r12
 *    pop %rsi
 *    pop %rdi
 *    leave
 *    ret
 *
 * Case 2: Used by core
 *  A Transparent Trampoline that calls SandboxInterfaceCall.
 *  The parameters passed in obey the Microsoft x64 calling convention (RCX, RDX, R8, R9, stack),
 *  while we call SandboxInterfaceCall with SysV calling convention (RDI, RSI, RDX, RCX, R8, R9).
 */
VOID *CreateInterfaceEntryPointTrampoline(IN UEFI_SANDBOX *Sandbox,
                                            IN CONST UINT64 Located,
                                            IN CONST UINT64 Offset,
                                            IN BOOLEAN ForCore) {
  UINT8 Count;
  UINT8 *Trampoline;
  Count = 0;
  Trampoline = AllocateSandboxCodeBuffer(Sandbox, ENTRY_TRAMPOLINE_SIZE);
  Count += TrampolinePrelude(Trampoline + Count);
  Count += TrampolinePreserveContext(Trampoline + Count);
  Count += TrampolineSetParams(Trampoline + Count);  // ParamsOnStack -> rdx
  Count += TrampolineMovImmediateReg(Trampoline + Count, Located, 7); // Located -> rdi
  Count += TrampolineMovImmediateReg(Trampoline + Count, Offset, 6); // Offset -> rsi
  if (ForCore) {
    Count += TrampolineCallFunc(Trampoline + Count, (UINT64)SandboxInterfaceCall);
  } else { 
    Count += TrampolineMovImmediateReg(Trampoline + Count, SANDBOX_SYS_RT_INTERFACE_CALL, 0); // Syscall number -> rax
    Count += TrampolineSVC(Trampoline + Count);
  }
  Count += TrampolineRestoreContext(Trampoline + Count);
  Count += TrampolinePostlude(Trampoline + Count);
  ASSERT(Count <= ENTRY_TRAMPOLINE_SIZE);
  return Trampoline;
}

VOID *CreateSandboxReturnTrampoline(IN UEFI_SANDBOX *Sandbox,
                                      IN CONST UINT64 JumpContext) {
  UINT8 Count;
  UINT8 *Trampoline;
  Count = 0;
  Trampoline = AllocateSandboxCodeBuffer(Sandbox, RETURN_TRAMPOLINE_SIZE);
  Count += TrampolineMoveRegister(Trampoline + Count, 0, 6); // rax (return Status) -> rsi
  Count += TrampolineMovImmediateReg(Trampoline + Count, JumpContext, 7); // JumpContext -> rdi
  Count += TrampolineMovImmediateReg(Trampoline + Count,
                                     SANDBOX_SYS_RT_SANDBOX_RETURN, 0); // Syscall number -> rax
  Count += TrampolineSVC(Trampoline + Count);
  ASSERT(Count <= RETURN_TRAMPOLINE_SIZE);
  return Trampoline;
}
