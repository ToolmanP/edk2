#ifndef TSS_H_
#define TSS_H_

#define IOPB_SIZE 8192  // 65536 位，用于所有 I/O 端口

#pragma pack(1)
typedef struct {
    UINT32 Reserved0;
    UINT64 Rsp0;       // [0x04] 内核栈指针 (Ring0)
    UINT64 Rsp1;       // [0x0C] (Ring1)
    UINT64 Rsp2;       // [0x14] (Ring2)
    UINT64 Reserved1;
    UINT64 Ist1;       // [0x24] 可选的中断栈
    UINT64 Ist2;
    UINT64 Ist3;
    UINT64 Ist4;
    UINT64 Ist5;
    UINT64 Ist6;
    UINT64 Ist7;
    UINT64 Reserved2;
    UINT16 Reserved3;
    UINT16 IOPBOffset; // IO位图偏移
} Tss64;
#pragma pack()

#endif