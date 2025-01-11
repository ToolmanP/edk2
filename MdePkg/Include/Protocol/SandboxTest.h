#ifndef __ARCH_PROTOCOL_SANDBOX_TEST_H
#define __ARCH_PROTOCOL_SANDBOX_TEST_H

#include <ProcessorBind.h>
#include <Uefi/UefiBaseType.h>
#include <Uefi/UefiSpec.h>

#define EFI_SANDBOX_TEST_PROTOCOL_GUID \
    { 0x614057a1, 0x9240, 0x42a3, {0xb1, 0xa8, 0x1c, 0xdd, 0xc5, 0x80, 0xa6, 0xfc }}

typedef struct _EFI_SANDBOX_TEST_PROTOCOL EFI_SANDBOX_TEST_PROTOCOL;

typedef struct {
    UINT64 Number;
    UINT64 Offset;
} TestPayload0;

typedef struct {
    UINT64 Index;
    TestPayload0 *Payload0;
} TestPayload1;

typedef EFI_STATUS (EFIAPI *RESET_SUM)(
    IN EFI_SANDBOX_TEST_PROTOCOL *This
);

// Call several times and checksum
typedef EFI_STATUS (EFIAPI *SIMPLE_CALL_TEST)(
    IN EFI_SANDBOX_TEST_PROTOCOL *This,
    IN UINT64 Number,
    IN BOOLEAN CheckSum,
    IN UINT64 Sum
);

// Call several times and checksum
typedef EFI_STATUS (EFIAPI *SIMPLE_INPUT_POINTER_TEST)(
    IN EFI_SANDBOX_TEST_PROTOCOL *This,
    IN UINT64 *Number,
    IN BOOLEAN CheckSum,
    IN UINT64 Sum
);

// Call several times and checksum
// Sum of Payload1 = Index * (Number + Offset)
typedef EFI_STATUS (EFIAPI *COMPLEX_INPUT_POINTER_TEST)(
    IN EFI_SANDBOX_TEST_PROTOCOL *This,
    IN TestPayload1 *Payload1,
    IN BOOLEAN CheckSum,
    IN UINT64 Sum
);

// Call and checksum
typedef EFI_STATUS (EFIAPI *LARGE_INPUT_BUFFER_TEST)(
    IN EFI_SANDBOX_TEST_PROTOCOL *This,
    IN VOID *Buffer,
    IN UINT64 BufferSize,
    IN UINT64 Sum
);

typedef EFI_STATUS (EFIAPI *OUTPUT_POINTER_TEST)(
    IN EFI_SANDBOX_TEST_PROTOCOL *This,
    IN OUT UINT64 *OutputBufferSize,
    IN OUT UINT64 *OutputSum,
    OUT VOID **OutputBuffer
);

struct _EFI_SANDBOX_TEST_PROTOCOL {
    RESET_SUM ResetSum;
    SIMPLE_CALL_TEST SimpleCallTest;
    SIMPLE_INPUT_POINTER_TEST SimpleInputPointerTest;
    COMPLEX_INPUT_POINTER_TEST ComplexInputPointerTest;
    LARGE_INPUT_BUFFER_TEST LargeInputBufferTest;
    OUTPUT_POINTER_TEST OutputPointerTest;
};

extern EFI_GUID gEfiSandboxTestProtocolGuid;

#endif
