#ifndef __INTERFACE_H__
#define __INTERFACE_H__

#include <Library/BaseLib.h>
#include <Interface/Magisk.h>

typedef struct {
  /* Handle of the Designated Handle */
  VOID *Handle;
  /* Pointer of the sandbox Interface */
  VOID *Opaque;
  /* RefCount */
  UINTN RefCount;
  /* OpenCount */
  UINTN OpenCount;
  /* SandboxID of the driver that implements the protocol*/
  UINTN SandboxID;

  const REFLECT_PROTOCOL *Desc;

  LIST_ENTRY LocatedList;

} SandboxInterface;

typedef SandboxInterface SANDBOX_INTERFACE;

#if defined (__x86_64__)
typedef struct {
  UINT64 R0;
  UINT64 R1;
  UINT64 R2;
  UINT64 R3;
  UINT64 R4;
  UINT64 R5;
  UINT64 RCX;  // Function Entrypoint
  UINT64 RDX;  // Return Trampoline
  UINT64 RSP;  // User Stack
} InterfaceParams;

#elif defined (__aarch64__)
typedef struct {
  /*General Perpose */
  UINT64 X0;
  UINT64 X1;
  UINT64 X2;
  UINT64 X3;
  UINT64 X4;
  UINT64 X5;
  UINT64 X6;
  UINT64 X7;
  /*Procedural */
  UINT64 LR;
  UINT64 SP;
  /*System Registers */
  UINT64 ELR;
  UINT64 SPSR;
} InterfaceParams;
#endif

typedef InterfaceParams INTERFACE_PARAMS;

typedef struct {
  INTERFACE_PARAMS EntryParams;
  VOID *JumpBuffer;
  BASE_LIBRARY_JUMP_BUFFER *JumpContext;
  EFI_PHYSICAL_ADDRESS StackBase;
  /* Return Trampoline */
  VOID *ReturnTrampoline;
  UEFI_SANDBOX *CallerSandbox, *CalleeSandbox;
} InterfaceContext;

typedef InterfaceContext INTERFACE_CONTEXT;

typedef struct {
  /* Used Interface by LocateProtocol/HandleProtocol Call*/
  LIST_ENTRY SandboxNode;

  LIST_ENTRY RegistryNode;
  /* Is it a Sandbox Interface */
  INTERFACE_MAGISK Magisk;
  SANDBOX_INTERFACE *Sandboxed;
  /*Protocol ID*/
  CONST EFI_GUID *ID;
  /* Protocol Entry */
  const REFLECT_PROTOCOL *Desc;
  /* Protocol Entry */
  EFI_HANDLE AgentHandle;
  EFI_HANDLE ControllerHandle;
  UINT32 Attributes;

  UINTN SandboxID;
  BOOLEAN Used;
} LocatedInterface;

typedef LocatedInterface LOCATED_INTERFACE;

#endif
