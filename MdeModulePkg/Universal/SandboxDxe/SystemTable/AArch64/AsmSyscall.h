#ifndef __ASM_SYSCALL_H__
#define __ASM_SYSCALL_H__
#include <AsmMacroIoLibV8.h>
#define ASM_SYSCALL(n)    \
  AARCH64_BTI(c)         ;\
  mov x8, n              ;\
  svc 0                  ;\
  ret                    ;\

#endif
