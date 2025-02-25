#ifndef PAGE_TABLE_H_
#define PAGE_TABLE_H_

#define USER_BASE (UINT64)0x7f0000000000

#define VIRT_TO_PHYS(Virt)                                                     \
  ((EFI_PHYSICAL_ADDRESS)((EFI_VIRTUAL_ADDRESS)Virt - USER_BASE))
#define PHYS_TO_VIRT(Phys)                                                     \
  ((EFI_VIRTUAL_ADDRESS)((EFI_PHYSICAL_ADDRESS)Phys + USER_BASE))

#define PTR_VIRT_TO_PHYS(Ptr) ((VOID *)(VIRT_TO_PHYS((EFI_VIRTUAL_ADDRESS)Ptr)))
#define PTR_PHYS_TO_VIRT(Ptr)                                                  \
  ((VOID *)(PHYS_TO_VIRT((EFI_PHYSICAL_ADDRESS)Ptr)))

#define IS_VIRT_ADDR(Virt) (((EFI_VIRTUAL_ADDRESS)Virt >> 40) == 0x7f)
#define IS_PHYS_ADDR(Phys) (!(IS_VIRT_ADDR(Phys)))

#endif
