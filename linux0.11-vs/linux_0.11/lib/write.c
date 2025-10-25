/*
*  linux/lib/write.c
*
*  ( C ) 1991  Linus Torvalds
*/

#define __LIBRARY__
#include <unistd.h>
#include <sys\types.h>
_syscall3( int , write, LONG , fd, UCHAR *, buf, off_t, count )
