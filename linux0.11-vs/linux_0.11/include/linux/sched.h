#ifndef _SCHED_H
#define _SCHED_H

#define NR_TASKS 64
#define HZ 100

#define FIRST_TASK task[ 0 ]
#define LAST_TASK  task[ NR_TASKS-1 ]

#include <linux/head.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <signal.h>

#if ( NR_OPEN > 32 )
#error "Currently the close-on-exec-flags are in one word, max 32 files/proc"
#endif

#define TASK_RUNNING			0
#define TASK_INTERRUPTIBLE		1
#define TASK_UNINTERRUPTIBLE	2
#define TASK_ZOMBIE				3
#define TASK_STOPPED			4

#ifndef NULL
#define NULL ( ( VOID * ) 0 )
#endif

extern LONG copy_page_tables( ULONG from, ULONG to, LONG size );
extern LONG free_page_tables( ULONG from, ULONG size );

extern VOID sched_init();
extern VOID schedule();
extern VOID trap_init();
extern volatile VOID panic( const CHAR * str );
extern LONG tty_write( unsigned minor, CHAR * buf, LONG count );

typedef LONG( *fn_ptr )();

typedef struct i387_struct {
	LONG	cwd;
	LONG	swd;
	LONG	twd;
	LONG	fip;
	LONG	fcs;
	LONG	foo;
	LONG	fos;
	LONG	st_space[ 20 ];	/* 8*10 bytes for each FP-reg = 80 bytes */
}I387_Struct ;

typedef struct tss_struct {
	LONG	back_link;		/* 16 high bits zero */
	LONG	esp0;
	LONG	ss0;			/* 16 high bits zero */
	LONG	esp1;
	LONG	ss1;			/* 16 high bits zero */
	LONG	esp2;
	LONG	ss2;			/* 16 high bits zero */
	LONG	cr3;
	LONG	eip;
	LONG	eflags;
	LONG	eax, ecx, edx, ebx;
	LONG	esp;
	LONG	ebp;
	LONG	esi;
	LONG	edi;
	LONG	es;				/* 16 high bits zero */
	LONG	cs;				/* 16 high bits zero */
	LONG	ss;				/* 16 high bits zero */
	LONG	ds;				/* 16 high bits zero */
	LONG	fs;				/* 16 high bits zero */
	LONG	gs;				/* 16 high bits zero */
	LONG	ldt;			/* 16 high bits zero */
	LONG	trace_bitmap;	/* bits: trace 0, bitmap 16-31 */
	I387_Struct i387;
}Tss_Struct;

typedef struct task_struct 
{
	/* these are hardcoded - don't touch */
	LONG				state;	/* -1 unrunnable, 0 runnable, >0 stopped */	// Offset=0x0   Size=0x4
	LONG				counter;											// Offset=0x4   Size=0x4
	LONG				priority;											// Offset=0x8   Size=0x4
	LONG				signal;												// Offset=0xc   Size=0x4
	struct sigaction	sigaction[ 32 ];									// Offset=0x10  Size=0x200
	LONG				blocked;	/* bitmap of masked signals */			// Offset=0x210 Size=0x4
	/* various fields */
	LONG				exit_code;											// Offset=0x214 Size=0x4
	ULONG				start_code; 										// Offset=0x218 Size=0x4
	ULONG				end_code;											// Offset=0x21c Size=0x4
	ULONG				end_data;											// Offset=0x220 Size=0x4
	ULONG				brk;												// Offset=0x224 Size=0x4
	ULONG				start_stack;										// Offset=0x228 Size=0x4
	LONG				pid;												// Offset=0x22c Size=0x4
	LONG				father;												// Offset=0x230 Size=0x4
	LONG				pgrp;												// Offset=0x234 Size=0x4
	LONG				session;											// Offset=0x238 Size=0x4
	LONG				leader;												// Offset=0x23c Size=0x4
	USHORT				uid;												// Offset=0x240 Size=0x2				
	USHORT				euid;												// Offset=0x242 Size=0x2
	USHORT				suid;												// Offset=0x244 Size=0x2
	USHORT				gid;												// Offset=0x246 Size=0x2
	USHORT				egid;												// Offset=0x248 Size=0x2
	USHORT				sgid;												// Offset=0x24a Size=0x2
	LONG				alarm;												// Offset=0x24c Size=0x4
	LONG				utime;												// Offset=0x250 Size=0x4
	LONG				stime;												// Offset=0x254 Size=0x4
	LONG				cutime;												// Offset=0x258 Size=0x4
	LONG				cstime;												// Offset=0x25c Size=0x4
	LONG				start_time;											// Offset=0x260 Size=0x4
	USHORT				used_math;											// Offset=0x264 Size=0x2
	/* file system info */
	LONG				tty;	/* -1 if no tty, so it must be signed */	// Offset=0x268 Size=0x4
	USHORT				umask;												// Offset=0x26c Size=0x2
	M_Inode *			pwd;												// Offset=0x270 Size=0x4
	M_Inode *			root;												// Offset=0x274 Size=0x4
	M_Inode *			executable;											// Offset=0x278 Size=0x4
	ULONG				close_on_exec;										// Offset=0x27c Size=0x4
	File	*			filp[ NR_OPEN ];									// Offset=0x280 Size=0x50
	/* ldt for this task 0 - zero 1 - cs 2 - ds&ss */						
	Desc_Struct			ldt[ 3 ];											// Offset=0x2d0 Size=0x18
	/* tss for this task */
	Tss_Struct			tss;												// Offset=0x2e8 Size=0xd4
}Task_Struct;

/*
 *  INIT_TASK is used to set up the first task table, touch at
 * your own risk!. Base=0, limit=0x9ffff ( =640kB )
 */
#define INIT_TASK \
	/* */			{ \
	/* state etc */		0, 15, 15, \
	/* signals */		0, { { 0, }, }, 0, \
	/* ec,brk... */		0, 0, 0, 0, 0, 0, \
	/* pid etc.. */		0, -1, 0, 0, 0, \
	/* uid etc */		0, 0, 0, 0, 0, 0, \
	/* alarm */			0, 0, 0, 0, 0, 0, \
	/* math */			0, \
	/* fs info */		-1, 0022, NULL, NULL, NULL, 0, \
	/* filp */			{ NULL, }, \
	/* */				{ \
	/* */					{ 0, 0 }, \
	/* ldt */				{ 0x9f, 0xc0fa00 }, \
	/* */					{ 0x9f, 0xc0f200 }, \
	/* */				}, \
	/* */				{ \
	/* tss */				0, PAGE_SIZE + ( LONG )&init_task, 0x10, 0, 0, 0, 0, ( LONG )&pg_dir, \
	/* */					0, 0, 0, 0, 0, 0, 0, 0, \
	/* */					0, 0, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, \
	/* */					_LDT( 0 ), 0x80000000, \
	/* */					{ 0, } \
	/* */				} \
	/* */			} \

extern Task_Struct *task[ NR_TASKS ];
extern Task_Struct *last_task_used_math;
extern Task_Struct *current;
extern LONG volatile jiffies;
extern LONG startup_time;

#define CURRENT_TIME ( startup_time+jiffies/HZ )

extern VOID add_timer( LONG jiffies, VOID( *fn )() );
extern VOID sleep_on( Task_Struct ** p );
extern VOID interruptible_sleep_on( Task_Struct ** p );
extern VOID wake_up( Task_Struct ** p );

/*
* Entry into gdt where to find first TSS. 0-nul, 1-cs, 2-ds, 3-syscall
* 4-TSS0, 5-LDT0, 6-TSS1 etc ...
*/
#define FIRST_TSS_ENTRY 4
#define FIRST_LDT_ENTRY ( FIRST_TSS_ENTRY+1 )
#define _TSS( n ) ( ( ( ( ULONG ) n )<<4 )+( FIRST_TSS_ENTRY<<3 ) )
#define _LDT( n ) ( ( ( ( ULONG ) n )<<4 )+( FIRST_LDT_ENTRY<<3 ) )

static __inline VOID ltr( short n )
{
	short a = _TSS( n );

	__asm mov	ax, a
	__asm ltr	ax
}

static __inline VOID lldt( short n )
{
	short a = _LDT( n );

	__asm mov	ax, a
	__asm lldt	ax
}

#define str( n ) \
	__asm xor	eax, eax \
	__asm str	ax \
	__asm sub	eax, FIRST_TSS_ENTRY << 3 \
	__asm shr	eax, 4 \
	__asm mov	n, eax

/*
*	switch_to( n ) should switch tasks to task nr n, first
* checking that n isn't the current task, in which case it does nothing.
* This also clears the TS-flag if the task we switched to has used
* tha math co-processor latest.
*/
static __inline VOID switch_to( LONG n )
{
	struct { LONG a, b; } __tmp;

	if ( task[ n ] == current )
	{
		return;
	}

	__tmp.b = _TSS( n );

	current = task[ n ];

	__asm jmp FWORD PTR __tmp

	if ( task[ n ] == last_task_used_math )
	{
		__asm clts
	}
}

static __inline _set_base( CHAR *addr, ULONG base )
{
	*( ( short* )( addr + 2 ) ) = base & 0xFFFF;
	*( addr + 4 ) = ( base >> 16 ) & 0xFF;
	*( addr + 7 ) = ( CHAR )( base >> 24 );
}

static __inline _set_limit( CHAR *addr, ULONG limit )
{
	*( short* )addr = limit & 0xFFFF;

	*( addr + 6 )   = ( *( addr + 6 ) & 0xF0 ) | ( ( limit >> 16 ) & 0xFF );
}

#define set_base( ldt,base ) _set_base( ( ( CHAR * )&( ldt ) ) , base )
#define set_limit( ldt,limit ) _set_limit( ( ( CHAR * )&( ldt ) ) , ( limit-1 )>>12 )

static __inline ULONG _get_base( UCHAR *addr )
{
	ULONG __base;

	__base = ( *( addr + 7 ) << 24 ) + ( *( addr + 4 ) << 16 ) + *( USHORT* )( addr + 2 );

	return __base;
}

#define get_base( ldt ) _get_base( ( ( CHAR * )&( ldt ) ) )

static __inline ULONG get_limit( LONG segment )
{
	ULONG limit;

	__asm lsl	eax	, segment
	__asm inc	eax
	__asm mov	limit, eax

	return limit;
}

#endif
