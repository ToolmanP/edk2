#include "Base.h"
#include "Library/BaseCounterLib/Counter.h"
#include "Library/DebugLib.h"
#include "Library/MemoryAllocationLib.h"
#include "Library/UefiBootServicesTableLib.h"
#include "Library/UefiLib.h"
#include "Uefi/UefiBaseType.h"
#include "Protocol/SandboxTest.h"

UINT64 NumberSum = 0;

EFI_STATUS
EFIAPI
ResetSum(IN EFI_SANDBOX_TEST_PROTOCOL *This) {
  NumberSum = 0;
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
SimpleCallTest(IN EFI_SANDBOX_TEST_PROTOCOL *This, IN UINT64 Number, IN BOOLEAN CheckSum, IN UINT64 Sum) {
  // NumberSum += Number;
  // if (CheckSum) {
  //   if (NumberSum != Sum) {
  //     return EFI_ABORTED;
  //   }
  // }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
SimpleInputPointerTest(IN EFI_SANDBOX_TEST_PROTOCOL *This, IN UINT64 *Number, IN BOOLEAN CheckSum, IN UINT64 Sum) {
  // if (Number) 
  //   NumberSum += *Number;

  // if (CheckSum) {
  //   if (NumberSum != Sum) {
  //     return EFI_ABORTED;
  //   }
  // }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
ComplexInputPointerTest(IN EFI_SANDBOX_TEST_PROTOCOL *This, IN TestPayload1 *Payload1, IN BOOLEAN CheckSum, IN UINT64 Sum) {
  // if (Payload1)
  //   NumberSum += Payload1->Index * (Payload1->Payload0->Number + Payload1->Payload0->Offset);

  // if (CheckSum) {
  //   if (NumberSum != Sum) {
  //     return EFI_ABORTED;
  //   }
  // }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
LargeInputBufferTest(IN EFI_SANDBOX_TEST_PROTOCOL *This, IN VOID *Buffer, IN UINT64 BufferSize, IN UINT64 Sum) {
  // UINT64 *Numbers = (UINT64 *)Buffer;
  // UINT64 Count = BufferSize / sizeof(UINT64);
  // for (UINT64 i = 0; i < Count; i++) {
  //   NumberSum += Numbers[i];
  // }

  // if (NumberSum != Sum) {
  //   return EFI_ABORTED;
  // }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
OutputPointerTest(IN EFI_SANDBOX_TEST_PROTOCOL *This, IN OUT UINT64 *OutputBufferSize, IN OUT UINT64 *OutputSum, OUT VOID **OutputBuffer) {
  UINT64 Count = 16;
  UINT64 Sum = 0;

  *OutputBuffer = AllocatePool(sizeof(UINT64) * Count);
  // for (UINT64 i = 0; i < Count; i++) {
  //   ((UINT64 *)*OutputBuffer)[i] = i;
  //   Sum += i;
  // }

  *OutputBufferSize = sizeof(UINT64) * Count;
  *OutputSum = Sum;
  return EFI_SUCCESS;
}

EFI_HANDLE mSandboxTestServerHandle = NULL;
EFI_SANDBOX_TEST_PROTOCOL mSandboxTest = {
  .ResetSum = ResetSum,
  .SimpleCallTest = SimpleCallTest,
  .SimpleInputPointerTest = SimpleInputPointerTest,
  .ComplexInputPointerTest = ComplexInputPointerTest,
  .LargeInputBufferTest = LargeInputBufferTest,
  .OutputPointerTest = OutputPointerTest
};

EFI_STATUS
EFIAPI
SandboxTestInit(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable) {
  DebugPrint(DEBUG_INFO, "SandboxTestServerInit\n");
  return gBS->InstallProtocolInterface(&mSandboxTestServerHandle, &gEfiSandboxTestProtocolGuid,
                                EFI_NATIVE_INTERFACE, &mSandboxTest);
}