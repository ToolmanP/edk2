
#ifndef __ASM_BLOB_H__
#define __ASM_BLOB_H__

#if defined (__aarch64__)
#include "AArch64/AsmSyscall.h"
#endif

#include <Base.h>

#define DELAY(x) x
#define ASM_SYS_BLOB_START_DEFINE(Name)  \
  .text                                ; \
  .global   ASM_BLOB_##Name            ; \
  .type     ASM_BLOB_##Name, %function ; \
  ASM_BLOB_##Name:                       \

#define ASM_SYS_BLOB_END_DEFINE(Name)        ; \
ASM_BLOB_##Name##_END:                       ; \
.size ASM_BLOB_##Name, . - ASM_BLOB_##Name   ; \

#define ASM_SYS_BLOB_SIZE_DEFINE(Name)         \
  .data;                                       \
  .global   ASM_BLOB_##Name##_SIZE           ; \
  .type     ASM_BLOB_##Name##_SIZE, %object  ; \
  ASM_BLOB_##Name##_SIZE:; \
    .quad ASM_BLOB_##Name##_END - ASM_BLOB_##Name

#define ASM_SYS_BLOB_GENERIC_DEFINE(NAME) \
ASM_SYS_BLOB_START_DEFINE(SANDBOX_SYS_##NAME);   \
ASM_SYSCALL(SANDBOX_SYS_##NAME);                 \
ASM_SYS_BLOB_END_DEFINE(SANDBOX_SYS_##NAME);     \
ASM_SYS_BLOB_SIZE_DEFINE(SANDBOX_SYS_##NAME);

#endif
