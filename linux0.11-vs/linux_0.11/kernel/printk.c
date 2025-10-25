/*
*  linux/kernel/printk.c
*
*  ( C ) 1991  Linus Torvalds
*/

/*
* When in kernel-mode, we cannot use printf, as fs is liable to
* point to 'interesting' things. Make a printf with fs-saving, and
* all is well.
*/
#include <stdarg.h>
#include <stddef.h>

#include <linux/kernel.h>

static CHAR buf[ 1024 ];

extern LONG vsprintf( CHAR * buf, const CHAR * fmt, va_list args );

LONG printk( const CHAR *fmt, ... )
{
	va_list args;
	LONG i;

	va_start( args, fmt );
	i = vsprintf( buf, fmt, args );
	va_end( args );
	__asm push	fs
	__asm push	ds
	__asm pop	fs
	tty_write( 0, buf, i );
	__asm pop	fs

	return i;
}
