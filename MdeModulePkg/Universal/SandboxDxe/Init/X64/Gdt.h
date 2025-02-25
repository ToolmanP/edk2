#ifndef SANDBOX_GDT_H_
#define SANDBOX_GDT_H_

//
// Local structure definitions
//

#pragma pack (1)

//
// Global Descriptor Entry structures
//

typedef struct _GDT_ENTRY {
  UINT16    Limit15_0;
  UINT16    Base15_0;
  UINT8     Base23_16;
  UINT8     Type;
  UINT8     Limit19_16_and_flags;
  UINT8     Base31_24;
} GDT_ENTRY;

typedef struct {
  UINT16 LimitLow;
  UINT16 BaseLow;
  UINT8  BaseMid;
  UINT8  Access;
  UINT8  Granularity;
  UINT8  BaseHigh;
  UINT32 BaseUpper;
  UINT32 Reserved;
} TSS_ENTRY;

typedef
  struct _GDT_ENTRIES {
  GDT_ENTRY    Null;
  GDT_ENTRY    Linear;
  GDT_ENTRY    LinearCode;
  GDT_ENTRY    SysData;
  GDT_ENTRY    SysCode;
  GDT_ENTRY    SysCode16;
  GDT_ENTRY    LinearData64;
  GDT_ENTRY    LinearCode64;
  GDT_ENTRY    UserData64;
  GDT_ENTRY    UserCode64;
  TSS_ENTRY    Tss;
  GDT_ENTRY    Spare5;
} GDT_ENTRIES;

#pragma pack ()

#define GDT_NULL           0
#define GDT_LINEAR         1
#define GDT_LINEAR_CODE    2
#define GDT_SYS_DATA       3
#define GDT_SYS_CODE       4
#define GDT_SYS_CODE16     5
#define GDT_LINEAR_DATA64  6
#define GDT_LINEAR_CODE64  7
#define GDT_USER_DATA64    8
#define GDT_USER_CODE64    9
#define GDT_TSS            10
#define GDT_SPARE5         11

#define NULL_SEL           OFFSET_OF (GDT_ENTRIES, Null)
#define LINEAR_SEL         OFFSET_OF (GDT_ENTRIES, Linear)
#define LINEAR_CODE_SEL    OFFSET_OF (GDT_ENTRIES, LinearCode)
#define SYS_DATA_SEL       OFFSET_OF (GDT_ENTRIES, SysData)
#define SYS_CODE_SEL       OFFSET_OF (GDT_ENTRIES, SysCode)
#define SYS_CODE16_SEL     OFFSET_OF (GDT_ENTRIES, SysCode16)
#define LINEAR_DATA64_SEL  OFFSET_OF (GDT_ENTRIES, LinearData64)
#define LINEAR_CODE64_SEL  OFFSET_OF (GDT_ENTRIES, LinearCode64)
#define UDATA64_SEL    OFFSET_OF (GDT_ENTRIES, UserData64)
#define UCODE64_SEL    OFFSET_OF (GDT_ENTRIES, UserCode64)
#define TSS_SEL            OFFSET_OF (GDT_ENTRIES, Tss)
#define SPARE5_SEL         OFFSET_OF (GDT_ENTRIES, Spare5)

#define KERNEL_CODE_SEL  LINEAR_CODE64_SEL
#define KERNEL_DATA_SEL  LINEAR_DATA64_SEL
#define USER_DATA_SEL   (UDATA64_SEL | 0x3)
#define USER_CODE_SEL   (UCODE64_SEL | 0x3)

#endif // _CPU_GDT_H_
