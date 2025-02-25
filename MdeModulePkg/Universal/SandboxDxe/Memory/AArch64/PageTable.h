#ifndef SANDBOX_MEMORY_AARCH64_H_
#define SANDBOX_MEMORY_AARCH64_H_

#include <Chipset/AArch64Mmu.h>

#define USER_BASE (UINT64)0xff0000000000

#define KERNEL_SYSTEM_DRAM_BASE (UINT64)0x40000000
#define KERNEL_SYSTEM_DRAM_SIZE (UINT64)0x100000000

#define VIRT_TO_PHYS(Virt)                                                     \
  ((EFI_PHYSICAL_ADDRESS)((EFI_VIRTUAL_ADDRESS)Virt - USER_BASE))
#define PHYS_TO_VIRT(Phys)                                                     \
  ((EFI_VIRTUAL_ADDRESS)((EFI_PHYSICAL_ADDRESS)Phys + USER_BASE))

#define PTR_VIRT_TO_PHYS(Ptr) ((VOID *)(VIRT_TO_PHYS((EFI_VIRTUAL_ADDRESS)Ptr)))
#define PTR_PHYS_TO_VIRT(Ptr)                                                  \
  ((VOID *)(PHYS_TO_VIRT((EFI_PHYSICAL_ADDRESS)Ptr)))

#define IS_VIRT_ADDR(Virt) (((EFI_VIRTUAL_ADDRESS)Virt >> 40) == 0xff)
#define IS_PHYS_ADDR(Phys) (!(IS_VIRT_ADDR(Phys)))

#define INNER_SHAREABLE (0x3)
/* Please search mair_el1 for these memory types. */
#define DEVICE_MEMORY (TT_ATTR_INDX_DEVICE_MEMORY >> 2)
#define NORMAL_MEMORY_WT (TT_ATTR_INDX_MEMORY_WRITE_THROUGH >> 2)
#define NORMAL_MEMORY_WB (TT_ATTR_INDX_MEMORY_WRITE_BACK >> 2)
#define NORMAL_MEMORY_NON_CACHEABLE (TT_ATTR_INDX_MEMORY_NON_CACHEABLE >> 2)

/* Description bits in page table entries. */

/* Read-write permission. */
#define AARCH64_MMU_ATTR_PAGE_AP_HIGH_RW_EL0_NONE (0)
#define AARCH64_MMU_ATTR_PAGE_AP_HIGH_RW_EL0_RW (1)
#define AARCH64_MMU_ATTR_PAGE_AP_HIGH_RO_EL0_NONE (2)
#define AARCH64_MMU_ATTR_PAGE_AP_HIGH_RO_EL0_RO (3)

/* X: execution permission. U: unprivileged. P: privileged. */
#define AARCH64_MMU_ATTR_PAGE_UX (0)
#define AARCH64_MMU_ATTR_PAGE_UXN (1)
#define AARCH64_MMU_ATTR_PAGE_PXN (1)

/* Access flag bit. */
#define AARCH64_MMU_ATTR_PAGE_AF_ACCESSED (1)

/* Non-secure bit */
#define AARCH64_MMU_ATTR_PAGE_NS_NON_SECURE (1)

typedef union {
  struct {
    UINT64 is_valid : 1, is_table : 1, ignored1 : 10, next_table_addr : 36,
        reserved : 4, ignored2 : 7,
        PXNTable : 1, // Privileged Execute-never for next level
        XNTable : 1,  // Execute-never for next level
        APTable : 2,  // Access permissions for next level
        NSTable : 1;
  } table;
  struct {
    UINT64 is_valid : 1, is_table : 1,
        attr_index : 3, // Memory attributes index
        NS : 1,         // Non-secure
        AP : 2,         // Data access permissions
        SH : 2,         // Shareability
        AF : 1,         // Accesss flag
        nG : 1,         // Not global bit
        reserved1 : 4, nT : 1, reserved2 : 13, pfn : 18, reserved3 : 2, GP : 1,
        reserved4 : 1,
        DBM : 1, // Dirty bit modifier
        Contiguous : 1,
        PXN : 1, // Privileged execute-never
        UXN : 1, // Execute never
        soft_reserved : 4,
        PBHA : 4; // Page based hardware attributes
  } l1_block;
  struct {
    UINT64 is_valid : 1, is_table : 1,
        attr_index : 3, // Memory attributes index
        NS : 1,         // Non-secure
        AP : 2,         // Data access permissions
        SH : 2,         // Shareability
        AF : 1,         // Accesss flag
        nG : 1,         // Not global bit
        reserved1 : 4, nT : 1, reserved2 : 4, pfn : 27, reserved3 : 2, GP : 1,
        reserved4 : 1,
        DBM : 1, // Dirty bit modifier
        Contiguous : 1,
        PXN : 1, // Privileged execute-never
        UXN : 1, // Execute never
        soft_reserved : 4,
        PBHA : 4; // Page based hardware attributes
  } l2_block;
  struct {
    UINT64 is_valid : 1, is_page : 1,
        attr_index : 3, // Memory attributes index
        NS : 1,         // Non-secure
        AP : 2,         // Data access permissions
        SH : 2,         // Shareability
        AF : 1,         // Accesss flag
        nG : 1,         // Not global bit
        pfn : 36, reserved : 3,
        DBM : 1, // Dirty bit modifier
        Contiguous : 1,
        PXN : 1, // Privileged execute-never
        UXN : 1, // Execute never
        soft_reserved : 4,
        PBHA : 4, // Page based hardware attributes
        ignored : 1;
  } l3_page;
  UINT64 PTE;
} PTE_T;

#endif
