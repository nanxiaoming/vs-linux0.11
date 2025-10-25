/*
 *  linux/lib/wait.c
 *
 *  ( C ) 1991  Linus Torvalds
 */

#define __LIBRARY__
#include <unistd.h>
#include <sys\wait.h>

_syscall3( pid_t, waitpid, pid_t, pid, LONG *, wait_stat, LONG, options )

pid_t wait( LONG * wait_stat )
{
	return waitpid( -1, wait_stat, 0 );
}
