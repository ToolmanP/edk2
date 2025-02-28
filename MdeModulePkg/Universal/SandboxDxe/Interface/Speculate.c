
#include <Interface/Duplicate.h>
#include <Memory/Memory.h>

#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>

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

UINTN SpeculateFunctionParamArraySize(
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
