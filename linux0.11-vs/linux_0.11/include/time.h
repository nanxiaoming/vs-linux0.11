#ifndef _TIME_H
#define _TIME_H

#include <sys\types.h>

#ifndef _TIME_T
#define _TIME_T
typedef long time_t;
#endif

#ifndef _SIZE_T
#define _SIZE_T
typedef unsigned long size_t;
#endif

#define CLOCKS_PER_SEC 100

typedef long clock_t;

struct tm {
	LONG tm_sec;			// Offset=0x0 Size=0x4
	LONG tm_min;			// Offset=0x4 Size=0x4
	LONG tm_hour;			// Offset=0x8 Size=0x4
	LONG tm_mday;			// Offset=0xc Size=0x4
	LONG tm_mon;			// Offset=0x10 Size=0x4
	LONG tm_year;			// Offset=0x14 Size=0x4
	LONG tm_wday;			// Offset=0x18 Size=0x4
	LONG tm_yday;			// Offset=0x1c Size=0x4
	LONG tm_isdst;			// Offset=0x20 Size=0x4
};

clock_t clock();
time_t time( time_t * tp );
double difftime( time_t time2, time_t time1 );
time_t mktime( struct tm * tp );

CHAR * asctime( const struct tm * tp );
CHAR * ctime( const time_t * tp );
struct tm * gmtime( const time_t *tp );
struct tm *localtime( const time_t * tp );
size_t strftime( CHAR * s, size_t smax, const CHAR * fmt, const struct tm * tp );
VOID tzset();

#endif
