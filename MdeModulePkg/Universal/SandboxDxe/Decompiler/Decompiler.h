#ifndef __DECOMPILER_H__
#define __DECOMPILER_H__

typedef EFI_STATUS (*Resolve)(UINTN Instr);

typedef struct Decompiler {
  VOID *CodeBuffer;
  EFI_STATUS (*Resolve)(EFI_SYSTEM_CONTEXT);

} DECOMPILER;

extern DECOMPILER mDecompiler;
#endif
