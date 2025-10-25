#ifndef _TIMES_H
#define _TIMES_H

#include <sys\types.h>

struct tms 
{
	time_t tms_utime;	// Offset=0x0 Size=0x4
	time_t tms_stime;	// Offset=0x4 Size=0x4
	time_t tms_cutime;	// Offset=0x8 Size=0x4
	time_t tms_cstime;	// Offset=0xc Size=0x4
};

extern time_t times( struct tms * tp );

#endif
