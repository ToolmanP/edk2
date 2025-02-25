#include "Init/ArchInit.h"
#include "Gdt.h"
#include "Library/MemoryAllocationLib.h"
#include "Memory.h"
#include "Print.h"
#include "Register/Intel/ArchitecturalMsr.h"
#include "Tss.h"
#include "CpuInfo.h"
#include "Uefi/UefiBaseType.h"
#include "CpuDxe/CpuDxe.h"
#include "Library/BaseLib.h"
#include "Library/BaseMemoryLib.h"
#include "Library/DebugLib.h"
#include "Library/UefiBootServicesTableLib.h"
#include "Library/HobLib.h"

VOID *TssKernelStackBase = 0;

UINT64 CurrentCpuInfoIndex = 0;
CpuInfo mCpuInfos[7];

UINT8 TmpR12;
UINT8 TmpR13;
UINT8 TmpR14;

//
// Global descriptor table (GDT) Template
//
STATIC GDT_ENTRIES  mGdtTemplate = {
  //
  // NULL_SEL
  //
  {
    0x0,            // limit 15:0
    0x0,            // base 15:0
    0x0,            // base 23:16
    0x0,            // type
    0x0,            // limit 19:16, flags
    0x0,            // base 31:24
  },
  //
  // LINEAR_SEL
  //
  {
    0x0FFFF,        // limit 15:0
    0x0,            // base 15:0
    0x0,            // base 23:16
    0x092,          // present, ring 0, data, read/write
    0x0CF,          // page-granular, 32-bit
    0x0,
  },
  //
  // LINEAR_CODE_SEL
  //
  {
    0x0FFFF,        // limit 15:0
    0x0,            // base 15:0
    0x0,            // base 23:16
    0x09F,          // present, ring 0, code, execute/read, conforming, accessed
    0x0CF,          // page-granular, 32-bit
    0x0,
  },
  //
  // SYS_DATA_SEL
  //
  {
    0x0FFFF,        // limit 15:0
    0x0,            // base 15:0
    0x0,            // base 23:16
    0x093,          // present, ring 0, data, read/write, accessed
    0x0CF,          // page-granular, 32-bit
    0x0,
  },
  //
  // SYS_CODE_SEL
  //
  {
    0x0FFFF,        // limit 15:0
    0x0,            // base 15:0
    0x0,            // base 23:16
    0x09A,          // present, ring 0, code, execute/read
    0x0CF,          // page-granular, 32-bit
    0x0,
  },
  //
  // SYS_CODE16_SEL
  //
  {
    0x0FFFF,        // limit 15:0
    0x0,            // base 15:0
    0x0,            // base 23:16
    0x09A,          // present, ring 0, code, execute/read
    0x08F,          // page-granular, 16-bit
    0x0,            // base 31:24
  },
  //
  // LINEAR_DATA64_SEL
  //
  {
    0x0FFFF,        // limit 15:0
    0x0,            // base 15:0
    0x0,            // base 23:16
    0x092,          // present, ring 0, data, read/write
    0x0CF,          // page-granular, 32-bit
    0x0,
  },
  //
  // LINEAR_CODE64_SEL
  //
  {
    0x0FFFF,        // limit 15:0
    0x0,            // base 15:0
    0x0,            // base 23:16
    0x09A,          // present, ring 0, code, execute/read
    0x0AF,          // page-granular, 64-bit code
    0x0,            // base (high)
  },
  // UserData64
  {
    0x0FFFF,
    0x0,
    0x0,
    0x0F2,  // DPL=3, data, present
    0x0CF,  // 64-bit code
    0x0,
  },
  // UserCode64
  {
    0x0FFFF,
    0x0,
    0x0,
    0x0FA,  // DPL=3, 64-bit code, present
    0x0AF,
    0x0,
  },
  // TSS (initialized later)
  {
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
  },
  //
  // SPARE5_SEL
  //
  {
    0x0,            // limit 15:0
    0x0,            // base 15:0
    0x0,            // base 23:16
    0x0,            // type
    0x0,            // limit 19:16, flags
    0x0,            // base 31:24
  },
};

EFI_STATUS
FindStackInfoFromHob (
  OUT EFI_PHYSICAL_ADDRESS *StackBase,
  OUT UINT64               *StackSize
  )
{
  EFI_PEI_HOB_POINTERS Hob;
  Hob.Raw = GetHobList();
  for (; !END_OF_HOB_LIST (Hob); Hob.Raw = GET_NEXT_HOB (Hob)) {
    if (GET_HOB_TYPE(Hob) == EFI_HOB_TYPE_MEMORY_ALLOCATION) {
      EFI_HOB_MEMORY_ALLOCATION *MemAllocHob = Hob.MemoryAllocation;
      if (CompareGuid(&MemAllocHob->AllocDescriptor.Name, &gEfiHobMemoryAllocStackGuid)) {
        // ¿¿Stack HOB
        if (StackBase != NULL) {
          *StackBase = MemAllocHob->AllocDescriptor.MemoryBaseAddress;
        }
        if (StackSize != NULL) {
          *StackSize = MemAllocHob->AllocDescriptor.MemoryLength;
        }
        return EFI_SUCCESS;
      }
    }
  }
  return EFI_NOT_FOUND;
}


VOID CreateTss(UINT64 *TssBase, UINT32 *TssLimit)
{
  Tss64 *Tss;

  // ¿¿ TSS ¿ IOPB ¿¿¿
  Tss = (Tss64 *) AllocateZeroPool(sizeof(Tss64) + IOPB_SIZE + 1);

  TssKernelStackBase = AllocatePages(EFI_SIZE_TO_PAGES(DEFAULT_STACK_SIZE));
  if (TssKernelStackBase == 0) {
    SBPrint("Failed to allocate kernel stack\n");
    return;
  }

  // ¿¿ Rsp0 = ¿¿¿¿
  Tss->Rsp0 = (UINT64)TssKernelStackBase + DEFAULT_STACK_SIZE;

  // ¿¿¿ TSS
  Tss->IOPBOffset = sizeof(Tss64);   // IOPB ¿¿¿¿

  // ¿¿ IOPB ¿¿¿
  UINT8 *IOPB = (UINT8 *) ((UINT8 *) Tss + Tss->IOPBOffset);

  // ¿¿¿ IOPB
  SetMem(IOPB, IOPB_SIZE, 0xFF);  // ¿¿¿¿¿¿¿¿¿

  // ¿¿¿¿¿¿¿¿
  UINT16 ports_to_enable[] = {0x402};
  for (int i = 0; i < sizeof(ports_to_enable)/sizeof(UINT16); i++) {
      UINT16 port = ports_to_enable[i];
      IOPB[port / 8] &= ~(1 << (port % 8));
  }

  // ¿¿ IOPB ¿¿¿¿
  IOPB[IOPB_SIZE] = 0xFF;

  *TssBase = (UINT64) Tss;
  *TssLimit = sizeof(Tss64) + IOPB_SIZE;
}

/**
  Initialize Global Descriptor Table.

**/
VOID
InitGlobalDescriptorTable (
  VOID
  )
{
  EFI_STATUS            Status;
  GDT_ENTRIES           *Gdt;
  IA32_DESCRIPTOR       Gdtr;
  EFI_PHYSICAL_ADDRESS  Memory;

  //
  // Allocate Runtime Data below 4GB for the GDT
  // AP uses the same GDT when it's waken up from real mode so
  // the GDT needs to be below 4GB.
  //
  Memory = SIZE_4GB - 1;
  Status = gBS->AllocatePages (
                  AllocateMaxAddress,
                  EfiRuntimeServicesData,
                  EFI_SIZE_TO_PAGES (sizeof (mGdtTemplate)),
                  &Memory
                  );
  ASSERT_EFI_ERROR (Status);
  ASSERT ((Memory != 0) && (Memory < SIZE_4GB));
  Gdt = (GDT_ENTRIES *)(UINTN)Memory;

  //
  // Initialize all GDT entries
  //
  CopyMem (Gdt, &mGdtTemplate, sizeof (mGdtTemplate));


  UINT64 TssBase = 0;
  UINT32 TssLimit = 0;
  CreateTss(&TssBase, &TssLimit);

  TSS_ENTRY *mTssDesc = &Gdt->Tss;
  ZeroMem (mTssDesc, sizeof(TSS_ENTRY));

  mTssDesc->LimitLow    = (UINT16)(TssLimit & 0xFFFF);
  mTssDesc->BaseLow     = (UINT16)(TssBase & 0xFFFF);
  mTssDesc->BaseMid     = (UINT8)((TssBase >> 16) & 0xFF);
  mTssDesc->Access      = 0x89; // 10001001b => P=1, DPL=0, ¿¿=1001(64¿TSS)
  mTssDesc->Granularity = (UINT8)(((TssLimit >> 16) & 0x0F));
  mTssDesc->BaseHigh    = (UINT8)((TssBase >> 24) & 0xFF);
  mTssDesc->BaseUpper   = (UINT32)((TssBase >> 32) & 0xFFFFFFFF);
  mTssDesc->Reserved    = 0;

  //
  // Write GDT register
  //
  Gdtr.Base  = (UINT32)(UINTN)Gdt;
  Gdtr.Limit = (UINT16)(sizeof (mGdtTemplate) - 1);
  AsmWriteGdtr (&Gdtr);

  AsmWriteTr(TSS_SEL);
}

#define EFER_SCE        (1ULL << 0)  // Bit 0

#define EFLAGS_TF       (1 << 8)     // Trap Flag
#define EFLAGS_IF       (1 << 9)     // Interrupt Enable Flag

extern VOID SyscallEntry(VOID);

VOID
InitSyscallMsr ()
{
  UINT64 val;

  //
  // 1. ¿¿ syscall/sysret  (EFER.SCE = 1)
  //
  val = AsmReadMsr64(MSR_IA32_EFER);
  val |= EFER_SCE;
  AsmWriteMsr64(MSR_IA32_EFER, val);

  //
  // 2. ¿¿ STAR:
  //
  UINT64 star = (((USER_CODE_SEL - 16) << 48) | (KERNEL_CODE_SEL << 32));
  SBDebug("star = 0x%lx, USER_CODE_SEL = 0x%x, KERNEL_CODE_SEL = 0x%x\n", star, USER_CODE_SEL, KERNEL_CODE_SEL);
  AsmWriteMsr64(MSR_IA32_STAR, star);

  //
  // 3. ¿¿ LSTAR: SYSCALL ¿¿¿
  //
  AsmWriteMsr64(MSR_IA32_LSTAR, (UINT64)SyscallEntry);

  //
  // 4. ¿¿ FMASK
  //
  AsmWriteMsr64(MSR_IA32_FMASK, EFLAGS_TF | EFLAGS_IF);
}

EFI_STATUS ArchInit(VOID)
{
  VOID *StackBase;
  /* Use a new stack for every level of syscall */
  for (UINT8 i = 0; i < 7; i++) {
    StackBase = AllocatePages(EFI_SIZE_TO_PAGES(DEFAULT_STACK_SIZE));
    if (StackBase == NULL) {
      SBError("Fail to allocate memory for syscall kernel stack\n");
      return EFI_OUT_OF_RESOURCES;
    }

    mCpuInfos[i].KernelStackTop = (UINT64) StackBase + DEFAULT_STACK_SIZE;
    mCpuInfos[i].Context = AllocateZeroPool(sizeof(EFI_SYSTEM_CONTEXT_X64));
  }

  InitGlobalDescriptorTable();
  InitSyscallMsr();

  return EFI_SUCCESS;
}
