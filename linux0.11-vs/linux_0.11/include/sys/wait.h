#ifndef _SYS_WAIT_H
#define _SYS_WAIT_H

#include <sys\types.h>

#define _LOW( v )		( ( v ) & 0377 )
#define _HIGH( v )	( ( ( v ) >> 8 ) & 0377 )

/* options for waitpid, WUNTRACED not supported */
#define WNOHANG		1
#define WUNTRACED	2

#define WIFEXITED( s )	( !( ( s )&0xFF )
#define WIFSTOPPED( s )	( ( ( s )&0xFF )==0x7F )
#define WEXITSTATUS( s )	( ( ( s )>>8 )&0xFF )
#define WTERMSIG( s )	( ( s )&0x7F )
#define WSTOPSIG( s )	( ( ( s )>>8 )&0xFF )
#define WIFSIGNALED( s )	( ( ( ULONG )( s )-1 & 0xFFFF ) < 0xFF )

pid_t wait( LONG *stat_loc );
pid_t waitpid( pid_t pid, LONG *stat_loc, LONG options );

#endif
