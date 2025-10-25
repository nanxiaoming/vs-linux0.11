/*
 *  linux/lib/_exit.c
 *
 *  ( C ) 1991  Linus Torvalds
 */

#define __LIBRARY__
#include <unistd.h>

volatile VOID _exit( LONG exit_code )
{
	__asm mov eax, __NR_exit
	__asm mov ebx, exit_code
	__asm int 0x80
}
