#include "Library/BaseMemoryLib.h"
#include "Library/DebugLib.h"
#include "ProcessorBind.h"
#include "ProtocolProxy.h"
#include "Utils/Hashmap.h"
#include "Base.h"
#include "Library/BaseLib.h"
#include "Library/JsonLib.h"
#include "Library/MemoryAllocationLib.h"
#include "Print.h"
#include "Uefi/UefiBaseType.h"

extern const CHAR8 ProtocolDBRaw[];

EDKII_JSON_OBJECT RootDB = NULL;
EDKII_JSON_OBJECT ProtocolDB = NULL;
EDKII_JSON_OBJECT FunctionDB = NULL;
EDKII_JSON_OBJECT TypeDB = NULL;
EDKII_JSON_OBJECT SynonymDB = NULL;

UINTN EnumArraySize = 0;
EDKII_JSON_ARRAY EnumArray = NULL;

struct TypeKV {
  CHAR8 *Name;
  struct Type *Type;
};

struct ProtocolKV {
  EFI_GUID Guid;
  struct Protocol *Protocol;
};

struct hashmap *TypeMap = NULL;
struct hashmap *ProtocolMap = NULL;

int TypeKV_Compare(const void *a, const void *b, void *udata) {
  const struct TypeKV *TypeA = (struct TypeKV *)a;
  const struct TypeKV *TypeB = (struct TypeKV *)b;
  return AsciiStrCmp(TypeA->Name, TypeB->Name);
}

uint64_t TypeKV_Hash(const void *item, uint64_t seed0, uint64_t seed1) {
  const struct TypeKV *Type = (struct TypeKV *)item;
  return hashmap_sip(Type->Name, AsciiStrLen(Type->Name), seed0, seed1);
}

int ProtocolKV_Compare(const void *a, const void *b, void *udata) {
  const struct ProtocolKV *ProtocolA = (struct ProtocolKV *)a;
  const struct ProtocolKV *ProtocolB = (struct ProtocolKV *)b;

  // Return 0 if the GUIDs are the same
  if (CompareGuid(&ProtocolA->Guid, &ProtocolB->Guid)) {
    return 0;
  } else {
    return 1;
  }
}

uint64_t ProtocolKV_Hash(const void *item, uint64_t seed0, uint64_t seed1) {
  const struct ProtocolKV *Protocol = (struct ProtocolKV *)item;
  return hashmap_sip(&Protocol->Guid, sizeof(EFI_GUID), seed0, seed1);
}

VOID PrintJsonError(EDKII_JSON_ERROR *Error) {
  SBError("Error at line %d column %d position %d, source: %a, text: %a\n", Error->Line, Error->Column, Error->Position, Error->Source, Error->Text);
}

static UINTN VariationSeed = 1234567891;
static EFI_GUID RANDOM_BASE_GUID = {
  0xdbc31dfa, 0xcd10, 0x4b6e,
  { 0xac, 0x83, 0x2d, 0xbd, 0x5e, 0x01, 0x27, 0x6f }
};

static EFI_STATUS GeneratePseudoRandomGuid(
  OUT EFI_GUID *NewGuid
) {
  CopyGuid(NewGuid, &RANDOM_BASE_GUID);

  // Modify fields to create a variation
  NewGuid->Data1 ^= VariationSeed;
  NewGuid->Data2 ^= (UINT16)(VariationSeed >> 16);
  NewGuid->Data3 ^= (UINT16)VariationSeed;

  // Modify Data4 using the VariationSeed
  for (UINTN i = 0; i < sizeof(NewGuid->Data4); i++) {
    NewGuid->Data4[i] ^= (UINT8)((VariationSeed >> (i * 4)) & 0xFF);
  }

  NewGuid->Data3 &= 0x0FFF;
  NewGuid->Data3 |= (4 << 12);
  NewGuid->Data4[0] &= 0x3F;
  NewGuid->Data4[0] |= 0x80;

  VariationSeed += 9876543211;

  return EFI_SUCCESS;
}

static VOID AddTypeToMap(const CHAR8 *Typename, struct Type *Ty)
{
  struct TypeKV TyKV;
  TyKV.Name = (char *)Typename;
  TyKV.Type = Ty;
  hashmap_set(TypeMap, &TyKV);
}

const struct BasicType BasicTypes[] = {
  { .TypeName = "UINT8", .TypeSize = sizeof(UINT8) },           // Should be 1 byte (8 bits)
  { .TypeName = "UINT16", .TypeSize = sizeof(UINT16) },         // Should be 2 bytes (16 bits)
  { .TypeName = "UINT32", .TypeSize = sizeof(UINT32) },         // Should be 4 bytes (32 bits)
  { .TypeName = "UINT64", .TypeSize = sizeof(UINT64) },         // Should be 8 bytes (64 bits)
  { .TypeName = "UINTN", .TypeSize = sizeof(UINTN) },           // Depends on platform, 4 bytes for 32-bit, 8 bytes for 64-bit
  { .TypeName = "INT8", .TypeSize = sizeof(INT8) },             // Should be 1 byte (8 bits)
  { .TypeName = "INT16", .TypeSize = sizeof(INT16) },           // Should be 2 bytes (16 bits)
  { .TypeName = "INT32", .TypeSize = sizeof(INT32) },           // Should be 4 bytes (32 bits)
  { .TypeName = "INT64", .TypeSize = sizeof(INT64) },           // Should be 8 bytes (64 bits)
  { .TypeName = "INTN", .TypeSize = sizeof(INTN) },             // Depends on platform, 4 bytes for 32-bit, 8 bytes for 64-bit
  { .TypeName = "CHAR8", .TypeSize = sizeof(CHAR8) },           // Should be 1 byte (8 bits)
  { .TypeName = "CHAR16", .TypeSize = sizeof(CHAR16) },         // Should be 2 bytes (16 bits, 16-bit characters)
  { .TypeName = "BOOLEAN", .TypeSize = sizeof(BOOLEAN) },       // Typically 1 byte (for true/false)
  { .TypeName = "VOID", .TypeSize = 1 },
  { .TypeName = "void", .TypeSize = 1 },
  { .TypeName = "EFI_GUID", .TypeSize = sizeof(EFI_GUID) },     // Typically 16 bytes
  { .TypeName = "EFI_STATUS", .TypeSize = sizeof(EFI_STATUS) }, // Typically 4 bytes
  { .TypeName = "EFI_HANDLE", .TypeSize = sizeof(EFI_HANDLE) }, // Typically 8 bytes (for pointer)
  { .TypeName = "EFI_EVENT", .TypeSize = sizeof(EFI_EVENT) },   // Typically 8 bytes (for pointer)
  { .TypeName = "EFI_TPL", .TypeSize = sizeof(EFI_TPL) },       // Typically 4 bytes
  { .TypeName = "EFI_LBA", .TypeSize = sizeof(EFI_LBA) },       // Typically 8 bytes
  { .TypeName = "EFI_PHYSICAL_ADDRESS", .TypeSize = sizeof(EFI_PHYSICAL_ADDRESS) }, // Typically 8 bytes
  { .TypeName = "EFI_VIRTUAL_ADDRESS", .TypeSize = sizeof(EFI_VIRTUAL_ADDRESS) },   // Typically 8 bytes
};

static EFI_STATUS LoadBasicTypes()
{
  for (UINTN i = 0; i < ARRAY_SIZE(BasicTypes); i++) {
    struct Type *Ty = AllocateZeroPool(sizeof(struct Type));
    Ty->Kind = BasicTypeKind;
    Ty->BasicType = (struct BasicType *)&BasicTypes[i];
    AddTypeToMap(BasicTypes[i].TypeName, Ty);
  }

  return EFI_SUCCESS;
}

/*
 * Load all the protocols from the protocol database.
 * As Protocol are indexed by GUID, we preload all the protocols at initilization.
 */
static EFI_STATUS LoadProtocols()
{
  EFI_STATUS Status;
  VOID *ProtocolJsonIter;
  CHAR8 *ProtocolName;
  const CHAR8 *ProtocolGuidString;
  EDKII_JSON_OBJECT ProtocolJson;
  EDKII_JSON_VALUE ProtocolGuidJson;
  struct Protocol *Protocol;
  struct ProtocolKV ProtoKV;
  struct Type *Ty;

  ProtocolJsonIter = JsonObjectIterator(ProtocolDB);
  while (ProtocolJsonIter != NULL) {
    ProtocolName = JsonObjectIteratorKey(ProtocolJsonIter);
    if (ProtocolName == NULL) {
      SBWarn("Failed to get protocol name\n");
      goto next;
    }

    ProtocolJson = JsonObjectIteratorValue(ProtocolJsonIter);
    if (ProtocolJson == NULL) {
      SBWarn("Failed to get protocol value\n");
      goto next;
    }

    ProtocolGuidJson = JsonObjectGetValue(ProtocolJson, "guid");
    if (ProtocolGuidJson == NULL || !JsonValueIsString(ProtocolGuidJson)) {
      SBWarn("Failed to get protocol guid\n");
      goto next;
    }

    ProtocolGuidString = JsonValueGetString(ProtocolGuidJson);
    if (ProtocolGuidString == NULL) {
      SBWarn("Failed to get protocol guid string, Protocol: %a\n", ProtocolName);
      goto next;
    }

    /*
     * Not all Protocols have GUID, for example: EFI_FILE_PROTOCOL.
     * For those who don't, generate a random one for them, the GUID won't be used.
     */
    if (AsciiStrLen(ProtocolGuidString) == 0) {
      Status = GeneratePseudoRandomGuid(&ProtoKV.Guid);
      if (EFI_ERROR(Status)) {
        SBWarn("Failed to generate random guid, Protocol: %a\n", ProtocolName);
        goto next;
      }

      SBDebug("Generated random GUID %g for Protocol: %a\n", &ProtoKV.Guid, ProtocolName);
    } else {
      /* Convert GUID string to GUID, and directly store in ProtoKV */
      Status = AsciiStrToGuid(ProtocolGuidString, &ProtoKV.Guid);
      if (EFI_ERROR(Status)) {
        SBWarn("Failed to convert guid, Protocol: %a\n", ProtocolName);
        goto next;
      }
    }

    /* Create new Protocol struct */
    Protocol = AllocateZeroPool(sizeof(struct Protocol));
    if (Protocol == NULL) {
      SBError("Failed to allocate memory for protocol\n");
      return EFI_OUT_OF_RESOURCES;
    }

    /* Do not allocate memory for name, directly point to the string in the JSON */
    Protocol->ProtocolName = ProtocolName;
    Protocol->ProtocolJson = ProtocolJson;
    CopyGuid(&Protocol->Guid, &ProtoKV.Guid);
    /* Protocol fields is lazy loaded */
    InitializeListHead(&Protocol->FieldsList);

    /* Set ProtKV Guid */
    ProtoKV.Protocol = Protocol;
    hashmap_set(ProtocolMap, &ProtoKV);

    Ty = AllocateZeroPool(sizeof(struct Type));
    Ty->Kind = ProtocolKind;
    Ty->Protocol = Protocol;
    AddTypeToMap(ProtocolName, Ty);

next:
    ProtocolJsonIter = JsonObjectIteratorNext(ProtocolDB, ProtocolJsonIter);
  }

  return EFI_SUCCESS;
}

static LIST_ENTRY LoadingProtocolStack = INITIALIZE_LIST_HEAD_VARIABLE(LoadingProtocolStack);

struct LoadingProtcol {
  LIST_ENTRY Node;
  const struct Protocol *Protocol;
};

static EFI_STATUS AddLoadingProtocol(const struct Protocol *Protocol)
{
  LIST_ENTRY *Link;
  struct LoadingProtcol *LoadingProt;

  BASE_LIST_FOR_EACH(Link, &LoadingProtocolStack) {
    LoadingProt = BASE_CR(Link, struct LoadingProtcol, Node);
    if (LoadingProt->Protocol == Protocol) {
      SBError("Recursive loading detected\n");
      return EFI_PROTOCOL_ERROR;
    }
  }

  LoadingProt = AllocateZeroPool(sizeof(struct LoadingProtcol));
  LoadingProt->Protocol = Protocol;
  InsertTailList(&LoadingProtocolStack, &LoadingProt->Node);

  return EFI_SUCCESS;
}

static VOID FinishLoadingProtocol(const struct Protocol *Protocol)
{
  LIST_ENTRY *Link;
  struct LoadingProtcol *LoadingProt;

  BASE_LIST_FOR_EACH(Link, &LoadingProtocolStack) {
    LoadingProt = BASE_CR(Link, struct LoadingProtcol, Node);
    if (LoadingProt->Protocol == Protocol) {
      RemoveEntryList(&LoadingProt->Node);
      FreePool(LoadingProt);
      return;
    }
  }
}

static EFI_STATUS LoadProtocolFields(struct Protocol *Protocol)
{
  EFI_STATUS Status;
  EDKII_JSON_OBJECT ProtocolTypeJson;
  EDKII_JSON_OBJECT ProtocolFieldsJson;
  EDKII_JSON_VALUE ProtocolSizeJson;

  VOID *FieldsJsonIter;
  struct ProtocolField *Field;
  CHAR8 *FieldName;
  EDKII_JSON_OBJECT FieldJson;
  const CHAR8 *TypeName;
  UINTN FieldOffset;
  UINTN FieldPointerLevel;
  UINTN FieldArraySize;
  struct Type* Ty;

  /*
   * To avoid recursive calls, check that we are not in the process of loading this protocol's fields
   */
  if (EFI_ERROR(AddLoadingProtocol(Protocol))) {
    return EFI_SUCCESS;
  }

  ProtocolTypeJson = JsonObjectGetValue(Protocol->ProtocolJson, "ty");
  if (ProtocolTypeJson == NULL) {
    SBError("Failed to get protocol type\n");
    Status = EFI_LOAD_ERROR;
    goto out;
  }

  ProtocolSizeJson = JsonObjectGetValue(ProtocolTypeJson, "size");
  if (ProtocolSizeJson == NULL) {
    SBError("Failed to get protocol size\n");
    Status = EFI_LOAD_ERROR;
    goto out;
  }
  Protocol->ProtocolSize = JsonValueGetInteger(ProtocolSizeJson);

  ProtocolFieldsJson = JsonObjectGetValue(ProtocolTypeJson, "fields");
  if (ProtocolFieldsJson == NULL) {
    SBError("Failed to get protocol fields\n");
    Status = EFI_LOAD_ERROR;
    goto out;
  }

  FieldsJsonIter = JsonObjectIterator(ProtocolFieldsJson);
  while (FieldsJsonIter != NULL) {
    FieldName = JsonObjectIteratorKey(FieldsJsonIter);
    if (FieldName == NULL) {
      SBError("Failed to get field name key\n");
      goto free_resource;
    }

    FieldJson = JsonObjectIteratorValue(FieldsJsonIter);
    if (FieldJson == NULL) {
      SBError("Failed to get field\n");
      goto free_resource;
    }

    TypeName = JsonValueGetString(JsonObjectGetValue(FieldJson, "ty"));
    if (TypeName == NULL) {
      SBError("Failed to get field type\n");
      goto free_resource;
    }

    /* Create new Field struct */
    Field = AllocateZeroPool(sizeof(struct ProtocolField));

    FieldOffset = JsonValueGetInteger(JsonObjectGetValue(FieldJson, "offset"));
    Field->Offset = FieldOffset;

    if (EFI_ERROR(GetTypeByName(TypeName, &Ty))) {
      SBError("Failed to get field type\n");
      goto free_resource;
    }

    if (Ty->Kind == FunctionKind) {
      Field->IsFunction = TRUE;
      Field->Function = Ty->Function;
    } else {
      struct Variable *Var;

      FieldPointerLevel = JsonValueGetInteger(JsonObjectGetValue(FieldJson, "pointer_levels"));
      FieldArraySize = JsonValueGetInteger(JsonObjectGetValue(FieldJson, "array_size"));

      Var = AllocateZeroPool(sizeof(struct Variable));
      Var->VariableName = FieldName;
      Var->PointerLevel = FieldPointerLevel;
      Var->ArraySize = FieldArraySize;
      Var->VariableType = Ty;

      Field->IsFunction = FALSE;
      Field->Variable = Var;
    }

    InsertTailList(&Protocol->FieldsList, &Field->ProtocolFieldNode);

    FieldsJsonIter = JsonObjectIteratorNext(ProtocolFieldsJson, FieldsJsonIter);
  }

  Status = EFI_SUCCESS;
  goto out;

free_resource:
  if (!IsListEmpty(&Protocol->FieldsList)) {
    LIST_ENTRY *Link;
    BASE_LIST_FOR_EACH(Link, &Protocol->FieldsList) {
      Field = BASE_CR(Link, struct ProtocolField, ProtocolFieldNode);
      if (Field != NULL) {
        FreePool(Field);
      }
    }

    InitializeListHead(&Protocol->FieldsList);
  }

  Status = EFI_LOAD_ERROR;

out:
  FinishLoadingProtocol(Protocol);

  return Status;
}

static EFI_STATUS LoadEnumByName(const CHAR8 *EnumName, struct Type **TypePtr)
{
  const CHAR8 *EnumStr;
  struct BasicType *BasicTy;
  struct Type *Ty;

  for (UINTN i = 0; i < EnumArraySize; i++) {
    EnumStr = JsonValueGetString(JsonArrayGetValue(EnumArray, i));

    if (AsciiStrCmp(EnumStr, EnumName) == 0) {
      BasicTy = AllocateZeroPool(sizeof(struct BasicType));
      BasicTy->TypeName = EnumStr;
      BasicTy->TypeSize = sizeof(int);


      Ty = AllocateZeroPool(sizeof(struct Type));
      Ty->Kind = BasicTypeKind;
      Ty->BasicType = BasicTy;
      AddTypeToMap(EnumName, Ty);

      *TypePtr = Ty;

      return EFI_SUCCESS;
    }
  }

  return EFI_NOT_FOUND;
}

/* Search "function" in json */
static EFI_STATUS LoadFunctionByName(const CHAR8 *FunctionName, struct Type **TypePtr)
{
  EDKII_JSON_OBJECT FunctionJson;
  EDKII_JSON_ARRAY ParamArrayJson;
  struct Function *Func;
  UINTN ParamNum;

  EDKII_JSON_VALUE ParamJson;
  struct Param *Param;
  const CHAR8 *ParamName;
  BOOLEAN InParam;
  BOOLEAN OutParam;
  BOOLEAN Optional;
  UINTN PointerLevel;
  UINTN Offset;
  const CHAR8 *ParamTypeName;
  struct Type *Ty;

  FunctionJson = JsonObjectGetValue(FunctionDB, FunctionName);
  if (FunctionJson == NULL) {
    return EFI_NOT_FOUND;
  }

  ParamArrayJson = JsonValueGetArray(JsonObjectGetValue(FunctionJson, "params"));
  if (ParamArrayJson == NULL) {
    SBError("Failed to get function params\n");
    return EFI_LOAD_ERROR;
  }
  ParamNum = JsonArrayCount(ParamArrayJson);

  Func = AllocateZeroPool(sizeof(struct Function));
  Func->FunctionName = JsonValueGetString(JsonObjectGetValue(FunctionJson, "name"));
  InitializeListHead(&Func->FunctionParams);

  for (UINTN i = 0; i < ParamNum; i++) {
    ParamJson = JsonArrayGetValue(ParamArrayJson, i);
    if (ParamJson == NULL) {
      SBError("Failed to get param\n");
      goto out;
    }

    ParamName = JsonValueGetString(JsonObjectGetValue(ParamJson, "name"));
    InParam = JsonValueGetBoolean(JsonObjectGetValue(ParamJson, "in_param"));
    OutParam = JsonValueGetBoolean(JsonObjectGetValue(ParamJson, "out_param"));
    Optional = JsonValueGetBoolean(JsonObjectGetValue(ParamJson, "optional"));
    Offset = JsonValueGetInteger(JsonObjectGetValue(ParamJson, "offset"));
    PointerLevel = JsonValueGetInteger(JsonObjectGetValue(ParamJson, "pointer_levels"));
    ParamTypeName = JsonValueGetString(JsonObjectGetValue(ParamJson, "ty"));

    if (EFI_ERROR(GetTypeByName(ParamTypeName, &Ty))) {
      SBError("Failed to get param type\n");
      goto out;
    }

    Param = AllocateZeroPool(sizeof(struct Param));
    Param->ParamName = ParamName;
    Param->Offset = Offset;
    Param->PointerLevel = PointerLevel;
    Param->InParam = InParam;
    Param->OutParam = OutParam;
    Param->Optional = Optional;
    Param->ParamType = Ty;

    InsertTailList(&Func->FunctionParams, &Param->ParamNode);
  }

  Ty = AllocateZeroPool(sizeof(struct Type));
  Ty->Kind = FunctionKind;
  Ty->Function = Func;
  AddTypeToMap(Func->FunctionName, Ty);

  *TypePtr = Ty;

  return EFI_SUCCESS;

out:
  if (!IsListEmpty(&Func->FunctionParams)) {
    LIST_ENTRY *Link;
    BASE_LIST_FOR_EACH(Link, &Func->FunctionParams) {
      Param = BASE_CR(Link, struct Param, ParamNode);
      if (Param != NULL) {
        FreePool(Param);
      }
    }
  }
  FreePool(Func);

  return EFI_LOAD_ERROR;
}

// TODO:If there exists mutual references between functions and custom types
/*
 * Search "type" in json to find a CustomType.
 * As LoadCustomType can only be called during the parsing process of a Protocol or a Function,
 * the parameter "CustomTypeName" must be loaded from the json and is guaranteed to be valid.
 */
static EFI_STATUS LoadCustomTypeByName(const CHAR8 *CustomTypeName, struct Type **TypePtr)
{
  EFI_STATUS Status;
  EDKII_JSON_OBJECT CustomTyJson;
  struct CustomType *CustomTy;
  EDKII_JSON_OBJECT FieldListJson;

  VOID *FieldListJsonIter;
  struct Field *Field;
  CHAR8 *FieldName;
  EDKII_JSON_OBJECT FieldJson;
  const CHAR8 *FieldTypeName;
  UINTN FieldOffset;
  UINTN FieldPointerLevel;
  UINTN FieldArraySize;
  struct Type* Ty;

  CustomTyJson = JsonObjectGetValue(TypeDB, CustomTypeName);
  if (CustomTyJson == NULL) {
    return EFI_NOT_FOUND;
  }

  FieldListJson = JsonObjectGetValue(CustomTyJson, "fields");
  if (FieldListJson == NULL) {
    SBError("Failed to get custom type fields\n");
    return EFI_LOAD_ERROR;
  }

  CustomTy = AllocateZeroPool(sizeof(struct CustomType));
  CustomTy->TypeSize = JsonValueGetInteger(JsonObjectGetValue(CustomTyJson, "size"));
  CustomTy->IsStruct = JsonValueGetBoolean(JsonObjectGetValue(CustomTyJson, "is_struct"));
  InitializeListHead(&CustomTy->Fields);

  FieldListJsonIter = JsonObjectIterator(FieldListJson);
  while (FieldListJsonIter != NULL) {
    FieldName = JsonObjectIteratorKey(FieldListJsonIter);
    if (FieldName == NULL) {
      SBError("Failed to get field name\n");
      Status = EFI_LOAD_ERROR;
      goto out;
    }

    FieldJson = JsonObjectIteratorValue(FieldListJsonIter);
    if (FieldJson == NULL) {
      SBError("Failed to get field value, field name: %a\n", FieldName);
      Status = EFI_LOAD_ERROR;
      goto out;
    }

    FieldTypeName = JsonValueGetString(JsonObjectGetValue(FieldJson, "ty"));
    if (FieldTypeName == NULL) {
      SBError("Failed to get ty\n");
      Status = EFI_LOAD_ERROR;
      goto out;
    }

    FieldPointerLevel = JsonValueGetInteger(JsonObjectGetValue(FieldJson, "pointer_levels"));
    FieldOffset = JsonValueGetInteger(JsonObjectGetValue(FieldJson, "offset"));
    FieldArraySize = JsonValueGetInteger(JsonObjectGetValue(FieldJson, "array_size"));

    /* Create new Field struct */
    Field = AllocateZeroPool(sizeof(struct Field));
    Field->Offset = FieldOffset;
    Field->FieldName = FieldName;
    Field->PointerLevel = FieldPointerLevel;
    Field->ArraySize = FieldArraySize;

    if (EFI_ERROR(GetTypeByName(FieldTypeName, &Ty))) {
      SBError("Failed to get field type\n");
      Status = EFI_LOAD_ERROR;
      goto out;
    }
    Field->FieldType = Ty;

    InsertTailList(&CustomTy->Fields, &Field->FieldNode);

    FieldListJsonIter = JsonObjectIteratorNext(FieldListJson, FieldListJsonIter);
  }

  /* Add CustomType to TypeMap */
  Ty = AllocateZeroPool(sizeof(struct Type));
  Ty->Kind = CustomTypeKind;
  Ty->CustomType = CustomTy;
  AddTypeToMap(CustomTypeName, Ty);

  *TypePtr = Ty;

  return EFI_SUCCESS;

out:
  if (!IsListEmpty(&CustomTy->Fields)) {
    LIST_ENTRY *Link;
    BASE_LIST_FOR_EACH(Link, &CustomTy->Fields) {
      Field = BASE_CR(Link, struct Field, FieldNode);
      if (Field != NULL) {
        FreePool(Field);
      }
    }
  }
  FreePool(CustomTy);

  return Status;
}

static EFI_STATUS GetSynonym(const CHAR8 *Name, const CHAR8 **SynonymPtr)
{
  const CHAR8 *Synonym;

  Synonym = JsonValueGetString(JsonObjectGetValue(SynonymDB, Name));
  if (Synonym == NULL) {
    return EFI_NOT_FOUND;
  }

  *SynonymPtr = Synonym;

  return EFI_SUCCESS;
}

EFI_STATUS GetProtocol(const EFI_GUID *Guid, struct Protocol **ProtocolPtr)
{
  struct ProtocolKV ProtoKey;
  const struct ProtocolKV *ProtoKV;
  struct Protocol *Protocol;

  CopyGuid(&ProtoKey.Guid, Guid);

  // We already have the protocol in the hashmap
  ProtoKV = (const struct ProtocolKV *)hashmap_get(ProtocolMap, &ProtoKey);
  if (ProtoKV == NULL) {
    SBError("Protocol %g not found in hashmap\n", Guid);
    return EFI_NOT_FOUND;
  }

  ASSERT(ProtoKV->Protocol != NULL);
  Protocol = ProtoKV->Protocol;

  // Check if Protocol fields have been loaded
  if (IsListEmpty(&Protocol->FieldsList)) {
    LoadProtocolFields(Protocol);
  }

  *ProtocolPtr = Protocol;

  return EFI_SUCCESS;
}

EFI_STATUS GetFunctionByOffset(const struct Protocol *Protocol, UINT64 Offset, struct Function **FunctionPtr)
{
  struct Function *Func;
  LIST_ENTRY *Link;

  if (IsListEmpty(&Protocol->FieldsList)) {
    SBError("Protocol fields not loaded\n");
    return EFI_LOAD_ERROR;
  }

  BASE_LIST_FOR_EACH(Link, &Protocol->FieldsList) {
    struct ProtocolField *Field = BASE_CR(Link, struct ProtocolField, ProtocolFieldNode);
    if (Field->Offset == Offset && Field->IsFunction) {
      Func = Field->Function;
      *FunctionPtr = Func;
      return EFI_SUCCESS;
    }
  }

  return EFI_NOT_FOUND;
}

EFI_STATUS GetTypeByName(const CHAR8 *TypeName, struct Type **Type)
{
  EFI_STATUS Status;
  struct TypeKV TyKVEntity;
  const struct TypeKV *TyKV;

  struct Type *Ty;
  const CHAR8 *SearchName;
  const CHAR8 *Synonym;

  /* Protocol Type and Basic Type have been preloaded during initilization */
  TyKVEntity.Name = (CHAR8 *)TypeName;
  TyKV = (const struct TypeKV *)hashmap_get(TypeMap, &TyKVEntity);
  if (TyKV != NULL) {
    ASSERT(TyKV->Type != NULL);
    Ty = TyKV->Type;

    /* For ProtocolKind, fields are lazily loaded */
    if (Ty->Kind == ProtocolKind && IsListEmpty(&Ty->Protocol->FieldsList)) {
      Status = LoadProtocolFields(Ty->Protocol);
      if (EFI_ERROR(Status)) {
        return Status;
      }
    }

    *Type = Ty;
    return EFI_SUCCESS;
  }

  /*
   * Type is not found in the map, it cannot be ProtocolKind,
   * first check if it is a function type, custom type, enum type.
   */
  SearchName = TypeName;
  Synonym = NULL;
  for (UINTN i = 0; i < 2; i++) {
    if ((Status = LoadFunctionByName(SearchName, &Ty)) != EFI_NOT_FOUND) {
      if (EFI_ERROR(Status)) {
        return Status;
      }

      break;
    }

    if ((Status = LoadCustomTypeByName(SearchName, &Ty)) != EFI_NOT_FOUND) {
      if (EFI_ERROR(Status)) {
        return Status;
      }

      break;
    }

    if (!EFI_ERROR(LoadEnumByName(SearchName, &Ty))) {
      break;
    }

    if (i == 0) {
      if (EFI_ERROR(GetSynonym(SearchName, &Synonym)) || AsciiStrCmp(Synonym, SearchName) == 0) {
        SBError("Type not found\n");
        return EFI_NOT_FOUND;
      }

      SearchName = Synonym;
    } else {
      SBError("Type not found\n");
      return EFI_NOT_FOUND;
    }
  }

  *Type = Ty;

  return EFI_SUCCESS;
}

EFI_STATUS InitProtocolDB()
{
  EFI_STATUS Status;
  EDKII_JSON_ERROR Error;

  RootDB = JsonLoadString(ProtocolDBRaw, 0, &Error);
  if (RootDB == NULL) {
    SBError("Failed to load protocol database\n");
    PrintJsonError(&Error);
    return EFI_LOAD_ERROR;
  }

  ProtocolDB = JsonObjectGetValue(RootDB, "protocols");
  if (ProtocolDB == NULL) {
    SBError("Failed to get protocols from database\n");
    return EFI_LOAD_ERROR;
  }

  FunctionDB = JsonObjectGetValue(RootDB, "functions");
  if (FunctionDB == NULL) {
    SBError("Failed to get functions from database\n");
    return EFI_LOAD_ERROR;
  }

  TypeDB = JsonObjectGetValue(RootDB, "types");
  if (TypeDB == NULL) {
    SBError("Failed to get types from database\n");
    return EFI_LOAD_ERROR;
  }

  SynonymDB = JsonObjectGetValue(RootDB, "synonyms");
  if (SynonymDB == NULL) {
    SBError("Failed to get synonyms from database\n");
    return EFI_LOAD_ERROR;
  }

  EnumArray = JsonObjectGetValue(RootDB, "enums");
  if (EnumArray == NULL) {
    SBError("Failed to get enumerates from database\n");
    return EFI_LOAD_ERROR;
  }
  EnumArraySize = JsonArrayCount(EnumArray);

  ProtocolMap = hashmap_new(sizeof(struct ProtocolKV), 0, 0, 0, ProtocolKV_Hash, ProtocolKV_Compare, NULL, NULL);
  if (ProtocolMap == NULL) {
    SBError("Failed to create Protocol hashmap\n");
    return EFI_OUT_OF_RESOURCES;
  }

  TypeMap = hashmap_new(sizeof(struct TypeKV), 0, 0, 0, TypeKV_Hash, TypeKV_Compare, NULL, NULL);
  if (TypeMap == NULL) {
    SBError("Failed to create Type hashmap\n");
    return EFI_OUT_OF_RESOURCES;
  }

  Status = LoadProtocols();
  if (EFI_ERROR(Status)) {
    SBError("Failed to load protocols\n");
    return Status;
  }

  Status = LoadBasicTypes();
  if (EFI_ERROR(Status)) {
    SBError("Failed to load basic types\n");
    return Status;
  }

  return EFI_SUCCESS;
}
