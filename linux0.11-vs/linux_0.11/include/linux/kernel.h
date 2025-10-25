/*
* 'kernel.h' contains some often-used function prototypes etc
*/

#include <sys\types.h>

VOID verify_area( VOID * addr, LONG count );
volatile VOID panic( const CHAR * str );
LONG printf( const CHAR * fmt, ... );
LONG printk( const CHAR * fmt, ... );
LONG tty_write( unsigned ch, CHAR * buf, LONG count );
VOID * malloc( ULONG size );
VOID free_s( VOID * obj, LONG size );

#define free( x ) free_s( ( x ), 0 )

/*
* This is defined as a macro, but at some point this might become a
* real subroutine that sets a flag if it returns true ( to do
* BSD-style accounting where the process is flagged if it uses root
* privs ).  The implication of this is that you should do normal
* permissions checks first, and check suser() last.
*/
#define suser() ( current->euid == 0 )

