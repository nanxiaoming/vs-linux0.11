#ifndef _STDARG_H
#define _STDARG_H

#include <sys\types.h>
typedef CHAR *va_list;

/* Amount of space required in an argument list for an arg of type TYPE.
TYPE may alternatively be an expression whose type is used.  */

#define __va_rounded_size( TYPE )  \
	( ( ( sizeof ( TYPE ) + sizeof ( LONG ) - 1 ) / sizeof ( LONG ) ) * sizeof ( LONG ) )

#ifndef __sparc__
#define va_start( AP, LASTARG ) 						\
	( AP = ( ( CHAR * ) &( LASTARG ) + __va_rounded_size ( LASTARG ) ) )
#else
#define va_start( AP, LASTARG ) 						\
	( __builtin_saveregs (),						\
	AP = ( ( CHAR * ) &( LASTARG ) + __va_rounded_size ( LASTARG ) ) )
#endif

VOID va_end( va_list );		/* Defined in gnulib */
#define va_end( AP )

#define va_arg( AP, TYPE )						\
	( AP += __va_rounded_size ( TYPE ),					\
	*( ( TYPE * ) ( AP - __va_rounded_size ( TYPE ) ) ) )

#endif /* _STDARG_H */
