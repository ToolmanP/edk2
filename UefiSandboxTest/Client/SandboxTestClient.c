#include "Library/BaseCounterLib/Counter.h"
#include "Library/BaseLib.h"
#include "Library/DebugLib.h"
#include "Library/MemoryAllocationLib.h"
#include "Library/UefiBootServicesTableLib.h"
#include "Library/UefiRuntimeServicesTableLib.h"
#include "Protocol/SandboxTest.h"
#include "Uefi/UefiBaseType.h"

EFI_SANDBOX_TEST_PROTOCOL *SandboxTest;

#define PRINT_TOTAL 0

EFI_STATUS
Test0(UINT64 *TotalTSC) {
  EFI_STATUS Status;
  // UINT64 StartTSC, EndTSC;
  // UINT64 CallStartTSC, CallEndTSC;
  UINT64 CallAverageTSC = 0;
  // UINT64 mSum = 0;
  (void)(CallAverageTSC);

  {
    // Status = SandboxTest->ResetSum(SandboxTest);
    // if (EFI_ERROR(Status)) {
    //   DebugPrint(DEBUG_ERROR, "ResetSum failed\n");
    //   return Status;
    // }

    // StartTSC = ReadCounter();

    for (UINT64 i = 0; i < 100; i++) {
      // CallStartTSC = ReadCounter();
      Status = SandboxTest->SimpleCallTest(SandboxTest, i, FALSE, 0);
      if (EFI_ERROR(Status)) {
        DebugPrint(DEBUG_ERROR, "Test0 failed\n");
        return Status;
      }
      // CallEndTSC = ReadCounter();
      // CallAverageTSC += CallEndTSC - CallStartTSC;
      // mSum += i;
    }

    // Status = SandboxTest->SimpleCallTest(SandboxTest, 0, TRUE, mSum);
    // if (EFI_ERROR(Status)) {
    //   DebugPrint(DEBUG_ERROR, "Test0 sum not right\n");
    //   return Status;
    // }

    // EndTSC = ReadCounter();

    // if (TotalTSC != NULL) {
    //   *TotalTSC = EndTSC - StartTSC;
    // }
#if PRINT_TOTAL
    DebugPrint(DEBUG_INFO, "Simple,%ld,%ld\n", *TotalTSC, CallAverageTSC / 100);
#endif
  }

  return Status;
}

EFI_STATUS
Test1(UINT64 *TotalTSC) {
  EFI_STATUS Status;
  // UINT64 StartTSC, EndTSC;
  // UINT64 mSum = 0;
  UINT64 Value;

  {
    // Status = SandboxTest->ResetSum(SandboxTest);
    // if (EFI_ERROR(Status)) {
    //   DebugPrint(DEBUG_ERROR, "ResetSum failed\n");
    //   return Status;
    // }

    // StartTSC = ReadCounter();

    for (UINT64 i = 0; i < 100; i++) {
      Value = i;
      Status =
          SandboxTest->SimpleInputPointerTest(SandboxTest, &Value, FALSE, 0);
      if (EFI_ERROR(Status)) {
        DebugPrint(DEBUG_ERROR, "Test1 failed\n");
        return Status;
      }
      // mSum += i;
    }

    // Status = SandboxTest->SimpleInputPointerTest(SandboxTest, NULL, TRUE, mSum);
    // if (EFI_ERROR(Status)) {
    //   DebugPrint(DEBUG_ERROR, "Test1 sum not right\n");
    //   return Status;
    // }

    // EndTSC = ReadCounter();

    // if (TotalTSC != NULL) {
    //   *TotalTSC = EndTSC - StartTSC;
    // }
#if PRINT_TOTAL
    DebugPrint(DEBUG_INFO, "Pointer,%ld,%ld\n", *TotalTSC, *TotalTSC / 100);
#endif
  }

  return Status;
}

EFI_STATUS
Test2(UINT64 *TotalTSC) {
  EFI_STATUS Status;
  UINT64 StartTSC, EndTSC;
  UINT64 mSum = 0;
  TestPayload1 Payload1;
  TestPayload0 Payload0;

  {
    // Status = SandboxTest->ResetSum(SandboxTest);
    // if (EFI_ERROR(Status)) {
    //   DebugPrint(DEBUG_ERROR, "ResetSum failed\n");
    //   return Status;
    // }

    StartTSC = ReadCounter();

    for (UINT64 i = 0; i < 100; i++) {
      Payload0.Number = i;
      Payload0.Offset = 1;
      Payload1.Payload0 = &Payload0;
      Payload1.Index = i;
      Status = SandboxTest->ComplexInputPointerTest(SandboxTest, &Payload1,
                                                    FALSE, 0);
      if (EFI_ERROR(Status)) {
        DebugPrint(DEBUG_ERROR, "Test2 failed\n");
        return Status;
      }
      mSum += i * (i + 1);
    }

    Status =
        SandboxTest->ComplexInputPointerTest(SandboxTest, NULL, TRUE, mSum);
    if (EFI_ERROR(Status)) {
      DebugPrint(DEBUG_ERROR, "Test2 sum not right\n");
      return Status;
    }

    EndTSC = ReadCounter();

    if (TotalTSC != NULL) {
      *TotalTSC = EndTSC - StartTSC;
    }
#if PRINT_TOTAL
    DebugPrint(DEBUG_INFO, "Complex,%ld,%ld\n", *TotalTSC, *TotalTSC / 100);
#endif
  }

  return Status;
}

EFI_STATUS
Test3(UINT64 *TotalTSC) {
  EFI_STATUS Status;
  // UINT64 StartTSC, EndTSC;
  UINT64 mSum = 0;
  UINT64 BufferSize = 100 * sizeof(UINT64);
  VOID *Buffer;

  {
    // Status = SandboxTest->ResetSum(SandboxTest);
    // if (EFI_ERROR(Status)) {
    //   DebugPrint(DEBUG_ERROR, "ResetSum failed\n");
    //   return Status;
    // }

    Buffer = AllocatePool(BufferSize);
    for (UINT64 i = 0; i < BufferSize / sizeof(UINT64); i++) {
      ((UINT64 *)Buffer)[i] = i;
      mSum += i;
    }

    // StartTSC = ReadCounter();

    Status = SandboxTest->LargeInputBufferTest(SandboxTest, Buffer, BufferSize,
                                               mSum);
    if (EFI_ERROR(Status)) {
      DebugPrint(DEBUG_ERROR, "Large Input Test3 sum not right\n");
      return Status;
    }

    // EndTSC = ReadCounter();

    // if (TotalTSC != NULL) {
    //   *TotalTSC = EndTSC - StartTSC;
    // }
#if PRINT_TOTAL
    DebugPrint(DEBUG_INFO, "Large,%ld,-1\n", *TotalTSC);
#endif
  }

  return Status;
}

EFI_STATUS
Test4(UINT64 *TotalTSC) {
  EFI_STATUS Status;
  // UINT64 StartTSC, EndTSC;
  // UINT64 mSum = 0;
  UINT64 *Buffer = NULL;
  UINT64 BufferSize = 0;
  UINT64 NumberSum = 0;

  {
    // Status = SandboxTest->ResetSum(SandboxTest);
    // if (EFI_ERROR(Status)) {
    //   DebugPrint(DEBUG_ERROR, "ResetSum failed\n");
    //   return Status;
    // }

    // StartTSC = ReadCounter();

    Status = SandboxTest->OutputPointerTest(SandboxTest, &BufferSize,
                                            &NumberSum, (VOID **)&Buffer);
    if (EFI_ERROR(Status)) {
      DebugPrint(DEBUG_ERROR, "Output Pointer Test4 failed\n");
      return Status;
    }

    // for (UINT64 i = 0; i < BufferSize / sizeof(UINT64); i++) {
    //   mSum += Buffer[i];
    // }

    // if (mSum != NumberSum) {
    //   DebugPrint(DEBUG_ERROR, "Output Pointer Test4 sum not right\n");
    //   return EFI_ABORTED;
    // }

    // EndTSC = ReadCounter();

    // if (TotalTSC != NULL) {
    //   *TotalTSC = EndTSC - StartTSC;
    // }
#if PRINT_TOTAL
    DebugPrint(DEBUG_INFO, "OutputPointer,%ld,-1\n", *TotalTSC);
#endif
  }

  return Status;
}

EFI_STATUS
EFIAPI
SandboxTestStart(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable) {
  EFI_STATUS Status;
  UINT64 TestResults[5];

  Status = gBS->LocateProtocol(&gEfiSandboxTestProtocolGuid, NULL,
                               (VOID **)&SandboxTest);
  if (EFI_ERROR(Status)) {
    DebugPrint(DEBUG_ERROR, "Fail to connect server\n");
    return Status;
  }

  DebugPrint(DEBUG_INFO, "Start of SandboxTestClient\n");
  UINTN Rounds = 16;

#if PRINT_TOTAL
  DebugPrint(DEBUG_INFO, "TestCase,Total,Average\n");
#else
  DebugPrint(DEBUG_INFO, "Parts,TSC\n");
#endif
  for(UINTN i = 0; i < Rounds; i++) {
    Status = Test0(TestResults);
    if (EFI_ERROR(Status)) {
      DebugPrint(DEBUG_ERROR, "Test0 failed\n");
      return Status;
    }
  }

#if !PRINT_TOTAL
  DebugPrint(DEBUG_INFO, "\n");
#endif


#if !PRINT_TOTAL
  DebugPrint(DEBUG_INFO, "Parts,TSC\n");
#endif

  for(UINTN i = 0; i < Rounds; i++) {

    Status = Test1(TestResults + 1);
    if (EFI_ERROR(Status)) {
      DebugPrint(DEBUG_ERROR, "Test1 failed\n");
      return Status;
    }

    // Status = Test2(TestResults + 2);
    // if (EFI_ERROR(Status)) {
    //   DebugPrint(DEBUG_ERROR, "Test2 failed\n");
    //   return Status;
    // }

  }

#if !PRINT_TOTAL
  DebugPrint(DEBUG_INFO, "\n");
#endif

#if !PRINT_TOTAL
  DebugPrint(DEBUG_INFO, "Parts,TSC\n");
#endif

  for(UINTN i = 0; i < Rounds; i++) {
    Status = Test3(TestResults + 3);
    if (EFI_ERROR(Status)) {
      DebugPrint(DEBUG_ERROR, "Test3 failed\n");
      return Status;
    }
  }

#if !PRINT_TOTAL
  DebugPrint(DEBUG_INFO, "\n");
#endif

#if !PRINT_TOTAL
  DebugPrint(DEBUG_INFO, "Parts,TSC\n");
#endif

  for(UINTN i = 0; i < Rounds; i++) {
    Status = Test4(TestResults + 4);
    if (EFI_ERROR(Status)) {
      DebugPrint(DEBUG_ERROR, "Test4 failed\n");
      return Status;
    }
  }

  return Status;
}
