#include "Library/BaseCounterLib/Counter.h"
#include "Library/BaseLib.h"
#include "Library/DebugLib.h"
#include "Library/PrintLib.h"
#include "Library/ShellCommandLib.h"
#include "Library/ShellLib.h"
#include "Library/UefiShellLevel2CommandsLib/UefiShellLevel2CommandsLib.h"
#include "Protocol/Shell.h"

VOID *AllocatePayload(UINTN Size) {
  VOID *Payload = AllocateZeroPool(Size);
  for (UINTN i = 0; i < Size; i++) {
    ((UINT8 *)Payload)[i] = (UINT8)'a';
  }
  return Payload;
}

VOID FreePayload(VOID *Payload) { FreePool(Payload); }

STATIC SHELL_STATUS RunTest(CONST CHAR16 *Cwd) {
  CHAR16 FileName[4096];
  SHELL_FILE_HANDLE Handle;
  VOID *Payload;
  UINTN FileSize = 128;
  Payload = AllocatePayload(FileSize);

  UINTN ValA = 0, ValB = 0;

  UnicodeSPrint(FileName, sizeof(FileName), L"%sgrub_sim.bin", Cwd);

  gBS->Stall(100);
  ValA = ReadCounter();
  ShellOpenFileByName(
      FileName, &Handle,
      EFI_FILE_MODE_CREATE | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_READ, 0);
  ValB = ReadCounter();

  DEBUG((DEBUG_ERROR, "OpenFile,%lu\n", ValB - ValA));

  gBS->Stall(100);
  ValA = ReadCounter();
  ShellGetFileInfo(Handle);
  ValB = ReadCounter();
  DEBUG((DEBUG_ERROR, "GetFileInfo,%lu\n", ValB - ValA));

  gBS->Stall(100);
  ValA = ReadCounter();
  ShellWriteFile(Handle, &FileSize, Payload);
  ValB = ReadCounter();
  DEBUG((DEBUG_ERROR, "WriteFile,%lu\n", ValB - ValA));

  gBS->Stall(100);
  ValA = ReadCounter();
  ShellSetFilePosition(Handle, 0);
  ValB = ReadCounter();
  DEBUG((DEBUG_ERROR, "SetFilePosition,%lu\n", ValB - ValA));

  gBS->Stall(100);
  ValA = ReadCounter();
  ShellFlushFile(Handle);
  ValB = ReadCounter();
  DEBUG((DEBUG_ERROR, "FlushFile,%lu\n", ValB - ValA));

  gBS->Stall(100);
  ValA = ReadCounter();
  ShellReadFile(Handle, &FileSize, Payload);
  ValB = ReadCounter();
  DEBUG((DEBUG_ERROR, "ReadFile,%lu\n", ValB - ValA));

  gBS->Stall(100);
  ValA = ReadCounter();
  ShellDeleteFile(&Handle) ;
  ValB = ReadCounter();
  DEBUG((DEBUG_ERROR, "DeleteFile,%lu\n", ValB - ValA));

  FreePayload(Payload);
  return EFI_SUCCESS;
}

SHELL_STATUS
EFIAPI
ShellCommandRunFsTest(IN EFI_HANDLE ImageHandle,
                      IN EFI_SYSTEM_TABLE *SystemTable) {

  // Perform a FileSystem Write/Read Stress Test for performance testing.
  //
  EFI_STATUS Status;
  SHELL_STATUS ShellStatus;
  LIST_ENTRY *Package;
  CHAR16 *ProblemParam;
  CONST CHAR16 *Cwd;
  UINT64 Intermediate;
  Status = ShellInitialize();
  ASSERT_EFI_ERROR(Status);

  Status = CommandInit();
  ASSERT_EFI_ERROR(Status);

  Status = ShellCommandLineParse(EmptyParamList, &Package, &ProblemParam, TRUE);
  ShellStatus = SHELL_SUCCESS;

  if (EFI_ERROR(Status)) {
    if ((Status == EFI_VOLUME_CORRUPTED) && (ProblemParam != NULL)) {
      ShellPrintHiiEx(-1, -1, NULL, STRING_TOKEN(STR_GEN_PROBLEM),
                      gShellLevel2HiiHandle, L"fs_test", ProblemParam);
      ShellStatus = SHELL_INVALID_PARAMETER;
      goto out;
    } else {
      ASSERT(FALSE);
    }
  }

  Status = ShellConvertStringToUint64(ShellCommandLineGetRawValue(Package, 1),
                                      &Intermediate, FALSE, FALSE);

  if (EFI_ERROR(Status) || (((UINT64)(UINTN)(Intermediate)) != Intermediate)) {
    ShellPrintHiiEx(-1, -1, NULL, STRING_TOKEN(STR_GEN_PARAM_INV),
                    gShellLevel2HiiHandle, L"fs_test",
                    ShellCommandLineGetRawValue(Package, 1));
    ShellStatus = SHELL_INVALID_PARAMETER;
    goto out;
  }

  Cwd = ShellGetCurrentDir(NULL);

  DebugPrint(DEBUG_INFO, "Size,time\n");

  for (UINTN i = 0; i < 64 ; i++) {
    RunTest(Cwd);
  }

out:
  return ShellStatus;
}
