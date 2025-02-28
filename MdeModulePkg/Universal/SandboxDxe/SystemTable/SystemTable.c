#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Pi/PiHob.h>

#include <Interface/Interface.h>
#include <Interface/Registry.h>
#include <Memory/Malloc.h>
#include <Memory/Memory.h>
#include <PageTable.h>
#include <SandboxDxe.h>
#include <SystemTable/Blob.h>
#include <SystemTable/SystemTable.h>
#include <Utils/Logger.h>

extern EFI_BOOT_SERVICES SandboxWrapperBootServicesTable;
extern EFI_RUNTIME_SERVICES SandboxWrapperRuntimeServicesTable;

EFI_STATUS StartOfServiceWrappers(VOID);
EFI_STATUS EndOfServiceWrappers(VOID);

VOID SetServiceFunctionsVirtualAddress(EFI_SYSTEM_TABLE *SandboxSystemTable);

STATIC
EFI_STATUS
MapSandboxHOBList(IN UEFI_SANDBOX *Sandbox, IN EFI_SYSTEM_TABLE *DstST,
                  IN EFI_SYSTEM_TABLE *srcST) {
  EFI_STATUS Status;
  VOID *HobStart;
  VOID *HobEnd;
  UINTN Index;
  EFI_PEI_HOB_POINTERS Hob;

  HobStart = NULL;

  /*
   * Get Hob list start address
   */
  for (Index = 0; Index < srcST->NumberOfTableEntries; Index++) {
    if (CompareGuid(&srcST->ConfigurationTable[Index].VendorGuid,
                    &gEfiHobListGuid)) {
      HobStart = srcST->ConfigurationTable[Index].VendorTable;
      break;
    }
  }

  if (HobStart == NULL) {
    return EFI_NOT_FOUND;
  }

  /*
   * Get Hob list end address
   */
  Hob.Raw = HobStart;
  while (!END_OF_HOB_LIST(Hob)) {
    Hob.Raw = GET_NEXT_HOB(Hob);
  }
  HobEnd = Hob.Raw;

  /*
   * Map HobList in sandbox pagetable
   */
  Status = AddVMRegion(Sandbox, PHYS_TO_VIRT(HobStart),
                       (EFI_PHYSICAL_ADDRESS)HobStart, HobEnd - HobStart,
                       VMR_READ, FALSE);

  /*
   * Set HobList pointer
   */
  if (!EFI_ERROR(Status)) {
    DstST->ConfigurationTable[Index].VendorTable =
        (VOID *)PHYS_TO_VIRT(HobStart);
  }

  return Status;
}

EFI_STATUS
InitAndMapSandboxSystemTable(IN OUT UEFI_SANDBOX *Sandbox,
                             IN EFI_SYSTEM_TABLE *srcST) {
  EFI_STATUS Status;
  VOID *BufferBase;
  EFI_SYSTEM_TABLE *SandboxSystemTable;
  UINTN BufferSize;

  BufferSize = sizeof(EFI_SYSTEM_TABLE) + sizeof(EFI_BOOT_SERVICES) +
               sizeof(EFI_RUNTIME_SERVICES) +
               (sizeof(EFI_CONFIGURATION_TABLE) * srcST->NumberOfTableEntries);
  BufferSize = ROUND_UP(BufferSize, PAGE_SIZE);

  BufferBase = AllocatePages(BufferSize / PAGE_SIZE);

  /*
   * Map the system table region as normal executable memory
   */
  Status = AddVMRegion(Sandbox, PHYS_TO_VIRT(BufferBase),
                       (EFI_PHYSICAL_ADDRESS)BufferBase, BufferSize,
                       VMR_READ | VMR_WRITE | VMR_EXEC, FALSE);

  ASSERT_EFI_ERROR(Status);

  if (BufferBase == NULL) {
    SBError("Fail to allocate buffer for sandbox system table\n");
    return EFI_OUT_OF_RESOURCES;
  }
  ZeroMem(BufferBase, BufferSize);

  SandboxSystemTable = (EFI_SYSTEM_TABLE *)BufferBase;
  CopyMem(SandboxSystemTable, srcST, sizeof(EFI_SYSTEM_TABLE));

  SandboxSystemTable->BootServices =
      (EFI_BOOT_SERVICES *)(BufferBase + sizeof(EFI_SYSTEM_TABLE));
  ZeroMem(SandboxSystemTable->BootServices, sizeof(EFI_BOOT_SERVICES));

  SandboxSystemTable->RuntimeServices =
      (EFI_RUNTIME_SERVICES *)(BufferBase + sizeof(EFI_SYSTEM_TABLE) +
                               sizeof(EFI_BOOT_SERVICES));
  ZeroMem(SandboxSystemTable->RuntimeServices, sizeof(EFI_RUNTIME_SERVICES));

  /*
   * Only set VendorGuid,
   * the setup of each VendorTable will depend on the sandbox, as it may not be
   * used
   */
  SandboxSystemTable->ConfigurationTable =
      (EFI_CONFIGURATION_TABLE *)(BufferBase + sizeof(EFI_SYSTEM_TABLE) +
                                  sizeof(EFI_BOOT_SERVICES) +
                                  sizeof(EFI_RUNTIME_SERVICES));
  for (UINTN i = 0; i < srcST->NumberOfTableEntries; i++) {
    CopyGuid(&SandboxSystemTable->ConfigurationTable[i].VendorGuid,
             &srcST->ConfigurationTable[i].VendorGuid);
  }

  Status = MapSandboxHOBList(Sandbox, SandboxSystemTable, srcST);
  ASSERT_EFI_ERROR(Status);
  SetSystemTableProtocolInterface(Sandbox, SandboxSystemTable, srcST);

  /*
   * Set the pointers to sandbox virtual address
   */
  SandboxSystemTable->BootServices =
      (EFI_BOOT_SERVICES *)PHYS_TO_VIRT(SandboxSystemTable->BootServices);
  SandboxSystemTable->RuntimeServices =
      (EFI_RUNTIME_SERVICES *)PHYS_TO_VIRT(SandboxSystemTable->RuntimeServices);
  SandboxSystemTable->ConfigurationTable =
      (EFI_CONFIGURATION_TABLE *)PHYS_TO_VIRT(
          SandboxSystemTable->ConfigurationTable);
  Sandbox->ImageData.Info.SystemTable =
      (EFI_SYSTEM_TABLE *)PHYS_TO_VIRT(SandboxSystemTable);

  Status = EFI_SUCCESS;
  return Status;
}

VOID FreeSandboxSystemTable(IN UEFI_SANDBOX *Sandbox) {
  VOID *BufferBase;
  UINTN BufferSize;
  EFI_SYSTEM_TABLE *SandboxSystemTable;

  BufferBase = (VOID *)VIRT_TO_PHYS(Sandbox->ImageData.Info.SystemTable);
  SandboxSystemTable = (EFI_SYSTEM_TABLE *)BufferBase;

  BufferSize = sizeof(EFI_SYSTEM_TABLE) + sizeof(EFI_BOOT_SERVICES) +
               sizeof(EFI_RUNTIME_SERVICES) +
               (sizeof(EFI_CONFIGURATION_TABLE) *
                SandboxSystemTable->NumberOfTableEntries);
  BufferSize = ROUND_UP(BufferSize, PAGE_SIZE);

  FreePages(BufferBase, BufferSize / PAGE_SIZE);
}
