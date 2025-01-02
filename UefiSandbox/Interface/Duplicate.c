#include "Duplicate.h"
#include "Interface/Interface.h"
#include "Library/BaseCounterLib/Counter.h"
#include "Library/BaseMemoryLib.h"
#include "Library/DebugLib.h"
#include "Library/MemoryAllocationLib.h"
#include "Library/UefiRuntimeServicesTableLib.h"
#include "Memory/Malloc.h"
#include "Memory/Memory.h"
#include "PointerList.h"
#include "Print.h"
#include "Uefi/UefiBaseType.h"
#include "UefiSandbox.h"

// Duplicate and replace the inner field of a custom type
STATIC EFI_STATUS EagerDuplicateTypePointer(IN DUPLICATE_CTX *Ctx,
                                            IN CONST EFI_VIRTUAL_ADDRESS Src,
                                            OUT EFI_VIRTUAL_ADDRESS *Dst);

STATIC BOOLEAN SpeculateSizeField(IN CONST CHAR8 *TargetName,
                                  IN CONST CHAR8 *SizeName) {
  UINTN FieldLen = AsciiStrLen(TargetName);
  if (AsciiStrLen(SizeName) < FieldLen) {
    return FALSE;
  }

  if (AsciiStrnCmp(TargetName, SizeName, FieldLen) != 0) {
    return FALSE;
  }

  return AsciiStrCmp(SizeName + FieldLen, "Length") == 0 ||
         AsciiStrCmp(SizeName + FieldLen, "Size") == 0 ||
         AsciiStrCmp(SizeName + FieldLen, "Count") == 0;
}

UINT64 SpeculateTypeFieldArraySize(IN CONST EFI_VIRTUAL_ADDRESS VirtTypeBase,
                                   IN CONST REFLECT_TYPE *Type,
                                   CONST CHAR8 *FieldName) {
  EFI_PHYSICAL_ADDRESS PhysTypeBase;
  REFLECT_FIELD *Field;
  LIST_ENTRY *Link;
  UINT64 Size;

  PhysTypeBase = TO_PHYS_ADDR(VirtTypeBase);
  Size = 1;
  BASE_LIST_FOR_EACH(Link, &Type->CustomType->Fields) {
    Field = BASE_CR(Link, REFLECT_FIELD, FieldNode);
    if (SpeculateSizeField(FieldName, Field->FieldName)) {
      ASSERT(Field->FieldType->Kind == BasicTypeKind);
      ASSERT(Field->FieldType->BasicType->TypeSize <= 8);
      CopyMem(&Size, (VOID *)(PhysTypeBase + Field->Offset),
              Field->FieldType->BasicType->TypeSize);
      break;
    }
  }
  return Size;
}

UINT64
SpeculateProtocolFieldArraySize(IN CONST EFI_VIRTUAL_ADDRESS VirtTypeBase,
                                IN CONST REFLECT_PROTOCOL *Protocol,
                                CONST CHAR8 *FieldName) {
  EFI_PHYSICAL_ADDRESS PhysTypeBase;
  REFLECT_PROTOCOL_FIELD *Field;
  LIST_ENTRY *Link;
  UINT64 Size;

  PhysTypeBase = TO_PHYS_ADDR(VirtTypeBase);
  Size = 1;
  BASE_LIST_FOR_EACH(Link, &Protocol->FieldsList) {
    Field = BASE_CR(Link, REFLECT_PROTOCOL_FIELD, ProtocolFieldNode);
    if (Field->IsFunction)
      continue;
    if (SpeculateSizeField(FieldName, Field->Variable->VariableName)) {
      ASSERT(Field->Variable->VariableType->Kind == BasicTypeKind);
      ASSERT(Field->Variable->VariableType->BasicType->TypeSize <= 8);
      CopyMem(&Size, (VOID *)(PhysTypeBase + Field->Offset),
              Field->Variable->VariableType->BasicType->TypeSize);
      break;
    }
  }
  return Size;
}

STATIC UINTN SpeculateFunctionParamArraySize(
    IN CONST UINT64 *Params, IN CONST REFLECT_FUNC_TYPE *Function,
    IN CONST REFLECT_PARAM *PointerParam) {
  LIST_ENTRY *Link;
  REFLECT_PARAM *Param;
  UINT64 Size = 1;
  UINTN Index = 0;
  BASE_LIST_FOR_EACH(Link, &Function->FunctionParams) {
    Param = BASE_CR(Link, REFLECT_PARAM, ParamNode);
    if (SpeculateSizeField(PointerParam->ParamName, Param->ParamName)) {
      ASSERT(Param->ParamType->Kind == BasicTypeKind);
      ASSERT(Param->ParamType->BasicType->TypeSize <= 8);
      if (Param->PointerLevel != 0) {
        ASSERT(Param->PointerLevel == 1);
        CopyMem(&Size, (VOID *)Params[Index],
                Param->ParamType->BasicType->TypeSize);
      } else {

        CopyMem(&Size, (VOID *)&(Params[Index]),
                Param->ParamType->BasicType->TypeSize);
      }
      break;
    }
    Index++;
  }
  return Size;
}

EFI_STATUS EagerDuplicateCustomTypeField(
    IN DUPLICATE_CTX *Ctx, IN CONST EFI_VIRTUAL_ADDRESS VirtTypeBase,
    IN CONST EFI_VIRTUAL_ADDRESS Offset, IN CONST UINTN TypeArraySize,
    IN CONST UINTN SpeculatedArraySize, IN CONST UINTN PointerLevel) {

  EFI_PHYSICAL_ADDRESS PhysTypeBase, PhysFieldSrc, PhysFieldDst, Cursor,
      ElemCursor;
  EFI_VIRTUAL_ADDRESS VirtFieldSrc, VirtFieldDst, VirtFieldElemSrc,
      VirtFieldElemDst;
  CONST REFLECT_TYPE *FieldType;
  BOOLEAN InUnion, Syncable;
  UINTN ArraySize, MemSize;
  UEFI_SANDBOX *Owner;
  FieldType = Ctx->CurrentType;
  PhysTypeBase = TO_PHYS_ADDR(VirtTypeBase);
  Owner = Ctx->PointerList->Owner;
  InUnion = Ctx->InUnion;
  Syncable = Ctx->Syncable;
  Cursor = PhysTypeBase + Offset;
  ArraySize = SpeculatedArraySize;
  if (PointerLevel > 0) {

    // We can not handle pointer types that are already in union because the
    // callee side code is not transparent to us. So we basically do not know
    // which field will be used. Multiple pointers may collide with each
    // other.
    //
    ASSERT(!Ctx->InUnion);
    // Current Types is an array of pointer;
    if (ArraySize != 0) {
      Ctx->InUnion = InUnion || (FieldType->Kind == CustomTypeKind &&
                                 !FieldType->CustomType->IsStruct);
      for (UINTN i = 0; i < ArraySize; i++) {

        // VirtDstBase + Field->Offset is &PointerArray[0]
        // Since there's no indirect references we just need dereference it
        // once.
        ElemCursor = Cursor + i * sizeof(EFI_VIRTUAL_ADDRESS);
        VirtFieldElemSrc = *(EFI_VIRTUAL_ADDRESS *)(ElemCursor);
        ASSERT(EagerDuplicateTypeMultiPointer(Ctx, VirtFieldElemSrc,
                                              &VirtFieldElemDst,
                                              PointerLevel) == EFI_SUCCESS);
        // Write the new pointer to the destination of the original struct.
        *(EFI_VIRTUAL_ADDRESS *)(ElemCursor) = VirtFieldElemDst;
      }
      // Not an explicit array of pointers but might still refer to a size
      // field to represent the array size. E.g. int **array, and int
      // arraySize is a pair.
    } else {
      VirtFieldSrc = *(EFI_VIRTUAL_ADDRESS *)(PhysTypeBase + Offset);
      PhysFieldSrc = TO_PHYS_ADDR(VirtFieldSrc);
      // If the field is only a single level of pointer. We might refer it as
      // a simple array.
      //
      if (PointerLevel == 1) {
        // Basic Type we don't need to do deepcopy.
        if (FieldType->Kind == BasicTypeKind) {

          MemSize = FieldType->BasicType->TypeSize * ArraySize;
          PhysFieldDst = (EFI_PHYSICAL_ADDRESS)AllocateSandboxMemory(
              Owner, ArraySize * FieldType->BasicType->TypeSize);
          CopyMem((VOID *)PhysFieldDst, (VOID *)PhysFieldSrc, MemSize);

        } else {
          // Not a basic type but a custom type. We need to do recursive deep
          // copy.
          MemSize = ArraySize * FieldType->CustomType->TypeSize;
          PhysFieldDst =
              (EFI_PHYSICAL_ADDRESS)AllocateSandboxMemory(Owner, MemSize);
          CopyMem((VOID *)PhysFieldDst, (VOID *)PhysFieldSrc, MemSize);
          Ctx->CurrentType = FieldType;
          Ctx->InUnion = InUnion || (FieldType->Kind == CustomTypeKind &&
                                     !FieldType->CustomType->IsStruct);

          for (UINTN i = 0; i < ArraySize; i++) {
            VirtFieldElemDst = TO_VIRT_ADDR(PhysFieldDst) +
                               i * FieldType->CustomType->TypeSize;
            ASSERT(EagerDuplicateCustomType(Ctx, VirtFieldElemDst) ==
                   EFI_SUCCESS);
          }
        }
        // Record the pointer for future synchronization and garbage
        // collection.
        Syncable = Ctx->Syncable;

      } else {

        MemSize = ArraySize * sizeof(EFI_VIRTUAL_ADDRESS);
        // Now this is a multi-level pointers with a pair of array size.
        PhysFieldDst =
            (EFI_PHYSICAL_ADDRESS)AllocateSandboxMemory(Owner, MemSize);

        for (UINTN i = 0; i < ArraySize; i++) {
          Ctx->CurrentType = FieldType;
          Ctx->InUnion = InUnion || (FieldType->Kind == CustomTypeKind &&
                                     !FieldType->CustomType->IsStruct);
          // This is the most complicated part. We need to recursively
          // duplicate the multi-level pointers. The first dereference is done
          // for basically get the pointer address. Then it boils down to the
          // arrays by incrementing the pointer by i * sizeof(VOID *) The
          // casting path is following: takes int **Array as Example
          // (VirtDstBase + Field->Offset) = &Array
          // *(VirtDstBase + Field->Offset) = Array
          // *(*(VirtualDstBase + Field->Offset) + i * sizeof(VOID *)) =
          // Array[i]
          // Then the level is decreased by 1 and we do the same thing again.
          ElemCursor = PhysFieldSrc + i * sizeof(EFI_VIRTUAL_ADDRESS);
          VirtFieldElemSrc = *(EFI_VIRTUAL_ADDRESS *)(ElemCursor);
          ASSERT(EagerDuplicateTypeMultiPointer(
                     Ctx, VirtFieldElemSrc, &VirtFieldElemDst,
                     PointerLevel - 1) == EFI_SUCCESS);
          ElemCursor = PhysFieldDst + i * sizeof(EFI_VIRTUAL_ADDRESS);
          *(EFI_VIRTUAL_ADDRESS *)(ElemCursor) = VirtFieldElemDst;
        }
        Syncable = FALSE;
      }
      VirtFieldDst = TO_VIRT_ADDR(PhysFieldDst);
      *(EFI_VIRTUAL_ADDRESS *)(Cursor) = VirtFieldDst;
      InsertPointerRecordList(Ctx->PointerList, FieldType, VirtFieldSrc,
                              VirtFieldDst, Cursor, MemSize, Syncable);
    }
  } else {
    // No Pointer present but we still should do recursive duplicating for
    // custom types.
    if (FieldType->Kind == CustomTypeKind) {
      Ctx->CurrentType = FieldType;
      Ctx->InUnion = InUnion || (FieldType->Kind == CustomTypeKind &&
                                 !FieldType->CustomType->IsStruct);
      // If it's only a typical struct, we should do deep copy.
      if (ArraySize == 0) {
        ASSERT(
            EagerDuplicateCustomType(Ctx, TO_VIRT_ADDR(Cursor) == EFI_SUCCESS));
      } else {
        // For array of custom types, we should do deep copy for each element.
        for (UINTN i = 0; i < ArraySize; i++) {
          ASSERT(EagerDuplicateCustomType(
                     Ctx, TO_VIRT_ADDR(Cursor +
                                       i * FieldType->CustomType->TypeSize)) ==
                 EFI_SUCCESS);
        }
      }
    }
  }
  return EFI_SUCCESS;
}

EFI_STATUS EagerDuplicateCustomType(IN DUPLICATE_CTX *Ctx,
                                    IN EFI_VIRTUAL_ADDRESS VirtTypeBase) {

  CONST REFLECT_TYPE *Type;
  LIST_ENTRY *Link;
  REFLECT_FIELD *Field;
  Type = Ctx->CurrentType;
  BASE_LIST_FOR_EACH(Link, &Type->CustomType->Fields) {
    Field = BASE_CR(Link, REFLECT_FIELD, FieldNode);
    // This means that the current field is at least a single level pointer
    // type,
    Ctx->CurrentType = Field->FieldType;
    if (Field->ArraySize != 0)
      EagerDuplicateCustomTypeField(Ctx, VirtTypeBase, Field->Offset,
                                    Field->ArraySize, 0, Field->PointerLevel);
    else
      EagerDuplicateCustomTypeField(
          Ctx, VirtTypeBase, Field->Offset, 0,
          SpeculateTypeFieldArraySize(VirtTypeBase, Type, Field->FieldName),
          Field->PointerLevel);
  }
  return EFI_SUCCESS;
}

// Duplicate A ReflectType From Src to Dst. Src is the address of a CustomType.
STATIC EFI_STATUS EagerDuplicateTypePointer(IN DUPLICATE_CTX *Ctx,
                                            IN CONST EFI_VIRTUAL_ADDRESS Src,
                                            OUT EFI_VIRTUAL_ADDRESS *Dst) {

  EFI_PHYSICAL_ADDRESS NextPhysBase;
  CONST REFLECT_TYPE *Type;
  UEFI_SANDBOX *Owner;
  Owner = Ctx->PointerList->Owner;

  if (Src == 0) {
    return Ctx->Optional ? EFI_SUCCESS : EFI_INVALID_PARAMETER;
  }

  Type = Ctx->CurrentType;
  NextPhysBase = (EFI_PHYSICAL_ADDRESS)AllocateSandboxMemory(
      Owner, Ctx->CurrentType->CustomType->TypeSize);

  CopyMem((VOID *)NextPhysBase, (VOID *)(TO_PHYS_ADDR(Src)),
          Ctx->CurrentType->CustomType->TypeSize);
  *(Dst) = TO_VIRT_ADDR(NextPhysBase);
  ASSERT(EagerDuplicateCustomType(Ctx, TO_VIRT_ADDR(NextPhysBase)) ==
         EFI_SUCCESS);

  InsertPointerRecordList(Ctx->PointerList, Type, Src, *Dst, (UINT64)Dst,
                          Type->CustomType->TypeSize, Ctx->Syncable);

  return EFI_SUCCESS;
}
// Recursively duplicate a multi-level pointer type. Note that this multi-level
// pointer is int **Elem not int *Elem[]
EFI_STATUS EagerDuplicateTypeMultiPointer(IN DUPLICATE_CTX *Ctx,
                                          IN CONST EFI_VIRTUAL_ADDRESS Src,
                                          OUT EFI_VIRTUAL_ADDRESS *Dst,
                                          IN CONST UINTN PointerLevel) {

  EFI_VIRTUAL_ADDRESS NextSrc, NextDst;
  EFI_VIRTUAL_ADDRESS *Cursor;
  UEFI_SANDBOX *Owner;
  CONST REFLECT_TYPE *Type;
  EFI_STATUS Status = 0;

  Type = Ctx->CurrentType;
  Owner = Ctx->PointerList->Owner;

  if (Src == 0) {
    return Ctx->Optional ? EFI_SUCCESS : EFI_INVALID_PARAMETER;
  }

  if (PointerLevel > 1) {

    NextSrc = *(EFI_VIRTUAL_ADDRESS *)(TO_PHYS_ADDR(Src));
    Status = EagerDuplicateTypeMultiPointer(Ctx, NextSrc, &NextDst,
                                            PointerLevel - 1);
    if (EFI_ERROR(Status)) {
      return Status;
    }
    Cursor = (EFI_VIRTUAL_ADDRESS *)AllocateSandboxMemory(
        Owner, sizeof(EFI_VIRTUAL_ADDRESS *));
    *Cursor = NextDst;

    (*Dst) = TO_VIRT_ADDR((EFI_VIRTUAL_ADDRESS)Cursor);
    InsertPointerRecordList(Ctx->PointerList, Type, NextSrc, NextDst,
                            (UINT64)Dst, sizeof(EFI_VIRTUAL_ADDRESS), FALSE);
    return EFI_SUCCESS;

  } else {
    switch (Type->Kind) {
    case BasicTypeKind:
      *Dst = (EFI_PHYSICAL_ADDRESS)AllocateSandboxMemory(
          Owner, Type->BasicType->TypeSize);
      CopyMem(Dst, (VOID *)Src, Type->BasicType->TypeSize);
      *Dst = TO_VIRT_ADDR(*Dst);
      InsertPointerRecordList(Ctx->PointerList, Type, Src, *Dst, (UINT64)Dst,
                              Type->BasicType->TypeSize, Ctx->Syncable);
      break;
    case CustomTypeKind:
      Status = EagerDuplicateTypePointer(Ctx, Src, Dst);
      break;
    default:
      ASSERT(FALSE);
    }
  }
  return Status;
}

STATIC VOID CopyOneCallParam(IN DUPLICATE_CTX *Ctx,
                             IN CONST REFLECT_FUNC_TYPE *Function,
                             IN CONST REFLECT_PARAM *Param,
                             IN CONST UINT64 *Src, OUT UINT64 *Dst,
                             IN CONST UINT64 Index, IN CONST BOOLEAN Alloc) {

  EFI_VIRTUAL_ADDRESS NextVirtDst;
  EFI_PHYSICAL_ADDRESS NextPhysDst;
  UINTN ArraySize, MemSize;
  UEFI_SANDBOX *Owner;
  BOOLEAN Syncable;

  Owner = Ctx->PointerList->Owner;

  if (AsciiStrStr(Param->ParamName, "Str") != NULL ||
      AsciiStrCmp(Param->ParamName, "FileName") == 0) {
    ASSERT(Param->ParamType->Kind == BasicTypeKind);
    if (AsciiStrCmp(Param->ParamType->BasicType->TypeName, "CHAR16") == 0) {
      MemSize = StrSize((CHAR16 *)TO_PHYS_ADDR(Src[Index]));
      Dst[Index] = TO_VIRT_ADDR((EFI_PHYSICAL_ADDRESS)AllocateSandboxMemory(
          Ctx->PointerList->Owner, MemSize));
      CopyMem((VOID *)TO_PHYS_ADDR(Dst[Index]), (VOID *)Src[Index], MemSize);

    } else if (AsciiStrCmp(Param->ParamType->BasicType->TypeName, "CHAR8") ==
               0) {

      MemSize = AsciiStrSize((CHAR8 *)TO_PHYS_ADDR(Src[Index]));
      Dst[Index] = TO_VIRT_ADDR((EFI_PHYSICAL_ADDRESS)AllocateSandboxMemory(
          Ctx->PointerList->Owner, MemSize));
      CopyMem((VOID *)TO_PHYS_ADDR(Dst[Index]), (VOID *)Src[Index], MemSize);

    } else {
      __unimplemented("Str Type: %a\n", Param->ParamType->BasicType->TypeName);
    }
    Syncable = Ctx->Syncable;
    InsertPointerRecordList(Ctx->PointerList, Param->ParamType, Src[Index],
                            Dst[Index], (UINT64)&Dst[Index], MemSize, Syncable);
    return;
  }

  ArraySize = SpeculateFunctionParamArraySize(Src, Function, Param);

  if (Param->PointerLevel == 1) {

    if (Param->ParamType->Kind == BasicTypeKind) {
      MemSize = ArraySize * Param->ParamType->BasicType->TypeSize;
      NextPhysDst = (EFI_PHYSICAL_ADDRESS)AllocateSandboxMemory(Owner, MemSize);

      CopyMem((VOID *)NextPhysDst, (VOID *)TO_PHYS_ADDR(Src[Index]), MemSize);

      NextVirtDst = TO_VIRT_ADDR(NextPhysDst);

    } else {

      MemSize = ArraySize * Param->ParamType->CustomType->TypeSize;
      NextPhysDst = (EFI_PHYSICAL_ADDRESS)AllocateSandboxMemory(Owner, MemSize);

      NextVirtDst = TO_VIRT_ADDR(NextPhysDst);
      CopyMem((VOID *)NextPhysDst, (VOID *)TO_PHYS_ADDR(Src[Index]), MemSize);
      Ctx->CurrentType = Param->ParamType;
      Ctx->InUnion = (Param->ParamType->Kind == CustomTypeKind &&
                      !Param->ParamType->CustomType->IsStruct);

      for (UINTN i = 0; i < ArraySize; i++) {
        ASSERT(EagerDuplicateCustomType(
                   Ctx,
                   NextVirtDst + i * Param->ParamType->CustomType->TypeSize) ==
               EFI_SUCCESS);
      }
    }
    Syncable = Ctx->Syncable;
  } else {

    Ctx->CurrentType = Param->ParamType;
    Ctx->InUnion = (Param->ParamType->Kind == CustomTypeKind &&
                    !Param->ParamType->CustomType->IsStruct);

    MemSize = ArraySize * sizeof(EFI_VIRTUAL_ADDRESS);
    NextPhysDst = (EFI_PHYSICAL_ADDRESS)AllocateSandboxMemory(Owner, MemSize);

    for (UINTN i = 0; i < ArraySize; i++) {
      ASSERT(EagerDuplicateTypeMultiPointer(
                 Ctx,
                 *(EFI_VIRTUAL_ADDRESS *)(Src[Index] +
                                          i * sizeof(EFI_VIRTUAL_ADDRESS)),
                 &NextVirtDst, Param->PointerLevel - 1) == EFI_SUCCESS);
      *(EFI_VIRTUAL_ADDRESS *)(NextPhysDst + i * sizeof(EFI_VIRTUAL_ADDRESS)) =
          NextVirtDst;
    }
    Syncable = FALSE;
  }
  Dst[Index] = TO_VIRT_ADDR(NextPhysDst);
  InsertPointerRecordList(Ctx->PointerList, Param->ParamType, Src[Index],
                          Dst[Index], (UINT64)&Dst[Index], MemSize, Syncable);
}

__attribute__((unused)) STATIC VOID AllocateSpaceForOneParam(
    IN DUPLICATE_CTX *Ctx, IN CONST REFLECT_FUNC_TYPE *Function,
    IN CONST REFLECT_PARAM *Param, IN CONST UINT64 *Src, OUT UINT64 *Dst,
    UINT64 Index) {

  UINTN ArraySize, MemSize;
  UEFI_SANDBOX *Owner;
  Owner = Ctx->PointerList->Owner;
  ArraySize = SpeculateFunctionParamArraySize(Src, Function, Param);
  if (Param->PointerLevel == 1) {
    if (Param->ParamType->Kind == BasicTypeKind)
      MemSize = ArraySize * Param->ParamType->BasicType->TypeSize;
    else
      MemSize = ArraySize * Param->ParamType->CustomType->TypeSize;
    Dst[Index] = (EFI_VIRTUAL_ADDRESS)TO_VIRT_ADDR(
        (EFI_PHYSICAL_ADDRESS)AllocateSandboxMemory(Owner, MemSize));
  } else {
    MemSize = ArraySize * sizeof(EFI_VIRTUAL_ADDRESS);
    Dst[Index] = (EFI_VIRTUAL_ADDRESS)AllocateSandboxMemory(Owner, MemSize);
  }
  InsertPointerRecordList(Ctx->PointerList, Param->ParamType, Src[Index],
                          Dst[Index], (UINT64)&Dst[Index], MemSize, FALSE);
}

VOID CopyInterfaceCallParams(IN DUPLICATE_CTX *Ctx, IN CONST VOID *Opaque,
                             IN CONST REFLECT_FUNC_TYPE *Function,
                             IN CONST UINT64 *Src, OUT UINT64 *Dst) {
  REFLECT_PARAM *Param;
  LIST_ENTRY *Link;
  UINTN Index;

#if SANDBOX_PERF_COPY_PARAMS
  UINTN Val1, Val2;
  Val1 = ReadCounter();
#endif

  Index = 0;
  BASE_LIST_FOR_EACH(Link, &Function->FunctionParams) {
    Param = BASE_CR(Link, REFLECT_PARAM, ParamNode);
    ASSERT(Param->OutParam || Param->InParam);
    if (Param->ParamType->Kind == ProtocolKind) {
      // Src[i] With Real Protocol Interface Counterpart find this protocol in
      // the protocol list;

      if (Param->InParam)
        Dst[Index] = (UINTN)Opaque;
      else {
        ASSERT(Param->PointerLevel == 2);
        Dst[Index] = TO_VIRT_ADDR((UINTN)AllocateSandboxMemory(
            Ctx->PointerList->Owner, sizeof(EFI_VIRTUAL_ADDRESS)));
        InsertPointerRecordList(Ctx->PointerList, NULL, Src[Index], Dst[Index],
                                (UINTN)&Dst[Index], sizeof(EFI_VIRTUAL_ADDRESS),
                                FALSE);
      }
    } else {
      if (Param->PointerLevel > 0) {

        if (Param->PointerLevel > 1 && Param->OutParam && !Param->InParam)
          __unimplemented();

        Ctx->Syncable = Param->OutParam;
        CopyOneCallParam(Ctx, Function, Param, Src, Dst, Index, TRUE);
      } else {
        Dst[Index] = Src[Index];
      }
    }
    Index++;
  }

#if SANDBOX_PERF_COPY_PARAMS
  Val2 = ReadCounter();
  SBPrint("Copy Params Time: %lu\n", Val2 - Val1);
#endif
}

STATIC EFI_STATUS AllocatePersistentLocatedInterface(
    IN UEFI_SANDBOX *CallerSandbox, IN UEFI_SANDBOX *CalleeSandbox,
    IN REFLECT_PROTOCOL *Protocol, IN CONST EFI_VIRTUAL_ADDRESS Delegated,
    OUT LOCATED_INTERFACE **Located) {

  LOCATED_INTERFACE *LocatedInterface;
  SANDBOX_INTERFACE *Sandboxed;

  LocatedInterface = AllocatePool(sizeof(*LocatedInterface));
  Sandboxed = AllocatePool(sizeof(*Sandboxed));

  ZeroMem(LocatedInterface, sizeof(*LocatedInterface));
  ZeroMem(Sandboxed, sizeof(*Sandboxed));

  Sandboxed->Opaque = (VOID *)Delegated;
  Sandboxed->Desc = Protocol;
  Sandboxed->SandboxID = CalleeSandbox->SandboxID;

  LocatedInterface->Desc = Protocol;
  LocatedInterface->SandboxID = CallerSandbox->SandboxID;
  LocatedInterface->Sandboxed = Sandboxed;
  InitPointerRecordList(&LocatedInterface->Magisk.PointerList, CallerSandbox);
  (*Located) = LocatedInterface;
  return CreateInterfaceMagisk(Protocol, Delegated, CallerSandbox == &CoreSandbox, &LocatedInterface->Magisk);
}

VOID SyncInterfaceCallParams(IN UEFI_SANDBOX *CallerSandbox,
                             IN UEFI_SANDBOX *CalleeSandbox,
                             IN CONST REFLECT_FUNC_TYPE *Function,
                             IN CONST UINT64 *Dst, OUT UINT64 *Src) {

  LOCATED_INTERFACE *Located;
  REFLECT_PARAM *Param;
  LIST_ENTRY *Link;
  UINTN Index;
  Index = 0;

  BASE_LIST_FOR_EACH(Link, &Function->FunctionParams) {
    Param = BASE_CR(Link, REFLECT_PARAM, ParamNode);
    if (Param->ParamType->Kind == ProtocolKind && Param->OutParam) {
      ASSERT(Param->PointerLevel == 2);
      ASSERT(Param->ParamType->Kind = ProtocolKind);
      ASSERT_EFI_ERROR(AllocatePersistentLocatedInterface(
          CallerSandbox, CalleeSandbox, Param->ParamType->Protocol,
          *(EFI_VIRTUAL_ADDRESS *)Dst[Index], &Located));
      *(VOID **)TO_PHYS_ADDR(Src[Index]) = Located->Magisk.Interface;
    }

    Index++;
  }
}
