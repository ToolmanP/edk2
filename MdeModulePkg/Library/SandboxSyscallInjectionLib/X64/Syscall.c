/*
 * Copyright (c) 2023 Institute of Parallel And Distributed Systems (IPADS), Shanghai Jiao Tong University (SJTU)
 * Licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include "../SandboxSyscallWrapper.h"

__inline UINTN SandboxSyscall0(UINTN n)
{
	UINTN ret;
	__asm__ __volatile__ ("syscall" : "=a"(ret) : "a"(n) : "rcx", "r11", "memory");
	return ret;
}

__inline UINTN SandboxSyscall1(UINTN n, UINTN a1)
{
	UINTN ret;
	__asm__ __volatile__ ("syscall" : "=a"(ret) : "a"(n), "D"(a1) : "rcx", "r11", "memory");
	return ret;
}

__inline UINTN SandboxSyscall2(UINTN n, UINTN a1, UINTN a2)
{
	UINTN ret;
	__asm__ __volatile__ ("syscall" : "=a"(ret) : "a"(n), "D"(a1), "S"(a2)
						  : "rcx", "r11", "memory");
	return ret;
}

__inline UINTN SandboxSyscall3(UINTN n, UINTN a1, UINTN a2, UINTN a3)
{
	UINTN ret;
	__asm__ __volatile__ ("syscall" : "=a"(ret) : "a"(n), "D"(a1), "S"(a2),
						  "d"(a3) : "rcx", "r11", "memory");
	return ret;
}

__inline UINTN SandboxSyscall4(UINTN n, UINTN a1, UINTN a2, UINTN a3, UINTN a4)
{
	UINTN ret;
	register UINTN r10 __asm__("r10") = a4;
	__asm__ __volatile__ ("syscall" : "=a"(ret) : "a"(n), "D"(a1), "S"(a2),
						  "d"(a3), "r"(r10): "rcx", "r11", "memory");
	return ret;
}

__inline UINTN SandboxSyscall5(UINTN n, UINTN a1, UINTN a2, UINTN a3, UINTN a4, UINTN a5)
{
	UINTN ret;
	register UINTN r10 __asm__("r10") = a4;
	register UINTN r8 __asm__("r8") = a5;
	__asm__ __volatile__ ("syscall" : "=a"(ret) : "a"(n), "D"(a1), "S"(a2),
						  "d"(a3), "r"(r10), "r"(r8) : "rcx", "r11", "memory");
	return ret;
}

__inline UINTN SandboxSyscall6(UINTN n, UINTN a1, UINTN a2, UINTN a3, UINTN a4, UINTN a5, UINTN a6)
{
	UINTN ret;
	register UINTN r10 __asm__("r10") = a4;
	register UINTN r8 __asm__("r8") = a5;
	register UINTN r9 __asm__("r9") = a6;
	__asm__ __volatile__ ("syscall" : "=a"(ret) : "a"(n), "D"(a1), "S"(a2),
						  "d"(a3), "r"(r10), "r"(r8), "r"(r9) : "rcx", "r11", "memory");
	return ret;
}
