#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>

#include <BinaryGen/BinaryGen.h>
#include <Interface/Duplicate.h>
#include <Interface/Interface.h>
#include <Interface/PointerList.h>
#include <Memory/Malloc.h>
#include <Memory/Memory.h>
#include <SandboxDxe.h>
#include <Utils/Logger.h>

// Duplicate and replace the inner field of a custom type
STATIC EFI_STATUS EagerDuplicateTypePointer(IN DUPLICATE_CTX *Ctx,
                                            IN CONST EFI_VIRTUAL_ADDRESS Src,
                                            OUT EFI_VIRTUAL_ADDRESS *Dst);

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
  UEFI_SANDBOX *DstSandbox;
  EFI_STATUS Status;
  FieldType = Ctx->CurrentType;
  PhysTypeBase = TO_PHYS_ADDR(VirtTypeBase);
  DstSandbox = Ctx->PointerList->DstSandbox;
  InUnion = Ctx->InUnion;
  Syncable = Ctx->Syncable;
  Cursor = PhysTypeBase + Offset;
  ArraySize = SpeculatedArraySize;
  Status = EFI_SUCCESS;

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
        Status = EagerDuplicateTypeMultiPointer(
            Ctx, VirtFieldElemSrc, &VirtFieldElemDst, PointerLevel);

        if (EFI_ERROR(Status))
          goto out;
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
              DstSandbox, ArraySize * FieldType->BasicType->TypeSize);

          VirtFieldDst = TO_VIRT_ADDR(PhysFieldDst);
          *(EFI_VIRTUAL_ADDRESS *)(Cursor) = VirtFieldDst;
          Status =
              InsertPointerRecordList(Ctx->PointerList, FieldType, VirtFieldSrc,
                                      VirtFieldDst, Cursor, MemSize, Syncable);
          if (EFI_ERROR(Status))
            goto out;
          CopyMem((VOID *)PhysFieldDst, (VOID *)PhysFieldSrc, MemSize);

        } else {
          // Not a basic type but a custom type. We need to do recursive deep
          // copy.
          MemSize = ArraySize * FieldType->CustomType->TypeSize;
          PhysFieldDst =
              (EFI_PHYSICAL_ADDRESS)AllocateSandboxMemory(DstSandbox, MemSize);

          VirtFieldDst = TO_VIRT_ADDR(PhysFieldDst);
          *(EFI_VIRTUAL_ADDRESS *)(Cursor) = VirtFieldDst;
          Status =
              InsertPointerRecordList(Ctx->PointerList, FieldType, VirtFieldSrc,
                                      VirtFieldDst, Cursor, MemSize, Syncable);
          if (EFI_ERROR(Status))
            goto out;

          CopyMem((VOID *)PhysFieldDst, (VOID *)PhysFieldSrc, MemSize);
          Ctx->CurrentType = FieldType;
          Ctx->InUnion = InUnion || (FieldType->Kind == CustomTypeKind &&
                                     !FieldType->CustomType->IsStruct);

          for (UINTN i = 0; i < ArraySize; i++) {
            VirtFieldElemDst = TO_VIRT_ADDR(PhysFieldDst) +
                               i * FieldType->CustomType->TypeSize;
            Status = EagerDuplicateCustomType(Ctx, VirtFieldElemDst);
            if (EFI_ERROR(Status))
              goto out;
          }
        }
        // Record the pointer for future synchronization and garbage
        // collection.
        Syncable = Ctx->Syncable;

      } else {

        MemSize = ArraySize * sizeof(EFI_VIRTUAL_ADDRESS);
        // Now this is a multi-level pointers with a pair of array size.
        PhysFieldDst =
            (EFI_PHYSICAL_ADDRESS)AllocateSandboxMemory(DstSandbox, MemSize);

        VirtFieldDst = TO_VIRT_ADDR(PhysFieldDst);
        *(EFI_VIRTUAL_ADDRESS *)(Cursor) = VirtFieldDst;
        Status =
            InsertPointerRecordList(Ctx->PointerList, FieldType, VirtFieldSrc,
                                    VirtFieldDst, Cursor, MemSize, Syncable);
        if (EFI_ERROR(Status))
          goto out;

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
          Status = EagerDuplicateTypeMultiPointer(
              Ctx, VirtFieldElemSrc, &VirtFieldElemDst, PointerLevel - 1);
          if (EFI_ERROR(Status))
            goto out;
          ElemCursor = PhysFieldDst + i * sizeof(EFI_VIRTUAL_ADDRESS);
          *(EFI_VIRTUAL_ADDRESS *)(ElemCursor) = VirtFieldElemDst;
        }
        Syncable = FALSE;
      }
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
        Status =
            EagerDuplicateCustomType(Ctx, TO_VIRT_ADDR(Cursor) == EFI_SUCCESS);
        if (EFI_ERROR(Status))
          goto out;
      } else {
        // For array of custom types, we should do deep copy for each element.
        for (UINTN i = 0; i < ArraySize; i++) {
          Status = EagerDuplicateCustomType(
              Ctx, TO_VIRT_ADDR(Cursor + i * FieldType->CustomType->TypeSize));
          if (EFI_ERROR(Status))
            goto out;
        }
      }
    }
  }
out:
  return Status;
}

EFI_STATUS EagerDuplicateCustomType(IN DUPLICATE_CTX *Ctx,
                                    IN EFI_VIRTUAL_ADDRESS VirtTypeBase) {

  CONST REFLECT_TYPE *Type;
  LIST_ENTRY *Link;
  REFLECT_FIELD *Field;
  EFI_STATUS Status = EFI_SUCCESS;
  Type = Ctx->CurrentType;
  BASE_LIST_FOR_EACH(Link, &Type->CustomType->Fields) {
    Field = BASE_CR(Link, REFLECT_FIELD, FieldNode);
    // This means that the current field is at least a single level pointer
    // type,
    Ctx->CurrentType = Field->FieldType;
    if (Field->ArraySize != 0)
      Status = EagerDuplicateCustomTypeField(Ctx, VirtTypeBase, Field->Offset,
                                             Field->ArraySize, 0,
                                             Field->PointerLevel);
    else
      Status = EagerDuplicateCustomTypeField(
          Ctx, VirtTypeBase, Field->Offset, 0,
          SpeculateTypeFieldArraySize(VirtTypeBase, Type, Field->FieldName),
          Field->PointerLevel);
    if (EFI_ERROR(Status))
      goto out;
  }
out:
  return Status;
}

// Duplicate A ReflectType From Src to Dst. Src is the address of a CustomType.
STATIC EFI_STATUS EagerDuplicateTypePointer(IN DUPLICATE_CTX *Ctx,
                                            IN CONST EFI_VIRTUAL_ADDRESS Src,
                                            OUT EFI_VIRTUAL_ADDRESS *Dst) {

  EFI_PHYSICAL_ADDRESS NextPhysBase;
  CONST REFLECT_TYPE *Type;
  UEFI_SANDBOX *DstSandbox;
  EFI_STATUS Status;
  DstSandbox = Ctx->PointerList->DstSandbox;
  Status = EFI_SUCCESS;

  if (Src == 0) {
    return Ctx->Optional ? EFI_SUCCESS : EFI_INVALID_PARAMETER;
  }

  Type = Ctx->CurrentType;
  NextPhysBase = (EFI_PHYSICAL_ADDRESS)AllocateSandboxMemory(
      DstSandbox, Ctx->CurrentType->CustomType->TypeSize);

  CopyMem((VOID *)NextPhysBase, (VOID *)(TO_PHYS_ADDR(Src)),
          Ctx->CurrentType->CustomType->TypeSize);
  *(Dst) = TO_VIRT_ADDR(NextPhysBase);

  Status =
      InsertPointerRecordList(Ctx->PointerList, Type, Src, *Dst, (UINT64)Dst,
                              Type->CustomType->TypeSize, Ctx->Syncable);
  if (!EFI_ERROR(Status))
    goto out;

  Status = EagerDuplicateCustomType(Ctx, TO_VIRT_ADDR(NextPhysBase));

out:
  return Status;
}
// Recursively duplicate a multi-level pointer type. Note that this multi-level
// pointer is int **Elem not int *Elem[]
EFI_STATUS EagerDuplicateTypeMultiPointer(IN DUPLICATE_CTX *Ctx,
                                          IN CONST EFI_VIRTUAL_ADDRESS Src,
                                          OUT EFI_VIRTUAL_ADDRESS *Dst,
                                          IN CONST UINTN PointerLevel) {

  EFI_VIRTUAL_ADDRESS NextSrc, NextDst;
  EFI_VIRTUAL_ADDRESS *Cursor;
  UEFI_SANDBOX *DstSandbox;
  CONST REFLECT_TYPE *Type;
  EFI_STATUS Status = 0;

  Type = Ctx->CurrentType;
  DstSandbox = Ctx->PointerList->DstSandbox;

  if (Src == 0) {
    *Dst = 0;
    return Ctx->Optional ? EFI_SUCCESS : EFI_INVALID_PARAMETER;
  }

  if (PointerLevel > 1) {

    NextSrc = *(EFI_VIRTUAL_ADDRESS *)(TO_PHYS_ADDR(Src));
    Status = EagerDuplicateTypeMultiPointer(Ctx, NextSrc, &NextDst,
                                            PointerLevel - 1);
    if (EFI_ERROR(Status))
      goto out;

    Cursor = (EFI_VIRTUAL_ADDRESS *)AllocateSandboxMemory(
        DstSandbox, sizeof(EFI_VIRTUAL_ADDRESS *));
    *Cursor = NextDst;

    (*Dst) = TO_VIRT_ADDR((EFI_VIRTUAL_ADDRESS)Cursor);
    return InsertPointerRecordList(Ctx->PointerList, Type, NextSrc, NextDst,
                                   (UINT64)Dst, sizeof(EFI_VIRTUAL_ADDRESS),
                                   FALSE);

  } else {
    switch (Type->Kind) {
    case BasicTypeKind:
      *Dst = (EFI_PHYSICAL_ADDRESS)AllocateSandboxMemory(
          DstSandbox, Type->BasicType->TypeSize);
      CopyMem(Dst, (VOID *)Src, Type->BasicType->TypeSize);
      *Dst = TO_VIRT_ADDR(*Dst);
      Status = InsertPointerRecordList(Ctx->PointerList, Type, Src, *Dst,
                                       (UINT64)Dst, Type->BasicType->TypeSize,
                                       Ctx->Syncable);
      break;
    case CustomTypeKind:
      Status = EagerDuplicateTypePointer(Ctx, Src, Dst);
      break;
    default:
      __unreachable();
    }
  }
out:
  return Status;
}

STATIC EFI_STATUS CopyOneCallParam(IN DUPLICATE_CTX *Ctx,
                                   IN CONST REFLECT_FUNC_TYPE *Function,
                                   IN CONST REFLECT_PARAM *Param,
                                   IN CONST UINT64 *Src, OUT UINT64 *Dst,
                                   IN CONST UINT64 Index) {

  EFI_VIRTUAL_ADDRESS NextVirtDst;
  EFI_PHYSICAL_ADDRESS NextPhysDst;
  UINTN ArraySize, MemSize;
  UEFI_SANDBOX *DstSandbox;
  BOOLEAN Syncable;
  EFI_STATUS Status;

  DstSandbox = Ctx->PointerList->DstSandbox;
  Status = EFI_SUCCESS;

  if (AsciiStrStr(Param->ParamName, "Str") != NULL ||
      AsciiStrCmp(Param->ParamName, "FileName") == 0) {
    ASSERT(Param->ParamType->Kind == BasicTypeKind);
    if (AsciiStrCmp(Param->ParamType->BasicType->TypeName, "CHAR16") == 0) {
      MemSize = StrSize((CHAR16 *)TO_PHYS_ADDR(Src[Index]));
      Dst[Index] = TO_VIRT_ADDR((EFI_PHYSICAL_ADDRESS)AllocateSandboxMemory(
          Ctx->PointerList->DstSandbox, MemSize));
      CopyMem((VOID *)TO_PHYS_ADDR(Dst[Index]), (VOID *)Src[Index], MemSize);

    } else if (AsciiStrCmp(Param->ParamType->BasicType->TypeName, "CHAR8") ==
               0) {

      MemSize = AsciiStrSize((CHAR8 *)TO_PHYS_ADDR(Src[Index]));
      Dst[Index] = TO_VIRT_ADDR((EFI_PHYSICAL_ADDRESS)AllocateSandboxMemory(
          Ctx->PointerList->DstSandbox, MemSize));
      CopyMem((VOID *)TO_PHYS_ADDR(Dst[Index]), (VOID *)Src[Index], MemSize);

    } else {
      __unimplemented("Str Type: %a\n", Param->ParamType->BasicType->TypeName);
    }
    Syncable = Ctx->Syncable;
    return InsertPointerRecordList(Ctx->PointerList, Param->ParamType,
                                   Src[Index], Dst[Index], (UINT64)&Dst[Index],
                                   MemSize, Syncable);
  }

  ArraySize = SpeculateFunctionParamArraySize(Src, Function, Param);

  if (Param->PointerLevel == 1) {

    if (Param->Optional && Src[Index] == 0) {
      Dst[Index] = 0;
      return EFI_SUCCESS;
    }

    if (Param->ParamType->Kind == BasicTypeKind) {
      Syncable = Ctx->Syncable;
      MemSize = ArraySize * Param->ParamType->BasicType->TypeSize;
      NextPhysDst =
          (EFI_PHYSICAL_ADDRESS)AllocateSandboxMemory(DstSandbox, MemSize);

      Dst[Index] = TO_VIRT_ADDR(NextPhysDst);
      Status = InsertPointerRecordList(Ctx->PointerList, Param->ParamType,
                                       Src[Index], Dst[Index],
                                       (UINT64)&Dst[Index], MemSize, Syncable);
      if (EFI_ERROR(Status))
        goto out;

      CopyMem((VOID *)NextPhysDst, (VOID *)TO_PHYS_ADDR(Src[Index]), MemSize);

      NextVirtDst = TO_VIRT_ADDR(NextPhysDst);

    } else {
      Syncable = Ctx->Syncable;
      MemSize = ArraySize * Param->ParamType->CustomType->TypeSize;
      NextPhysDst =
          (EFI_PHYSICAL_ADDRESS)AllocateSandboxMemory(DstSandbox, MemSize);

      Dst[Index] = TO_VIRT_ADDR(NextPhysDst);
      Status = InsertPointerRecordList(Ctx->PointerList, Param->ParamType,
                                       Src[Index], Dst[Index],
                                       (UINT64)&Dst[Index], MemSize, Syncable);
      if (EFI_ERROR(Status))
        goto out;

      NextVirtDst = TO_VIRT_ADDR(NextPhysDst);
      CopyMem((VOID *)NextPhysDst, (VOID *)TO_PHYS_ADDR(Src[Index]), MemSize);
      Ctx->CurrentType = Param->ParamType;
      Ctx->InUnion = (Param->ParamType->Kind == CustomTypeKind &&
                      !Param->ParamType->CustomType->IsStruct);

      for (UINTN i = 0; i < ArraySize; i++) {
        Status = EagerDuplicateCustomType(
            Ctx, NextVirtDst + i * Param->ParamType->CustomType->TypeSize);
        if (EFI_ERROR(Status))
          goto out;
      }
    }
  } else {

    Ctx->CurrentType = Param->ParamType;
    Ctx->InUnion = (Param->ParamType->Kind == CustomTypeKind &&
                    !Param->ParamType->CustomType->IsStruct);
    Syncable = FALSE;

    MemSize = ArraySize * sizeof(EFI_VIRTUAL_ADDRESS);
    NextPhysDst =
        (EFI_PHYSICAL_ADDRESS)AllocateSandboxMemory(DstSandbox, MemSize);

    Dst[Index] = TO_VIRT_ADDR(NextPhysDst);
    Status = InsertPointerRecordList(Ctx->PointerList, Param->ParamType,
                                     Src[Index], Dst[Index],
                                     (UINT64)&Dst[Index], MemSize, Syncable);
    if (EFI_ERROR(Status))
      goto out;

    for (UINTN i = 0; i < ArraySize; i++) {
      Status = EagerDuplicateTypeMultiPointer(
          Ctx,
          *(EFI_VIRTUAL_ADDRESS *)(Src[Index] +
                                   i * sizeof(EFI_VIRTUAL_ADDRESS)),
          &NextVirtDst, Param->PointerLevel - 1);

      if (EFI_ERROR(Status))
        goto out;

      *(EFI_VIRTUAL_ADDRESS *)(NextPhysDst + i * sizeof(EFI_VIRTUAL_ADDRESS)) =
          NextVirtDst;
    }
  }

out:
  return Status;
}

__attribute__((unused)) STATIC VOID AllocateSpaceForOneParam(
    IN DUPLICATE_CTX *Ctx, IN CONST REFLECT_FUNC_TYPE *Function,
    IN CONST REFLECT_PARAM *Param, IN CONST UINT64 *Src, OUT UINT64 *Dst,
    UINT64 Index) {

  UINTN ArraySize, MemSize;
  UEFI_SANDBOX *DstSandbox;
  DstSandbox = Ctx->PointerList->DstSandbox;
  ArraySize = SpeculateFunctionParamArraySize(Src, Function, Param);
  if (Param->PointerLevel == 1) {
    if (Param->ParamType->Kind == BasicTypeKind)
      MemSize = ArraySize * Param->ParamType->BasicType->TypeSize;
    else
      MemSize = ArraySize * Param->ParamType->CustomType->TypeSize;
    Dst[Index] = (EFI_VIRTUAL_ADDRESS)TO_VIRT_ADDR(
        (EFI_PHYSICAL_ADDRESS)AllocateSandboxMemory(DstSandbox, MemSize));
  } else {
    MemSize = ArraySize * sizeof(EFI_VIRTUAL_ADDRESS);
    Dst[Index] =
        (EFI_VIRTUAL_ADDRESS)AllocateSandboxMemory(DstSandbox, MemSize);
  }
  InsertPointerRecordList(Ctx->PointerList, Param->ParamType, Src[Index],
                          Dst[Index], (UINT64)&Dst[Index], MemSize, FALSE);
}

EFI_STATUS CopyCalloutParams(IN DUPLICATE_CTX *Ctx, IN CONST VOID *Opaque,
                             IN CONST REFLECT_FUNC_TYPE *Func,
                             IN CONST UINT64 *Src, OUT UINT64 *Dst) {
  REFLECT_PARAM *Param;
  LIST_ENTRY *Link;
  UINTN Index;
  EFI_STATUS Status;

#if SANDBOX_PERF_COPY_PARAMS
  UINTN Val1, Val2;
  Val1 = ReadCounter();
#endif

  Status = EFI_SUCCESS;
  Index = 0;
  BASE_LIST_FOR_EACH(Link, &Func->FunctionParams) {
    Param = BASE_CR(Link, REFLECT_PARAM, ParamNode);
    ASSERT(Param->OutParam || Param->InParam);

    if (AsciiStrCmp(Param->ParamName, "Context") == 0) {
      Dst[Index] = Src[Index];
      continue;
    }

    if (AsciiStrStr(Param->ParamName, "CallBack") != NULL) {
      ASSERT(Param->ParamType->Kind == FunctionKind);
      Dst[Index] = TO_VIRT_ADDR((EFI_PHYSICAL_ADDRESS)CreateCallbackTrampoline(
          Ctx->PointerList->SrcSandbox, (UINTN)Param->ParamType->Function,
          Ctx->PointerList->DstSandbox->SandboxID, Src[Index]));
      InsertPointerRecordList(Ctx->PointerList, Param->ParamType, Src[Index],
                              Dst[Index], (UINTN)&Src[Index], sizeof(UINTN),
                              FALSE);
      continue;
    }

    if (Param->ParamType->Kind == ProtocolKind) {
      // Src[i] With Real Protocol Interface Counterpart find this protocol in
      // the protocol list;

      if (Param->InParam)
        Dst[Index] = (UINTN)Opaque;
      else {
        ASSERT(Param->PointerLevel == 2);
        Dst[Index] = TO_VIRT_ADDR((UINTN)AllocateSandboxMemory(
            Ctx->PointerList->DstSandbox, sizeof(EFI_VIRTUAL_ADDRESS)));
        Status = InsertPointerRecordList(Ctx->PointerList, NULL, Src[Index],
                                         Dst[Index], (UINTN)&Dst[Index],
                                         sizeof(EFI_VIRTUAL_ADDRESS), FALSE);
        if (EFI_ERROR(Status))
          goto out;
      }
    } else {

      if (Param->PointerLevel > 0) {

        if (Param->PointerLevel > 1 && Param->OutParam && !Param->InParam) {
          ASSERT(Param->PointerLevel == 2);
          Dst[Index] = TO_VIRT_ADDR((UINTN)AllocateSandboxMemory(
              Ctx->PointerList->DstSandbox, sizeof(EFI_VIRTUAL_ADDRESS)));
          InsertPointerRecordList(Ctx->PointerList, NULL, Src[Index],
                                  Dst[Index], (UINTN)&Dst[Index],
                                  sizeof(EFI_VIRTUAL_ADDRESS), FALSE);
        } else {
          Ctx->Syncable = Param->OutParam;
          Status = CopyOneCallParam(Ctx, Func, Param, Src, Dst, Index);

          if (EFI_ERROR(Status))
            goto out;
        }

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
out:
  return Status;
}

EFI_STATUS AllocatePersistentLocatedInterface(
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
  InitPointerRecordList(&LocatedInterface->Magisk.PointerList, CalleeSandbox,
                        CallerSandbox);
  (*Located) = LocatedInterface;
  return CreateInterfaceMagisk(Protocol, Delegated,
                               CallerSandbox == &CoreSandbox,
                               &LocatedInterface->Magisk);
}

VOID SyncCalloutParams(IN UEFI_SANDBOX *CallerSandbox,
                       IN UEFI_SANDBOX *CalleeSandbox,
                       IN CONST REFLECT_FUNC_TYPE *Function,
                       IN CONST UINT64 *Dst, OUT UINT64 *Src) {

  LOCATED_INTERFACE *Located;
  REFLECT_PARAM *Param;
  LIST_ENTRY *Link;
  UINTN ArraySize, Size, Index;
  VOID *PhysSrc, *PhysDst;
  EFI_STATUS Status;
  Index = 0;

  BASE_LIST_FOR_EACH(Link, &Function->FunctionParams) {
    Param = BASE_CR(Link, REFLECT_PARAM, ParamNode);
    if (Param->ParamType->Kind == ProtocolKind && Param->OutParam) {
      ASSERT(Param->PointerLevel == 2);
      ASSERT(Param->ParamType->Kind = ProtocolKind);
      Status = AllocatePersistentLocatedInterface(
          CallerSandbox, CalleeSandbox, Param->ParamType->Protocol,
          *(EFI_VIRTUAL_ADDRESS *)Dst[Index], &Located);
      ASSERT_EFI_ERROR(Status);
      *(VOID **)TO_PHYS_ADDR(Src[Index]) = Located->Magisk.Interface;
    } else {
      if (Param->PointerLevel > 1 && Param->OutParam) {
        PhysDst =
            (VOID *)TO_PHYS_ADDR((UINTN)(*(VOID **)TO_PHYS_ADDR(Dst[Index])));

        if (AsciiStrStr(Param->ParamName, "Name") != NULL) {
          Size = StrSize(PhysDst);
        } else {

          ArraySize = SpeculateFunctionParamArraySize(Dst, Function, Param);
          if (Param->ParamType->Kind == BasicTypeKind)
            Size = ArraySize * Param->ParamType->BasicType->TypeSize;
          else
            Size = ArraySize * Param->ParamType->CustomType->TypeSize;
        }
        PhysSrc = AllocateSandboxMemory(CallerSandbox, Size);
        CopyMem(PhysSrc, PhysDst, Size);
        *(VOID **)TO_PHYS_ADDR(Src[Index]) =
            (VOID *)TO_VIRT_ADDR((UINTN)PhysSrc);
      }
    }

    Index++;
  }
}
