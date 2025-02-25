#include "Exception/Asm.h"
#include "Init/X64/CpuInfo.h"

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

.extern SandboxServicesManager
.extern mCpuInfos
.extern CurrentCpuInfoIndex

.extern TmpR12
.extern TmpR13
.extern TmpR14

#
# void SyscallEntry();
#
# The syscall arguments are in %rdi, %rsi, %rdx, %r10, %r8, %r9
# =======================================================================

.text
.align  8
BEGIN_FUNC(SyscallEntry)

    cli                             # Disable interrupts

    movq    %r12, TmpR12(%rip)      # Save %r12
    movq    %r13, TmpR13(%rip)      # Save %r13
    movq    %r14, TmpR14(%rip)      # Save %r14

    movq    CurrentCpuInfoIndex(%rip), %r13   # Load global CurrentCpuInfoIndex
    cmpq    $6, %r13                          # Compare CurrentCpuInfoIndex with 6
    jle     continue                          # Jump if CurrentCpuInfoIndex <= 6
    int     $3                                # Interrupt if CurrentCpuInfoIndex > 6

continue:
    #-------------------------------------------------------------------
    # Calculate the address of mCpuInfos[CurrentCpuInfoIndex]
    # Each CpuInfo structure has a size of 24 bytes (3 * 8)
    #-------------------------------------------------------------------
    leaq    mCpuInfos(%rip), %r12             # &mCpuInfos[0]
    movq    $24, %r14                         # Size of CpuInfo = 24
    imulq   %r13, %r14                        # %r14 = %r13 * 24
    addq    %r12, %r14                        # %r14 = &mCpuInfos[0] + (index * 24)

    #-------------------------------------------------------------------
    # CurrentCpuInfoIndex += 1
    #-------------------------------------------------------------------
    addq    $1, %r13                          # Increment CurrentCpuInfoIndex
    movq    %r13, CurrentCpuInfoIndex(%rip)   # Write back to global variable

    #-------------------------------------------------------------------
    # Store the syscall number into the current CpuInfo->CurrentSyscallNumber (offset 0)
    #-------------------------------------------------------------------
    movq    %rax, (%r14)

    #-------------------------------------------------------------------
    # Retrieve the context pointer from CpuInfo->Context (offset 16) into %rax
    #-------------------------------------------------------------------
    movq    16(%r14), %rax                    # %rax = mCpuInfos[index].Context

    #-------------------------------------------------------------------
    # Save user-mode registers to memory pointed by CpuInfo->Context
    #   (Offsets 728/736/... correspond to register locations in EFI_SYSTEM_CONTEXT_X64)
    #-------------------------------------------------------------------
    movq    %rdi, 728(%rax)
    movq    %rsi, 736(%rax)
    movq    %rbp, 744(%rax)
    movq    %rsp, 752(%rax)
    movq    %rbx, 760(%rax)
    movq    %rdx, 768(%rax)
    movq    %rcx, 776(%rax)
    movq    %r8,  792(%rax)
    movq    %r9,  800(%rax)
    movq    %r10, 808(%rax)
    movq    %r11, 816(%rax)
    movq    %r15, 848(%rax)

    movq    TmpR12(%rip), %r15
    movq    %r15, 824(%rax)
    movq    TmpR13(%rip), %r15
    movq    %r15, 832(%rax)
    movq    TmpR14(%rip), %r15
    movq    %r15, 840(%rax)

    #-------------------------------------------------------------------
    # Set SS = 0x30 (LINEAR_DATA64_SEL)
    #-------------------------------------------------------------------
    movw    $0x30, %ax
    movw    %ax, %ss

    #-------------------------------------------------------------------
    # Switch to kernel stack (CpuInfo->KernelStackTop, offset 8)
    #-------------------------------------------------------------------
    movq    8(%r14), %rsp                     # %rsp = mCpuInfos[index].KernelStackTop

    #-------------------------------------------------------------------
    # Retrieve the syscall number back into %rax 
    #   (CpuInfo->CurrentSyscallNumber, offset 0)
    #-------------------------------------------------------------------
    movq    (%r14), %rax

    #-------------------------------------------------------------------
    # Find the function pointer in SandboxServicesManager based on the syscall number
    #-------------------------------------------------------------------
    leaq    SandboxServicesManager(%rip), %r12
    movq    (%r12,%rax,8), %rax

    #-------------------------------------------------------------------
    # According to SysV AMD64 calling convention, the 4th syscall argument 
    # should be placed in %rcx (originally in %r10, needs to be moved to %rcx)
    #-------------------------------------------------------------------
    movq    %r10, %rcx

    sti                                     # Enable interrupts
    call    *%rax                           # Call function pointer
    cli                                     # Disable interrupts after returning

    #-------------------------------------------------------------------
    # Decrement CurrentCpuInfoIndex
    #-------------------------------------------------------------------
    movq    CurrentCpuInfoIndex(%rip), %r13
    subq    $1, %r13
    movq    %r13, CurrentCpuInfoIndex(%rip)

    # ? omit getting the CpuInfo pointer again?
    # leaq    mCpuInfos(%rip), %r12             # &mCpuInfos[0]
    # movq    $24, %r14                         # Size of CpuInfo = 24
    # imulq   %r13, %r14                        # %r14 = %r13 * 24
    # addq    %r12, %r14                        # %r14 = &mCpuInfos[0] + (index * 24)

    #-------------------------------------------------------------------
    # Store the return value into CpuInfo->CurrentSyscallNumber (offset 0)
    #-------------------------------------------------------------------
    movq    %rax, (%r14)

    #-------------------------------------------------------------------
    # Retrieve CpuInfo->Context (offset 16) again for restoring user registers
    #-------------------------------------------------------------------
    movq    16(%r14), %rax

    #-------------------------------------------------------------------
    # Restore user-mode registers
    #-------------------------------------------------------------------
    movq    848(%rax), %r15
    movq    840(%rax), %r13
    movq    %r13, TmpR14(%rip)
    movq    832(%rax), %r13
    movq    824(%rax), %r12
    movq    816(%rax), %r11
    movq    808(%rax), %r10
    movq    800(%rax), %r9
    movq    792(%rax), %r8
    movq    776(%rax), %rcx
    movq    768(%rax), %rdx
    movq    760(%rax), %rbx
    movq    752(%rax), %rsp
    movq    744(%rax), %rbp
    movq    736(%rax), %rsi
    movq    728(%rax), %rdi

    #-------------------------------------------------------------------
    # Retrieve the return value (just stored in CpuInfo->CurrentSyscallNumber) back to %rax
    #-------------------------------------------------------------------
    movq    (%r14), %rax

    movq    TmpR14(%rip), %r14

    #-------------------------------------------------------------------
    # sysretq: Return to user mode
    #   AMD documentation requires %rcx = RIP after return, %rsp = user stack
    #   This matches the previously saved/restored user context, so directly sysretq
    #-------------------------------------------------------------------
    sysretq

END_FUNC(SyscallEntry)