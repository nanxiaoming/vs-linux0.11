#ifndef _BLK_H
#define _BLK_H

#include <linux\head.h>

#define NR_BLK_DEV	7
/*
* NR_REQUEST is the number of entries in the request-queue.
* NOTE that writes may use only the low 2/3 of these: reads
* take precedence.
*
* 32 seems to be a reasonable number: enough to get some benefit
* from the elevator-mechanism, but not so much as to lock a lot of
* buffers when they are in the queue. 64 seems to be too many ( easily
* LONG pauses in reading when heavy writing/syncing is going on )
*/
#define NR_REQUEST	32

/*
* Ok, this is an expanded form so that we can use the same
* request for paging requests when that is implemented. In
* paging, 'bh' is NULL, and 'waiting' is used to wait for
* read/write completion.
*/
typedef struct request
{
	LONG			dev;	/* -1 if no request */	// Offset=0x0 Size=0x4
	LONG			cmd;	/* READ or WRITE */		// Offset=0x4 Size=0x4
	LONG			errors;							// Offset=0x8 Size=0x4
	ULONG			sector;							// Offset=0xc Size=0x4
	ULONG			nr_sectors;						// Offset=0x10 Size=0x4
	CHAR 		*	buffer;							// Offset=0x14 Size=0x4
	Task_Struct *	waiting;						// Offset=0x18 Size=0x4
	Buffer_Head *	bh;								// Offset=0x1c Size=0x4
	struct request * next;							// Offset=0x20 Size=0x4
}Request;

/*
* This is used in the elevator algorithm: Note that
* reads always go before writes. This is natural: reads
* are much more time-critical than writes.
*/
#define IN_ORDER( s1,s2 ) \
	( ( s1 )->cmd<( s2 )->cmd || ( s1 )->cmd==( s2 )->cmd && \
	( ( s1 )->dev < ( s2 )->dev || ( ( s1 )->dev == ( s2 )->dev && \
	( s1 )->sector < ( s2 )->sector ) ) )

typedef struct blk_dev_struct
{
	VOID( *request_fn )();
	struct request *current_request;
}BLK_DEV;

extern BLK_DEV			blk_dev[ NR_BLK_DEV ];
extern Request			request[ NR_REQUEST ];
extern Task_Struct *	wait_for_request;

#ifdef MAJOR_NR

/*
* Add entries as needed. Currently the only block devices
* supported are hard-disks and floppies.
*/

#if ( MAJOR_NR == 1 )
/* ram disk */
#define DEVICE_NAME "ramdisk"
#define DEVICE_REQUEST do_rd_request
#define DEVICE_NR( device ) ( ( device ) & 7 )
#define DEVICE_ON( device ) 
#define DEVICE_OFF( device )

#elif ( MAJOR_NR == 2 )
/* floppy */
#define DEVICE_NAME "floppy"
#define DEVICE_INTR do_floppy
#define DEVICE_REQUEST do_fd_request
#define DEVICE_NR( device ) ( ( device ) & 3 )
#define DEVICE_ON( device ) floppy_on( DEVICE_NR( device ) )
#define DEVICE_OFF( device ) floppy_off( DEVICE_NR( device ) )

#elif ( MAJOR_NR == 3 )
/* harddisk */
#define DEVICE_NAME "harddisk"
#define DEVICE_INTR do_hd
#define DEVICE_REQUEST do_hd_request
#define DEVICE_NR( device ) ( MINOR( device )/5 )
#define DEVICE_ON( device )
#define DEVICE_OFF( device )

#elif
/* unknown blk device */
#error "unknown blk device"

#endif

#define CURRENT ( blk_dev[ MAJOR_NR ].current_request )
#define CURRENT_DEV DEVICE_NR( CURRENT->dev )

#ifdef DEVICE_INTR
VOID( *DEVICE_INTR )() = NULL;
#endif
static VOID ( DEVICE_REQUEST )();

//extern __inline VOID unlock_buffer( Buffer_Head * bh )
#define unlock_buffer( bh )										\
{																\
	if ( !( ( Buffer_Head * )( bh ) )->b_lock )					\
		printk( DEVICE_NAME ": free buffer being unlocked\n" );	\
	( ( Buffer_Head * )( bh ) )->b_lock = 0;					\
	wake_up( &( ( Buffer_Head * )( bh ) )->b_wait );			\
}

//extern __inline VOID end_request( LONG uptodate )
#define end_request( uptodate )								\
{															\
	DEVICE_OFF( CURRENT->dev );								\
	if ( CURRENT->bh ) {									\
		CURRENT->bh->b_uptodate = ( uptodate );				\
		unlock_buffer( CURRENT->bh );						\
	}														\
	if ( !( uptodate ) ) {									\
		printk( DEVICE_NAME " I/O error\n\r" );				\
		printk( "dev %04x, block %d\n\r", CURRENT->dev,		\
			CURRENT->bh->b_blocknr );						\
	}														\
	wake_up( &CURRENT->waiting );							\
	wake_up( &wait_for_request );							\
	CURRENT->dev = -1;										\
	CURRENT = CURRENT->next;								\
}

#define INIT_REQUEST \
repeat: \
	if ( !CURRENT ) \
	return; \
	if ( MAJOR( CURRENT->dev ) != MAJOR_NR ) \
	panic( DEVICE_NAME ": request list destroyed" ); \
	if ( CURRENT->bh ) { \
	if ( !CURRENT->bh->b_lock ) \
	panic( DEVICE_NAME ": block not locked" ); \
	}

#endif

#endif
