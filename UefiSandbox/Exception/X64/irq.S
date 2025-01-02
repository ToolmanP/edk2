#include "Exception/Asm.h"
#include "Init/X64/CpuInfo.h"

.extern SandboxServicesManager
.extern mCpuInfo

.text
.align  8
BEGIN_FUNC(IretToSandbox)
    # 1) 关中断
    cli

    # ----------------------------------------------------------------
    # 3) 读取 Context->UserFunc, UserReturnAddr, UserStackTop, Param1, Param2
    # ----------------------------------------------------------------
    movq   776(%rdi), %rbx    # %rbx = UserFunc
    movq   768(%rdi), %rsi    # %rsi = UserReturnAddr
    movq   792(%rdi), %r10    # %r10 = UserStackTop (使用 %r10 代替 %rdx)
    movq   800(%rdi), %r9     # %r9  = Param1
    movq   784(%rdi), %rax    # %rax = Param2

    # ----------------------------------------------------------------
    # 4) 设置参数传递寄存器为 RCX 和 RDX (ms_abi for drivers)
    # ----------------------------------------------------------------
    movq   %r9,  %rcx          # rcx = Param1
    movq   %rax, %rdx          # rdx = Param2
    # 现在:
    #   %rbx = UserFunc
    #   %rsi = UserReturnAddr
    #   %r10 = UserStackTop
    #   %rcx = Param1
    #   %rdx = Param2

    # ----------------------------------------------------------------
    # 5) reserved 32 bytes on user stack for shadow space in ms_abi
    # ---------------------------------------------------------------- 
    movq   %r10, %rax          # %rax = UserStackTop
    subq   $32,  %rax

    # ----------------------------------------------------------------
    # 6) 在“用户栈” (r10) 上预留 8 字节给“返回地址” => user_return
    # ----------------------------------------------------------------
    subq   $8,   %rax          # %rax = UserStackTop - 40
    movq   %rsi, (%rax)        # [UserStackTop - 40] = UserReturnAddr

    # ----------------------------------------------------------------
    # 7) 构造 iretq 的弹栈顺序
    #    iretq(特权级切换) 会 pop => RIP, CS, RFLAGS, RSP, SS
    #    => 压栈顺序(逆序): SS, RSP, RFLAGS, CS, RIP
    # ----------------------------------------------------------------
    pushq  $0x43               # SS (DPL=3) USER_DATA_SEL
    pushq  %rax                # RSP => (UserStackTop - 40)
    pushq  $0x202              # RFLAGS => IF=1, bit1=1
    pushq  $0x4b               # CS (DPL=3) USER_CODE_SEL
    pushq  %rbx                # RIP => UserFunc

    # 当 iretq 执行时，会将 RIP 设置为 UserFunc，CS 设置为 USER_CODE_SEL 等。
    iretq

    # 如果地址不合法 => #GP
    hlt

END_FUNC(IretToSandbox)

.text
.align  8
BEGIN_FUNC(SyscallEntry)
    cli

    # Save syscall number to CpuInfo->CurrentSyscallNumber
    movq    %rax, mCpuInfo(%rip)             # CpuInfo.CurrentSyscallNumber offset is 0x00

    # Load CpuInfo.Context into rax
    movq    mCpuInfo+0x10(%rip), %rax             # CpuInfo.Context offset is 0x10

    # Save user registers to CpuInfo->Context
    movq    %rdi, 728(%rax)            # Rdi offset is 728
    movq    %rsi, 736(%rax)            # Rsi offset is 736
    movq    %rbp, 744(%rax)            # Rbp offset is 744
    # Save user stack pointer to CpuInfo->Context->Rsp
    movq    %rsp, 752(%rax)            # Rsp offset is 752
    movq    %rbx, 760(%rax)            # Rbx offset is 760
    movq    %rdx, 768(%rax)            # Rdx offset is 768
    # syscall instruction saves next user rip in rcx
    movq    %rcx, 776(%rax)            # Rcx offset is 776
    # omit %rax
    movq    %r8,  792(%rax)            # R8 offset is 792
    movq    %r9,  800(%rax)            # R9 offset is 800
    movq    %r10, 808(%rax)            # R10 offset is 808
    # syscall instruction saves user eflags in r11
    movq    %r11, 816(%rax)            # R11 offset is 816
    movq    %r12, 824(%rax)            # R12 offset is 824
    movq    %r13, 832(%rax)            # R13 offset is 832
    movq    %r14, 840(%rax)            # R14 offset is 840
    movq    %r15, 848(%rax)            # R15 offset is 848

    # Set SS to LINEAR_DATA64_SEL
    movw    $0x30, %ax
    movw    %ax, %ss

    # Switch to kernel stack
    movq    mCpuInfo+0x08(%rip), %rsp             # CpuInfo.KernelStack offset is 0x08

    # Restore syscall number from CpuInfo->CurrentSyscallNumber
    movq    mCpuInfo(%rip), %rax             # Restore syscall number

    # Calculate the address of the function in the table
    leaq    SandboxServicesManager(%rip), %rbx   # Load address of the table
    movq    (%rbx,%rax,8), %rax     # Get function pointer (8 bytes per entry)

    # Prepare arguments for the function call
    # The syscall arguments are in %rdi, %rsi, %rdx, %r10, %r8, %r9
    # Need to move %r10 into %rcx as per calling convention
    movq    %r10, %rcx

    # Align the stack to 16 bytes as per System V AMD64 ABI before call
    # Calculate stack alignment
    # Save current stack pointer into %r11
    movq    %rsp, %r11             # Save current stack pointer
    andq    $-16, %rsp             # Align stack to 16 bytes
    subq    $8, %rsp               # Create space for return address

    sti                            # Enable interrupts

    # Call the function
    call    *%rax                  # Call the function pointer in %rax

    cli                            # Disable interrupts

    # Restore original stack pointer from %r11
    movq    %r11, %rsp

    # Function return value is in %rax
    # Store return value as needed; in syscalls, return value is in %rax
    movq    %rax, mCpuInfo(%rip)         # Store return value in CpuInfo.CurrentSyscallNumber

    # Load CpuInfo.Context into rax again
    movq    mCpuInfo+0x10(%rip), %rax         # CpuInfo.Context offset is 0x10

    # Restore user registers from CpuInfo->Context
    movq    848(%rax), %r15        # Restore R15
    movq    840(%rax), %r14        # Restore R14
    movq    832(%rax), %r13        # Restore R13
    movq    824(%rax), %r12        # Restore R12
    movq    816(%rax), %r11        # Restore R11
    movq    808(%rax), %r10        # Restore R10
    movq    800(%rax), %r9         # Restore R9
    movq    792(%rax), %r8         # Restore R8
    # omit %rax
    movq    776(%rax), %rcx        # Restore Rcx
    movq    768(%rax), %rdx        # Restore Rdx
    movq    760(%rax), %rbx        # Restore Rbx
    movq    752(%rax), %rsp        # Restore Rsp
    movq    744(%rax), %rbp        # Restore Rbp
    movq    736(%rax), %rsi        # Restore Rsi
    movq    728(%rax), %rdi        # Restore Rdi

    movq    mCpuInfo(%rip), %rax         # Restore syscall return value

    # sysretq requires %rcx = target RIP, %rsp = user stack
    sysretq                         # Return to user mode
END_FUNC(SyscallEntry)