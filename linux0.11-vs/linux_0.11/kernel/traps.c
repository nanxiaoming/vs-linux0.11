/*
*  linux/kernel/traps.c
*
*  ( C ) 1991  Linus Torvalds
*/

/*
* 'Traps.c' handles hardware traps and faults after we have saved some
* state in 'asm.s'. Currently mostly a debugging-aid, will be extended
* to mainly kill the offending process ( probably by giving it a signal,
* but possibly by killing it outright if necessary ).
*/
#include <string.h>

#include <linux\head.h>
#include <linux\sched.h>
#include <linux\kernel.h>
#include <asm\system.h>
#include <asm\segment.h>
#include <asm\io.h>

static __inline CHAR get_seg_byte( USHORT _seg, CHAR *addr )
{
	register CHAR __res;

	__asm mov	ax, _seg
	__asm push	fs
	__asm mov	fs, ax
	__asm mov	eax, addr
	__asm mov	al, fs :[ eax ];
	__asm pop	fs
	__asm mov	__res, al

	return __res;
}

static __inline ULONG get_seg_long( USHORT _seg, LONG *addr )
{
	register ULONG __res;

	__asm mov	ax, _seg
	__asm push	fs
	__asm mov	fs, ax
	__asm mov	eax, addr
	__asm mov	eax, fs:[ eax ];
	__asm pop	fs
	__asm mov	__res, eax

	return __res;
}

static __inline USHORT _fs()
{
	register USHORT __res;

	__asm mov ax	, fs
	__asm mov __res	, ax

	return __res;
}

LONG do_exit( LONG code );

VOID page_exception();

VOID divide_error();
VOID debug();
VOID nmi();
VOID int3();
VOID overflow();
VOID bounds();
VOID invalid_op();
VOID device_not_available();
VOID double_fault();
VOID coprocessor_segment_overrun();
VOID invalid_TSS();
VOID segment_not_present();
VOID stack_segment();
VOID general_protection();
VOID page_fault();
VOID coprocessor_error();
VOID reserved();
VOID parallel_interrupt();
VOID irq13();

static VOID die( CHAR * str, LONG esp_ptr, LONG nr )
{
	LONG *	esp = ( LONG * )esp_ptr;
	LONG	i;

	printk( "%s: %04x\n\r", str, nr & 0xffff );
	printk( "EIP:\t%04x:%p\nEFLAGS:\t%p\nESP:\t%04x:%p\n",
			esp[ 1 ], esp[ 0 ], esp[ 2 ], esp[ 4 ], esp[ 3 ] );

	printk( "fs: %04x\n", _fs() );

	printk( "base: %p, limit: %p\n", get_base( current->ldt[ 1 ] ), get_limit( 0x17 ) );

	if ( esp[ 4 ] == 0x17 ) 
	{
		printk( "Stack: " );

		for ( i = 0; i < 4; i++ )
		{
			printk( "%p ", get_seg_long( 0x17, i + ( LONG * )esp[ 3 ] ) );
		}

		printk( "\n" );
	}

	str( i );

	printk( "Pid: %d, process nr: %d\n\r", current->pid, 0xffff & i );

	for ( i = 0; i < 10; i++ )
	{
		printk( "%02x ", 0xff & get_seg_byte( ( USHORT )esp[ 1 ], ( i + ( CHAR * )esp[ 0 ] ) ) );
	}

	printk( "\n\r" );

	do_exit( 11 );		/* play segment exception */
}

VOID do_double_fault( LONG esp, LONG error_code )
{
	die( "double fault", esp, error_code );
}

VOID do_general_protection( LONG esp, LONG error_code )
{
	die( "general protection", esp, error_code );
}

VOID do_divide_error( LONG esp, LONG error_code )
{
	die( "divide error", esp, error_code );
}

VOID do_int3( 
	LONG *	esp	, 
	LONG	error_code,
	LONG	fs	, 
	LONG	es	, 
	LONG	ds	,
	LONG	ebp , 
	LONG	esi , 
	LONG	edi ,
	LONG	edx , 
	LONG	ecx , 
	LONG	ebx , 
	LONG	eax )
{
	LONG tr;

	__asm xor	eax, eax
	__asm str	ax
	__asm mov	tr, eax

	printk( "eax\t\tebx\t\tecx\t\tedx\n\r%8x\t%8x\t%8x\t%8x\n\r",
			eax, ebx, ecx, edx );
	
	printk( "esi\t\tedi\t\tebp\t\tesp\n\r%8x\t%8x\t%8x\t%8x\n\r",
			esi, edi, ebp, ( LONG )esp );

	printk( "\n\rds\tes\tfs\ttr\n\r%4x\t%4x\t%4x\t%4x\n\r",
			ds, es, fs, tr );

	printk( "EIP: %8x   CS: %4x  EFLAGS: %8x\n\r", esp[ 0 ], esp[ 1 ], esp[ 2 ] );
}

VOID do_nmi( LONG esp, LONG error_code )
{
	die( "nmi", esp, error_code );
}

VOID do_debug( LONG esp, LONG error_code )
{
	die( "debug", esp, error_code );
}

VOID do_overflow( LONG esp, LONG error_code )
{
	die( "overflow", esp, error_code );
}

VOID do_bounds( LONG esp, LONG error_code )
{
	die( "bounds", esp, error_code );
}

VOID do_invalid_op( LONG esp, LONG error_code )
{
	die( "invalid operand", esp, error_code );
}

VOID do_device_not_available( LONG esp, LONG error_code )
{
	die( "device not available", esp, error_code );
}

VOID do_coprocessor_segment_overrun( LONG esp, LONG error_code )
{
	die( "coprocessor segment overrun", esp, error_code );
}

VOID do_invalid_TSS( LONG esp, LONG error_code )
{
	die( "invalid TSS", esp, error_code );
}

VOID do_segment_not_present( LONG esp, LONG error_code )
{
	die( "segment not present", esp, error_code );
}

VOID do_stack_segment( LONG esp, LONG error_code )
{
	die( "stack segment", esp, error_code );
}

VOID do_coprocessor_error( LONG esp, LONG error_code )
{
	if ( last_task_used_math != current )
		return;
	die( "coprocessor error", esp, error_code );
}

VOID do_reserved( LONG esp, LONG error_code )
{
	die( "reserved ( 15,17-47 ) error", esp, error_code );
}

VOID trap_init()
{
	LONG i;

	set_trap_gate	( 0  , &divide_error				);
	set_trap_gate	( 1  , &debug						);
	set_trap_gate	( 2  , &nmi							);
	set_system_gate	( 3  , &int3						);	/* int3-5 can be called from all */
	set_system_gate	( 4  , &overflow					);
	set_system_gate	( 5  , &bounds						);
	set_trap_gate	( 6  , &invalid_op					);
	set_trap_gate	( 7  , &device_not_available		);
	set_trap_gate	( 8  , &double_fault				);
	set_trap_gate	( 9  , &coprocessor_segment_overrun );
	set_trap_gate	( 10 , &invalid_TSS					);
	set_trap_gate	( 11 , &segment_not_present			);
	set_trap_gate	( 12 , &stack_segment				);
	set_trap_gate	( 13 , &general_protection			);
	set_trap_gate	( 14 , &page_fault					);
	set_trap_gate	( 15 , &reserved					);
	set_trap_gate	( 16 , &coprocessor_error			);

	for ( i = 17; i < 48; i++ )
		set_trap_gate( i, &reserved );

	set_trap_gate( 45, &irq13 );

	outb_p	( inb_p( 0x21 ) & 0xfb, 0x21 );
	outb	( inb_p( 0xA1 ) & 0xdf, 0xA1 );

	set_trap_gate( 39, &parallel_interrupt );
}
