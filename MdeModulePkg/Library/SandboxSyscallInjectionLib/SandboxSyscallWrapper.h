#ifndef SANDBOX_SYSCALL_H_
#define SANDBOX_SYSCALL_H_

UINTN SandboxSyscall0(UINTN n);
UINTN SandboxSyscall1(UINTN n, UINTN a);
UINTN SandboxSyscall2(UINTN n, UINTN a, UINTN b);
UINTN SandboxSyscall3(UINTN n, UINTN a, UINTN b, UINTN c);
UINTN SandboxSyscall4(UINTN n, UINTN a, UINTN b, UINTN c, UINTN d);
UINTN SandboxSyscall5(UINTN n, UINTN a, UINTN b, UINTN c, UINTN d, UINTN e);
UINTN SandboxSyscall6(UINTN n, UINTN a, UINTN b, UINTN c, UINTN d, UINTN e, UINTN f);
UINTN SandboxSyscall7(UINTN n, UINTN a, UINTN b, UINTN c, UINTN d, UINTN e, UINTN f, UINTN g);

#endif
