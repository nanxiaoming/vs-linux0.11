/*
* 'tty.h' defines some structures used by tty_io.c and some defines.
*
* NOTE! Don't touch this without checking that nothing in rs_io.s or
* con_io.s breaks. Some constants are hardwired into the system ( mainly
* offsets into 'tty_queue'
*/

/*
 * tty������ɶ��˼? 
 * 
 * tty �� "Teletypewriter"����д����Դ�����ڼ�������ն��豸����ʷ��
 * ����������Դ�ڻ�е���ֻ���tty ����ָ�����ı��ն˽ӿڣ�
 * ���������ն��豸�Ľ��������ִ�Linuxϵͳ�У�
 * tty ͨ��ָ�����նˡ�α�նˡ��������ն��豸��
 * 
 */

#ifndef _TTY_H
#define _TTY_H

#include <termios.h>

#define TTY_BUF_SIZE 1024

struct tty_queue {
	ULONG data;
	ULONG head;
	ULONG tail;
	struct task_struct * proc_list;
	CHAR buf[ TTY_BUF_SIZE ];
};

#define INC( a )	( ( a ) = ( ( a )+1 ) & ( TTY_BUF_SIZE-1 ) )
#define DEC( a )	( ( a ) = ( ( a )-1 ) & ( TTY_BUF_SIZE-1 ) )
#define EMPTY( a )	( ( a ).head == ( a ).tail )
#define LEFT( a )	( ( ( a ).tail-( a ).head-1 )&( TTY_BUF_SIZE-1 ) )
#define LAST( a )	( ( a ).buf[ ( TTY_BUF_SIZE-1 )&( ( a ).head-1 ) ] )
#define FULL( a )	( !LEFT( a ) )
#define CHARS( a )	( ( ( a ).head-( a ).tail )&( TTY_BUF_SIZE-1 ) )
#define GETCH( queue,c ) \
	 ( c=( queue ).buf[ ( queue ).tail ],INC( ( queue ).tail ) )

#define PUTCH( c,queue ) \
	 ( ( queue ).buf[ ( queue ).head ]=( c ),INC( ( queue ).head ) )

#define INTR_CHAR( tty )	( ( tty )->termios.c_cc[ VINTR ]	)
#define QUIT_CHAR( tty )	( ( tty )->termios.c_cc[ VQUIT ]	)
#define ERASE_CHAR( tty )	( ( tty )->termios.c_cc[ VERASE ]	)
#define KILL_CHAR( tty )	( ( tty )->termios.c_cc[ VKILL ]	)
#define EOF_CHAR( tty )		( ( tty )->termios.c_cc[ VEOF ]		)
#define START_CHAR( tty )	( ( tty )->termios.c_cc[ VSTART ]	)
#define STOP_CHAR( tty )	( ( tty )->termios.c_cc[ VSTOP ]	)
#define SUSPEND_CHAR( tty ) ( ( tty )->termios.c_cc[ VSUSP ]	)

typedef
struct tty_struct 
{
	struct termios termios;
	LONG pgrp;
	LONG stopped;
	VOID( *write )( struct tty_struct * tty );
	struct tty_queue read_q;
	struct tty_queue write_q;
	struct tty_queue secondary;
}TTY_Struct;

extern TTY_Struct tty_table[];

/*	
intr=^C		quit=^|		erase=del	kill=^U
eof=^D		vtime=\0	vmin=\1		sxtc=\0
start=^Q	stop=^S		susp=^Z		eol=\0
reprint=^R	discard=^U	werase=^W	lnext=^V
eol2=\0
*/
#define INIT_C_CC "\003\034\177\025\004\0\1\0\021\023\032\0\022\017\027\026\0"

VOID rs_init();
VOID con_init();
VOID tty_init();

LONG tty_read( unsigned c, CHAR * buf, LONG n );
LONG tty_write( unsigned c, CHAR * buf, LONG n );

VOID rs_write( TTY_Struct * tty );
VOID con_write( TTY_Struct * tty );

VOID copy_to_cooked( TTY_Struct * tty );

#endif
