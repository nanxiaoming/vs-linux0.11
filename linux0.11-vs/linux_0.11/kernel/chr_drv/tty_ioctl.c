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

// ��������������( ���Ϊ�������� ).�������벨�������ӵĶ�Ӧ��ϵ�μ��б���˵��
static USHORT quotient[] = {
	0, 2304, 1536, 1047, 857,
	768, 576, 384, 192, 96,
	64, 48, 24, 12, 6, 3
};

// �޸Ĵ�������.
// ����:tty - �ն˶�Ӧ��tty ���ݽṹ.
// �ڳ��������־DLAB( ��·���ƼĴ���λ7 )��λ�����,ͨ���˿�0x3f8 ��0x3f9 ��UART �ֱ�д��
// ���������ӵ��ֽں͸��ֽ�

static VOID change_speed( struct tty_struct * tty )
{
	USHORT port, quot;

	// ���ڴ����ն�,��tty �ṹ�Ķ��������data �ֶδ�ŵ��Ǵ��ж˿ں�( 0x3f8 ��0x2f8 ).
	if ( !( port = ( USHORT )tty->read_q.data ) )
		return;

	// ��tty ��termios �ṹ����ģʽ��־����ȡ�����õĲ�����������,�ݴ˴Ӳ���������������ȡ��
	// ��Ӧ�Ĳ���������ֵ.CBAUD �ǿ���ģʽ��־���в�����λ������
	quot = quotient[ tty->termios.c_cflag & CBAUD ];
	cli();
	outb_p( 0x80, port + 3 );			/* set DLAB			*/	// �������ó���������־DLAB.
	outb_p( quot & 0xff, port );		/* LS of divisor	*/	// ������ӵ��ֽ�.
	outb_p( quot >> 8, port + 1 );	/* MS of divisor	*/	// ������Ӹ��ֽ�.
	outb( 0x03, port + 3 );			/* reset DLAB		*/	// ��λDLAB.
	sti();
}

// ˢ��tty �������.
// ����:gueue - ָ���Ļ������ָ��.
// �����е�ͷָ�����βָ��,�Ӷ��ﵽ��ջ�����( ���ַ� )��Ŀ��
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

// ȡ�ն�termios �ṹ��Ϣ.
// ����:tty - ָ���ն˵�tty �ṹָ��;termios - �û�������termios �ṹ������ָ��.
static LONG get_termios( struct tty_struct * tty, struct termios * termios )
{
	LONG i;

	// ������֤һ���û��Ļ�����ָ����ָ�ڴ����Ƿ��㹻,�粻��������ڴ�

	verify_area( termios, sizeof ( *termios ) );

	// ����ָ��tty �ṹ�е�termios �ṹ��Ϣ���û� termios �ṹ������
	for ( i = 0; i < ( sizeof ( *termios ) ); i++ )
		put_fs_byte( ( ( CHAR * )&tty->termios )[ i ], i + ( CHAR * )termios );
	return 0;
}

// �����ն�termios �ṹ��Ϣ.
// ����:tty - ָ���ն˵�tty �ṹָ��;termios - �û�������termios �ṹָ��
static LONG set_termios( struct tty_struct * tty, struct termios * termios )
{
	LONG i;

	// ���ȸ����û���������termios �ṹ��Ϣ��ָ��tty �ṹ��
	for ( i = 0; i < ( sizeof ( *termios ) ); i++ )
		( ( CHAR * )&tty->termios )[ i ] = get_fs_byte( i + ( CHAR * )termios );
	// �û��п������޸���tty �Ĵ��пڴ��䲨����,���Ը���termios �ṹ�еĿ���ģʽ��־c_cflag
	// �޸Ĵ���оƬUART �Ĵ��䲨����
	change_speed( tty );
	return 0;
}

// ��ȡtermio �ṹ�е���Ϣ.
// ����:tty - ָ���ն˵�tty �ṹָ��;termio - �û�������termio �ṹ������ָ��.

static LONG get_termio( struct tty_struct * tty, struct termio * termio )
{
	LONG i;
	struct termio tmp_termio;

	// ������֤һ���û��Ļ�����ָ����ָ�ڴ����Ƿ��㹻,�粻��������ڴ�
	verify_area( termio, sizeof ( *termio ) );
	// ��termios �ṹ����Ϣ���Ƶ�termio �ṹ��.Ŀ����Ϊ������ģʽ��־�������ͽ���ת��,Ҳ��
	// ��termios �ĳ���������ת��Ϊtermio �Ķ���������
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
 * �����termio ���ú�������386 ���ֽ���ǰ�ķ�ʽ�¿���.
 */
 // �����ն�termio �ṹ��Ϣ.
 // ����:tty - ָ���ն˵�tty �ṹָ��;termio - �û�������termio �ṹָ��.
 // ���û�������termio ����Ϣ���Ƶ��ն˵�termios �ṹ��.����0 
static LONG set_termio( struct tty_struct * tty, struct termio * termio )
{
	LONG i;
	struct termio tmp_termio;

	// ���ȸ����û���������termio �ṹ��Ϣ����ʱtermio �ṹ��
	for ( i = 0; i < ( sizeof ( *termio ) ); i++ )
		( ( CHAR * )&tmp_termio )[ i ] = get_fs_byte( i + ( CHAR * )termio );
	// �ٽ�termio �ṹ����Ϣ���Ƶ�tty ��termios �ṹ��.Ŀ����Ϊ������ģʽ��־�������ͽ���ת��,
	// Ҳ����termio �Ķ���������ת����termios �ĳ���������
	*( USHORT * )&tty->termios.c_iflag = tmp_termio.c_iflag;
	*( USHORT * )&tty->termios.c_oflag = tmp_termio.c_oflag;
	*( USHORT * )&tty->termios.c_cflag = tmp_termio.c_cflag;
	*( USHORT * )&tty->termios.c_lflag = tmp_termio.c_lflag;
	// ���ֽṹ��c_line ��c_cc[]�ֶ�����ȫ��ͬ��
	tty->termios.c_line = tmp_termio.c_line;
	for ( i = 0; i < NCC; i++ )
		tty->termios.c_cc[ i ] = tmp_termio.c_cc[ i ];
	// �û��������޸���tty �Ĵ��пڴ��䲨����,���Ը���termios �ṹ�еĿ���ģʽ��־��c_cflag
	// �޸Ĵ���оƬUART �Ĵ��䲨����
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
