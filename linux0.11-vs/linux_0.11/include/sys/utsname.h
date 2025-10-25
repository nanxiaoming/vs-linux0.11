#ifndef _SYS_UTSNAME_H
#define _SYS_UTSNAME_H

#include <sys\types.h>

struct utsname {
	CHAR sysname[ 9 ];
	CHAR nodename[ 9 ];
	CHAR release[ 9 ];
	CHAR version[ 9 ];
	CHAR machine[ 9 ];
};

extern int uname( struct utsname * utsbuf );

#endif
