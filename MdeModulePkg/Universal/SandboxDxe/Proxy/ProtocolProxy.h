#ifndef SANDBOX_PROTOCOL_PROXY_H_
#define SANDBOX_PROTOCOL_PROXY_H_

#include <Library/JsonLib.h>

struct BasicType {
  const CHAR8 *TypeName;
  UINT64 TypeSize;
};

// Custom defined structs
struct CustomType {
  const CHAR8 *TypeName;
  UINT64 TypeSize;
  BOOLEAN IsStruct;
  LIST_ENTRY Fields;
};

typedef enum {
  BasicTypeKind,
  CustomTypeKind,
  ProtocolKind,
  FunctionKind,
} TypeKind;

struct Type {
  TypeKind Kind;
  union {
    struct BasicType *BasicType;
    struct CustomType *CustomType;
    struct Protocol *Protocol;
    struct Function *Function;
  };
};

struct GenericField {
  const CHAR8 *FieldName;
  UINTN PointerLevel;
  UINTN ArraySize;
  struct Type *FieldType;
};

struct Field {
  LIST_ENTRY FieldNode;
  UINT64 Offset;
  const CHAR8 *FieldName;
  UINTN PointerLevel;
  UINTN ArraySize;
  struct Type *FieldType;
};

struct Variable {
  const CHAR8 *VariableName;
  UINTN PointerLevel;
  UINTN ArraySize;
  struct Type *VariableType;
};

struct Param {
  LIST_ENTRY ParamNode;

  const CHAR8 *ParamName;
  UINT64 Offset;
  UINT64 PointerLevel;
  BOOLEAN InParam;
  BOOLEAN OutParam;
  BOOLEAN Optional;
  struct Type *ParamType;
};

struct Function {
  const CHAR8 *FunctionName;
  LIST_ENTRY FunctionParams;
};

struct ProtocolField {
  LIST_ENTRY ProtocolFieldNode;

  UINT64 Offset;
  BOOLEAN IsFunction;
  union {
    struct Function *Function;
    struct Variable *Variable;
  };
};

/* Store used Protocols in hash table, index with GUID */
struct Protocol {
  const CHAR8 *ProtocolName;
  EFI_GUID Guid;
  UINT64 ProtocolSize;
  LIST_ENTRY FieldsList;
  EDKII_JSON_VALUE ProtocolJson;
};

EFI_STATUS InitProtocolDB();
EFI_STATUS GetProtocol(const EFI_GUID *Guid, struct Protocol **Protocol);
EFI_STATUS GetFunctionByOffset(const struct Protocol *Protocol, UINT64 Offset,
                               struct Function **Function);
EFI_STATUS GetTypeByName(const CHAR8 *TypeName, struct Type **Type);

#endif
