#ifndef UEFI_SANDBOX_H_
#define UEFI_SANDBOX_H_

#include <Library/BaseLib.h>
#include <Library/UefiLib.h>
#include <Protocol/Sandbox.h>

typedef struct {
  EFI_PHYSICAL_ADDRESS StackBase;
} UefiSandboxContext;

struct SandboxMallocManager;

typedef enum SandboxStatus SandboxStatus;
typedef struct {
  LIST_ENTRY SandboxListNode;
  UINTN SandboxID;
  /* Sandbox Context */
  UefiSandboxContext Context;
  /* Pointer to pool allocation for context save/restore */
  VOID *JumpBuffer;
  /* Pointer to buffer for context save/restore */
  BASE_LIBRARY_JUMP_BUFFER *JumpContext;
  /* Image Data */
  EFI_SANDBOX_IMAGE_DATA ImageData;
  /* VMSpace related metadata */
  UINT64 TranslationTable;
  /* List of VMRegions */
  LIST_ENTRY VMRegions;
  /* List of Interfaces installed by this sandbox */
  LIST_ENTRY InstalledInterfaces;
  /* List of used interfaces */
  LIST_ENTRY LocatedInterfaces;
  /* List of trampoline contexts*/
  LIST_ENTRY SVCTrampolineList;
  /* Lock for trampoline context */
  EFI_LOCK SVCTrampolineLock;
  /* Interface Magisk List */
  LIST_ENTRY InterfaceMagiskList;
  /* Memory Management */
  struct SandboxMallocManager *MallocManager;

} UefiSandbox;

typedef UefiSandbox UEFI_SANDBOX;
typedef struct SandboxMallocManager UEFI_SANDBOX_MALLOC_MANAGER;

extern UefiSandbox *CurrentSandbox;
extern UefiSandbox CoreSandbox;

UefiSandbox *FindSandbox(UINTN SandboxID);

#define SANDBOX_PERF_INTERFACE_CALL 0
#define SANDBOX_PERF_COPY_PARAMS 0

#endif
