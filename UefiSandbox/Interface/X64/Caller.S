#include "Exception/Asm.h"

/*
 * EFI_STATUS CallCoreFunc(UINT64 Params[6], UINT64 FunctionAddress)
 */
.text
.align  8
BEGIN_FUNC(CallCoreFunc)
  # rdi = Params (指向包含6个UINT64的数组)
  # rsi = FunctionAddress (函数地址)

  movq %rdi, %r12
  movq %rsi, %rax

  # FIXME: MS ABI
  movq 0(%r12), %rdi        # Params[0] -> rdi
  movq 8(%r12), %rsi        # Params[1] -> rsi
  movq 16(%r12), %rdx       # Params[2] -> rdx
  movq 24(%r12), %rcx       # Params[3] -> rcx
  movq 32(%r12), %r8        # Params[4] -> r8
  movq 40(%r12), %r9        # Params[5] -> r9

  # 调用指定的 FunctionAddress
  call *%rax

  ret
END_FUNC(CallCoreFunc)

/* void CallSandboxFunc(InterfaceParams *) */
.text
.align  8
BEGIN_FUNC(CallSandboxFunc)
    # 1) 关中断（可选）
    cli

    # ----------------------------------------------------------------
    # 2) 从 InterfaceParams 结构体中读取各个参数
    # ----------------------------------------------------------------
    movq    0x00(%rdi), %r10          # %r10 = R0
    movq    0x08(%rdi), %r11          # %r11 = R1
    movq    0x10(%rdi), %r12          # %r12 = R2
    movq    0x18(%rdi), %r13          # %r13 = R3
    movq    0x20(%rdi), %r14          # %r14 = R4
    movq    0x28(%rdi), %r15          # %r15 = R5
    movq    0x30(%rdi), %rbx          # %rbx = Function Entrypoint
    movq    0x38(%rdi), %rsi          # %rsi = Return Trampoline
    movq    0x40(%rdi), %rax          # %rax = User Stack (RSP)


    # ----------------------------------------------------------------
    # 3) 设置函数参数寄存器为 RCX, RDX, R8, R9（ms_abi 前四个参数）
    # ----------------------------------------------------------------
    movq    %r10, %rcx                # rcx = R0
    movq    %r11, %rdx                # rdx = R1
    movq    %r12, %r8                 # r8  = R2
    movq    %r13, %r9                 # r9  = R3

    # ----------------------------------------------------------------
    # 4) 在用户栈上预留空间并传递额外参数 R4 和 R5 (ms_abi 只有四个参数在寄存器)
    # ----------------------------------------------------------------
    subq    $16, %rax                 # 为 R4 和 R5 预留 16 字节
    movq    %r14, (%rax)              # [RSP - 16] = R4
    movq    %r15, 8(%rax)             # [RSP - 8]  = R5

    # ---------------------------------------------------------------
    # 5) reserved 32 bytes on user stack for shadow space in ms_abi
    # ---------------------------------------------------------------
    subq    $32, %rax                # RSP - 48

    # ----------------------------------------------------------------
    # 6) 设置返回跳转地址（Return Trampoline）在用户栈上
    # ----------------------------------------------------------------
    subq    $8, %rax                  # 为 Return Trampoline 预留 8 字节
    movq    %rsi, (%rax)             # [RSP - 50] = Return Trampoline

    # ----------------------------------------------------------------
    # 7) 构造 iretq 所需的栈帧
    #    iretq 会弹出：RIP, CS, RFLAGS, RSP, SS
    #    所以需要按逆序压入：SS, RSP, RFLAGS, CS, RIP
    # ----------------------------------------------------------------
    pushq  $0x43               # SS (DPL=3) USER_DATA_SEL
    pushq  %rax                # RSP => (UserStackTop - 50)
    pushq  $0x202              # RFLAGS => IF=1, bit1=1
    pushq  $0x4b               # CS (DPL=3) USER_CODE_SEL
    pushq  %rbx                # RIP => UserFunc

    # ----------------------------------------------------------------
    # 8) 执行 iretq 进行特权级切换并跳转到用户态函数
    # ----------------------------------------------------------------
    iretq
END_FUNC(CallSandboxFunc)