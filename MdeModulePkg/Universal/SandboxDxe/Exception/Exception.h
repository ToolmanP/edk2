#ifndef SANDBOX_EXCEPTION_H_
#define SANDBOX_EXCEPTION_H_

#if defined(__x86_64__)

VOID IretToSandbox(EFI_SYSTEM_CONTEXT_X64 *Context);
#elif defined(__aarch64__)
/* Registers */
#define SPSR_EL1_EL0t               0b0000
#define SPSR_EL1_EL1t               0b0100
#define SPSR_EL1_EL1h               0b0101

#define SPSR_EL1_KERNEL             SPSR_EL1_EL1h
#define SPSR_EL1_USER               SPSR_EL1_EL0t

VOID EretToSandbox(EFI_SYSTEM_CONTEXT_AARCH64 *Context);
#endif

#define SANDBOX_RETURN_ADDRESS     0xffffbeefbeef
#define SANDBOX_TRAMPOLINE_RETURN_ADDRESS   0xffffcafecafe


#define EXCEPTION_INSTRUCTION_ABORT 0x100
#define EXCEPTION_DATA_ABORT 0x200

/*
 * Exceptions handled by sandbox core
 */
#define EXCEPTION_SYSTEM_CALL 0
#define EXCEPTION_SANDBOX_RETURN 1
#define EXCEPTION_INSTRUCTION_PERMISSION_FAULT 2

EFI_STATUS RegisterSyncExceptionHandler (BOOLEAN Unregister);

#endif // SANDBOX_EXCEPTION_H_