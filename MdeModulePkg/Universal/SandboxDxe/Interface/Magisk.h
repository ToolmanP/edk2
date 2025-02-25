#ifndef __MAGISK_H__
#define __MAGISK_H__

#include <Interface/PointerList.h>
#include <Interface/Reflect.h>

typedef struct {
  /* The Magisk of the Delegated Interface Instance*/
  VOID *Interface;
  /*Pointer Record List for Recording Magisk Duplication*/
  POINTER_LIST PointerList;
} INTERFACE_MAGISK;

EFI_STATUS CreateInterfaceMagisk(IN REFLECT_PROTOCOL *Protocol,
                                 IN CONST EFI_VIRTUAL_ADDRESS Delegated,
                                 IN BOOLEAN ForCore,
                                 IN OUT INTERFACE_MAGISK *Magisk);
EFI_STATUS FreeInterfaceMagisk(IN CONST EFI_GUID *ProtocolID,
                               IN OUT INTERFACE_MAGISK *Magisk);

EFI_STATUS SyncInterfaceMagisk(IN INTERFACE_MAGISK *Magisk);

#endif
