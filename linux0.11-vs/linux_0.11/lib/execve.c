/*
 *  linux/lib/execve.c
 *
 *  ( C ) 1991  Linus Torvalds
 */

#define __LIBRARY__
#include <unistd.h>


/*
 * �Ե�һ�� execve( "/bin/sh", argv, envp ) Ϊ��,
 * �˴ε����ǵڶ�������,������Ե�ַ�������� 0x08000000
 * 
 * 
 * <bochs:30> r
 *	rax: 00000000_0000000b	->  0xb 11 Ϊ __NR_execve �����
 *	rbx: 00000000_00012608
 *	rcx: 00000000_00014048
 *	rdx: 00000000_00014050
 *	rsp: 00000000_0001ad6c
 *	rbp: 00000000_0001ad74
 *	rsi: 00000000_00000000
 *	rdi: 00000000_00000000
 * 
 * ��һ������
 * 
 *	<bochs:17> print-string 0x8012608
 *	0x08012608: /bin/sh
 * 
 * �ڶ�������
 * 
 *	0x0000000008014048 <bogus+       0>:    0x00012608
 *	<bochs:19> print-string 0x8012608
 *	0x08012608: /bin/sh
 * 
 * ����������
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
 * 		__asm int	0x80 \					-> �˴���Ӧ�ò����һ�仰,Ȼ������ں�
 * 		__asm mov	__res, eax \               Ӧ�ò���ں˲�����Լ���ջ��Ϣ,����ʹ�üĴ�������
 * 		} \
 * 		if ( __res >= 0 ) \
 * 		return ( type )__res; \
 * 		errno = -__res; \
 * 		return -1; \
 * 		}
 * 
 */

_syscall3( int, execve, const CHAR *, file, CHAR **, argv, CHAR **, envp )
