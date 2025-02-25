#include "../SandboxSyscallWrapper.h"

#define __asm_syscall(...) do { \
	__asm__ __volatile__ ( "svc 0" \
	: "=r"(x0) : __VA_ARGS__ : "memory", "cc"); \
	return x0; \
	} while (0)

inline UINTN SandboxSyscall0(UINTN n)
{
	register UINTN x8 __asm__("x8") = n;
	register UINTN x0 __asm__("x0");
	__asm_syscall("r"(x8));
}

inline UINTN SandboxSyscall1(UINTN n, UINTN a)
{
	register UINTN x8 __asm__("x8") = n;
	register UINTN x0 __asm__("x0") = a;
	__asm_syscall("r"(x8), "0"(x0));
}

inline UINTN SandboxSyscall2(UINTN n, UINTN a, UINTN b)
{
	register UINTN x8 __asm__("x8") = n;
	register UINTN x0 __asm__("x0") = a;
	register UINTN x1 __asm__("x1") = b;
	__asm_syscall("r"(x8), "0"(x0), "r"(x1));
}

inline UINTN SandboxSyscall3(UINTN n, UINTN a, UINTN b, UINTN c)
{
	register UINTN x8 __asm__("x8") = n;
	register UINTN x0 __asm__("x0") = a;
	register UINTN x1 __asm__("x1") = b;
	register UINTN x2 __asm__("x2") = c;
	__asm_syscall("r"(x8), "0"(x0), "r"(x1), "r"(x2));
}

inline UINTN SandboxSyscall4(UINTN n, UINTN a, UINTN b, UINTN c, UINTN d)
{
	register UINTN x8 __asm__("x8") = n;
	register UINTN x0 __asm__("x0") = a;
	register UINTN x1 __asm__("x1") = b;
	register UINTN x2 __asm__("x2") = c;
	register UINTN x3 __asm__("x3") = d;
	__asm_syscall("r"(x8), "0"(x0), "r"(x1), "r"(x2), "r"(x3));
}

inline UINTN SandboxSyscall5(UINTN n, UINTN a, UINTN b, UINTN c, UINTN d, UINTN e)
{
	register UINTN x8 __asm__("x8") = n;
	register UINTN x0 __asm__("x0") = a;
	register UINTN x1 __asm__("x1") = b;
	register UINTN x2 __asm__("x2") = c;
	register UINTN x3 __asm__("x3") = d;
	register UINTN x4 __asm__("x4") = e;
	__asm_syscall("r"(x8), "0"(x0), "r"(x1), "r"(x2), "r"(x3), "r"(x4));
}

inline UINTN SandboxSyscall6(UINTN n, UINTN a, UINTN b, UINTN c, UINTN d, UINTN e, UINTN f)
{
	register UINTN x8 __asm__("x8") = n;
	register UINTN x0 __asm__("x0") = a;
	register UINTN x1 __asm__("x1") = b;
	register UINTN x2 __asm__("x2") = c;
	register UINTN x3 __asm__("x3") = d;
	register UINTN x4 __asm__("x4") = e;
	register UINTN x5 __asm__("x5") = f;
	__asm_syscall("r"(x8), "0"(x0), "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5));
}

inline UINTN SandboxSyscall7(UINTN n, UINTN a, UINTN b, UINTN c, UINTN d, UINTN e, UINTN f, UINTN g)
{
	register UINTN x8 __asm__("x8") = n;
	register UINTN x0 __asm__("x0") = a;
	register UINTN x1 __asm__("x1") = b;
	register UINTN x2 __asm__("x2") = c;
	register UINTN x3 __asm__("x3") = d;
	register UINTN x4 __asm__("x4") = e;
	register UINTN x5 __asm__("x5") = f;
	register UINTN x6 __asm__("x6") = g;
	__asm_syscall("r"(x8), "0"(x0), "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5), "r"(x6));
}

