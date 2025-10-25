#ifndef _UTIME_H
#define _UTIME_H

#include <sys\types.h>	/* I know - shouldn't do this, but .. */

struct utimbuf 
{
	time_t actime;	// Offset=0x0 Size=0x4
	time_t modtime;	// Offset=0x4 Size=0x4
};

extern int utime( const CHAR *filename, struct utimbuf *times );

#endif
