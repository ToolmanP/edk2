#include <SystemTable/AsmBlob.h>
#include <Library/SandboxSyscalls.h>

ASM_SYS_BLOB_START_DEFINE(SANDBOX_SYS_BS_INSTALL_MULTIPLE_PROTOCOL_INTERFACES)
  sub sp, sp, #0x40
  str x7, [sp, #0x38]
  str x6, [sp, #0x30]
  str x5, [sp, #0x28]
  str x4, [sp, #0x20]
  str x3, [sp, #0x18]
  str x2, [sp, #0x10]
  str x1, [sp, #0x8]
  str x0, [sp, #0x0]
  mov x6, x29
  mov x7, x30
  add x4, sp, #0x8
  mov x0, #16
  mov x8, #SANDBOX_SYS_BS_RAISE_TPL
  svc 0
  mov x5, x0
loop:
  ldr x1, [x4]
  cmp x1, #0
  b.eq out
  ldr x0, [sp]
  mov x2, #0
  ldr x3, [x4, #0x8]
  mov x8, #SANDBOX_SYS_BS_INSTALL_PROTOCOL_INTERFACE
  svc 0
  cmp x0, #0
  b.ne out
  add x4, x4, #0x10
  b loop
out:
  mov x0, x5
  mov x8, #SANDBOX_SYS_BS_RESTORE_TPL
  svc 0
  add sp, sp, #0x40
  mov x29, x6
  mov x30, x7
  ret
ASM_SYS_BLOB_END_DEFINE(SANDBOX_SYS_BS_INSTALL_MULTIPLE_PROTOCOL_INTERFACES)
ASM_SYS_BLOB_SIZE_DEFINE(SANDBOX_SYS_BS_INSTALL_MULTIPLE_PROTOCOL_INTERFACES)

ASM_SYS_BLOB_START_DEFINE(SANDBOX_SYS_BS_UNINSTALL_MULTIPLE_PROTOCOL_INTERFACES)
  sub sp, sp, #0x40
  str x7, [sp, #0x38]
  str x6, [sp, #0x30]
  str x5, [sp, #0x28]
  str x4, [sp, #0x20]
  str x3, [sp, #0x18]
  str x2, [sp, #0x10]
  str x1, [sp, #0x8]
  str x0, [sp, #0x0]
  mov x6, x29
  mov x7, x30
  add x4, sp, #0x8
loop2:
  ldr x1, [x4]
  cmp x1, #0
  b.eq out2
  ldr x0, [sp]
  mov x2, #0
  ldr x3, [x4, #0x8]
  mov x8, #SANDBOX_SYS_BS_UNINSTALL_PROTOCOL_INTERFACE
  svc 0
  cmp x0, #0
  b.ne out2
  add x4, x4, #0x10
  b loop2
out2:
  add sp, sp, #0x40
  mov x29, x6
  mov x30, x7
  ret
ASM_SYS_BLOB_END_DEFINE(SANDBOX_SYS_BS_UNINSTALL_MULTIPLE_PROTOCOL_INTERFACES)
ASM_SYS_BLOB_SIZE_DEFINE(SANDBOX_SYS_BS_UNINSTALL_MULTIPLE_PROTOCOL_INTERFACES)

