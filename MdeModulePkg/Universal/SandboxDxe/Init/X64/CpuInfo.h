#ifndef SANDBOX_CPU_INFO_H_
#define SANDBOX_CPU_INFO_H_

#include "Protocol/DebugSupport.h"

typedef struct {
    UINT64 CurrentSyscallNumber;
    UINT64 KernelStackTop;
    EFI_SYSTEM_CONTEXT_X64 *Context;
} CpuInfo;

#endif