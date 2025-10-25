/*
 *  linux/lib/execve.c
 *
 *  ( C ) 1991  Linus Torvalds
 */

#define __LIBRARY__
#include <unistd.h>


/*
 * 以第一次 execve( "/bin/sh", argv, envp ) 为例,
 * 此次调用是第二个进程,因此线性地址基础加上 0x08000000
 * 
 * 
 * <bochs:30> r
 *	rax: 00000000_0000000b	->  0xb 11 为 __NR_execve 的序号
 *	rbx: 00000000_00012608
 *	rcx: 00000000_00014048
 *	rdx: 00000000_00014050
 *	rsp: 00000000_0001ad6c
 *	rbp: 00000000_0001ad74
 *	rsi: 00000000_00000000
 *	rdi: 00000000_00000000
 * 
 * 第一个参数
 * 
 *	<bochs:17> print-string 0x8012608
 *	0x08012608: /bin/sh
 * 
 * 第二个参数
 * 
 *	0x0000000008014048 <bogus+       0>:    0x00012608
 *	<bochs:19> print-string 0x8012608
 *	0x08012608: /bin/sh
 * 
 * 第三个参数
 *	<bochs:31> x /1w 0x8014050
 *	[bochs]:
 *	0x0000000008014050 <bogus+       0>:    0x00012610
 *
 *	<bochs:33> print-string 0x8012610
 *	0x08012610: HOME=/
 * 
 * 
 *	#define _syscall3( type,name,atype,a,btype,b,ctype,c ) \
 * 		type name( atype a, btype b, ctype c ) \
 * 		{ \
 * 		int __res; \
 * 		{ \
 * 		__asm mov	eax, __NR_##name \		-> 0xb
 * 		__asm mov	ebx, a \				-> 0x012608 filename
 * 		__asm mov	ecx, b \				-> 0x014048 argv
 * 		__asm mov	edx, c \				-> 0x014050 envp
 * 		__asm int	0x80 \					-> 此处是应用层最后一句话,然后进入内核
 * 		__asm mov	__res, eax \               应用层和内核层各有自己的栈信息,参数使用寄存器传递
 * 		} \
 * 		if ( __res >= 0 ) \
 * 		return ( type )__res; \
 * 		errno = -__res; \
 * 		return -1; \
 * 		}
 * 
 */

_syscall3( int, execve, const CHAR *, file, CHAR **, argv, CHAR **, envp )
