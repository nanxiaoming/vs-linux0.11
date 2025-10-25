/*
*  linux/kernel/chr_drv/tty_ioctl.c
*
*  ( C ) 1991  Linus Torvalds
*/

#include <errno.h>
#include <termios.h>

#include <linux\sched.h>
#include <linux\kernel.h>
#include <linux\tty.h>

#include <asm\io.h>
#include <asm\segment.h>
#include <asm\system.h>

// 波特率因子数组( 或称为除数数组 ).波特率与波特率因子的对应关系参见列表后的说明
static USHORT quotient[] = {
	0, 2304, 1536, 1047, 857,
	768, 576, 384, 192, 96,
	64, 48, 24, 12, 6, 3
};

// 修改传输速率.
// 参数:tty - 终端对应的tty 数据结构.
// 在除数锁存标志DLAB( 线路控制寄存器位7 )置位情况下,通过端口0x3f8 和0x3f9 向UART 分别写入
// 波特率因子低字节和高字节

static VOID change_speed( struct tty_struct * tty )
{
	USHORT port, quot;

	// 对于串口终端,其tty 结构的读缓冲队列data 字段存放的是串行端口号( 0x3f8 或0x2f8 ).
	if ( !( port = ( USHORT )tty->read_q.data ) )
		return;

	// 从tty 的termios 结构控制模式标志集中取得设置的波特率索引号,据此从波特率因子数组中取得
	// 对应的波特率因子值.CBAUD 是控制模式标志集中波特率位屏蔽码
	quot = quotient[ tty->termios.c_cflag & CBAUD ];
	cli();
	outb_p( 0x80, port + 3 );			/* set DLAB			*/	// 首先设置除数锁定标志DLAB.
	outb_p( quot & 0xff, port );		/* LS of divisor	*/	// 输出因子低字节.
	outb_p( quot >> 8, port + 1 );	/* MS of divisor	*/	// 输出因子高字节.
	outb( 0x03, port + 3 );			/* reset DLAB		*/	// 复位DLAB.
	sti();
}

// 刷新tty 缓冲队列.
// 参数:gueue - 指定的缓冲队列指针.
// 令缓冲队列的头指针等于尾指针,从而达到清空缓冲区( 零字符 )的目的
static VOID flush( struct tty_queue * queue )
{
	cli();
	queue->head = queue->tail;
	sti();
}

static VOID wait_until_sent( struct tty_struct * tty )
{
	/* do nothing - not implemented */
}

static VOID send_break( struct tty_struct * tty )
{
	/* do nothing - not implemented */
}

// 取终端termios 结构信息.
// 参数:tty - 指定终端的tty 结构指针;termios - 用户数据区termios 结构缓冲区指针.
static LONG get_termios( struct tty_struct * tty, struct termios * termios )
{
	LONG i;

	// 首先验证一下用户的缓冲区指针所指内存区是否足够,如不够则分配内存

	verify_area( termios, sizeof ( *termios ) );

	// 复制指定tty 结构中的termios 结构信息到用户 termios 结构缓冲区
	for ( i = 0; i < ( sizeof ( *termios ) ); i++ )
		put_fs_byte( ( ( CHAR * )&tty->termios )[ i ], i + ( CHAR * )termios );
	return 0;
}

// 设置终端termios 结构信息.
// 参数:tty - 指定终端的tty 结构指针;termios - 用户数据区termios 结构指针
static LONG set_termios( struct tty_struct * tty, struct termios * termios )
{
	LONG i;

	// 首先复制用户数据区中termios 结构信息到指定tty 结构中
	for ( i = 0; i < ( sizeof ( *termios ) ); i++ )
		( ( CHAR * )&tty->termios )[ i ] = get_fs_byte( i + ( CHAR * )termios );
	// 用户有可能已修改了tty 的串行口传输波特率,所以根据termios 结构中的控制模式标志c_cflag
	// 修改串行芯片UART 的传输波特率
	change_speed( tty );
	return 0;
}

// 读取termio 结构中的信息.
// 参数:tty - 指定终端的tty 结构指针;termio - 用户数据区termio 结构缓冲区指针.

static LONG get_termio( struct tty_struct * tty, struct termio * termio )
{
	LONG i;
	struct termio tmp_termio;

	// 首先验证一下用户的缓冲区指针所指内存区是否足够,如不够则分配内存
	verify_area( termio, sizeof ( *termio ) );
	// 将termios 结构的信息复制到termio 结构中.目的是为了其中模式标志集的类型进行转换,也即
	// 从termios 的长整数类型转换为termio 的短整数类型
	tmp_termio.c_iflag = ( USHORT )tty->termios.c_iflag;
	tmp_termio.c_oflag = ( USHORT )tty->termios.c_oflag;
	tmp_termio.c_cflag = ( USHORT )tty->termios.c_cflag;
	tmp_termio.c_lflag = ( USHORT )tty->termios.c_lflag;
	tmp_termio.c_line = tty->termios.c_line;
	for ( i = 0; i < NCC; i++ )
		tmp_termio.c_cc[ i ] = tty->termios.c_cc[ i ];
	for ( i = 0; i < ( sizeof ( *termio ) ); i++ )
		put_fs_byte( ( ( CHAR * )&tmp_termio )[ i ], i + ( CHAR * )termio );
	return 0;
}

/*
 * This only works as the 386 is low-byt-first
 */
/*
 * 下面的termio 设置函数仅在386 低字节在前的方式下可用.
 */
 // 设置终端termio 结构信息.
 // 参数:tty - 指定终端的tty 结构指针;termio - 用户数据区termio 结构指针.
 // 将用户缓冲区termio 的信息复制到终端的termios 结构中.返回0 
static LONG set_termio( struct tty_struct * tty, struct termio * termio )
{
	LONG i;
	struct termio tmp_termio;

	// 首先复制用户数据区中termio 结构信息到临时termio 结构中
	for ( i = 0; i < ( sizeof ( *termio ) ); i++ )
		( ( CHAR * )&tmp_termio )[ i ] = get_fs_byte( i + ( CHAR * )termio );
	// 再将termio 结构的信息复制到tty 的termios 结构中.目的是为了其中模式标志集的类型进行转换,
	// 也即从termio 的短整数类型转换成termios 的长整数类型
	*( USHORT * )&tty->termios.c_iflag = tmp_termio.c_iflag;
	*( USHORT * )&tty->termios.c_oflag = tmp_termio.c_oflag;
	*( USHORT * )&tty->termios.c_cflag = tmp_termio.c_cflag;
	*( USHORT * )&tty->termios.c_lflag = tmp_termio.c_lflag;
	// 两种结构的c_line 和c_cc[]字段是完全相同的
	tty->termios.c_line = tmp_termio.c_line;
	for ( i = 0; i < NCC; i++ )
		tty->termios.c_cc[ i ] = tmp_termio.c_cc[ i ];
	// 用户可能已修改了tty 的串行口传输波特率,所以根据termios 结构中的控制模式标志集c_cflag
	// 修改串行芯片UART 的传输波特率
	change_speed( tty );
	return 0;
}

LONG tty_ioctl( LONG dev, LONG cmd, LONG arg )
{
	struct tty_struct * tty;
	if ( MAJOR( dev ) == 5 ) {
		dev = current->tty;
		if ( dev < 0 )
			panic( "tty_ioctl: dev<0" );
	}
	else
		dev = MINOR( dev );
	tty = dev + tty_table;
	switch ( cmd ) {
	case TCGETS:
		return get_termios( tty, ( struct termios * ) arg );
	case TCSETSF:
		flush( &tty->read_q ); /* fallthrough */
	case TCSETSW:
		wait_until_sent( tty ); /* fallthrough */
	case TCSETS:
		return set_termios( tty, ( struct termios * ) arg );
	case TCGETA:
		return get_termio( tty, ( struct termio * ) arg );
	case TCSETAF:
		flush( &tty->read_q ); /* fallthrough */
	case TCSETAW:
		wait_until_sent( tty ); /* fallthrough */
	case TCSETA:
		return set_termio( tty, ( struct termio * ) arg );
	case TCSBRK:
		if ( !arg ) {
			wait_until_sent( tty );
			send_break( tty );
		}
		return 0;
	case TCXONC:
		return -EINVAL; /* not implemented */
	case TCFLSH:
		if ( arg == 0 )
			flush( &tty->read_q );
		else if ( arg == 1 )
			flush( &tty->write_q );
		else if ( arg == 2 ) {
			flush( &tty->read_q );
			flush( &tty->write_q );
		}
		else
			return -EINVAL;
		return 0;
	case TIOCEXCL:
		return -EINVAL; /* not implemented */
	case TIOCNXCL:
		return -EINVAL; /* not implemented */
	case TIOCSCTTY:
		return -EINVAL; /* set controlling term NI */
	case TIOCGPGRP:
		verify_area( ( VOID * )arg, 4 );
		put_fs_long( tty->pgrp, ( ULONG * )arg );
		return 0;
	case TIOCSPGRP:
		tty->pgrp = get_fs_long( ( ULONG * )arg );
		return 0;
	case TIOCOUTQ:
		verify_area( ( VOID * )arg, 4 );
		put_fs_long( CHARS( tty->write_q ), ( ULONG * )arg );
		return 0;
	case TIOCINQ:
		verify_area( ( VOID * )arg, 4 );
		put_fs_long( CHARS( tty->secondary ),
			( ULONG * )arg );
		return 0;
	case TIOCSTI:
		return -EINVAL; /* not implemented */
	case TIOCGWINSZ:
		return -EINVAL; /* not implemented */
	case TIOCSWINSZ:
		return -EINVAL; /* not implemented */
	case TIOCMGET:
		return -EINVAL; /* not implemented */
	case TIOCMBIS:
		return -EINVAL; /* not implemented */
	case TIOCMBIC:
		return -EINVAL; /* not implemented */
	case TIOCMSET:
		return -EINVAL; /* not implemented */
	case TIOCGSOFTCAR:
		return -EINVAL; /* not implemented */
	case TIOCSSOFTCAR:
		return -EINVAL; /* not implemented */
	default:
		return -EINVAL;
	}
}
