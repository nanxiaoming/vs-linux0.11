/*
*  linux/kernel/tty_io.c
*
*  ( C ) 1991  Linus Torvalds
*/

/*
 * 'tty_io.c' gives an orthogonal feeling to tty's, be they consoles
 * or rs-channels. It also implements echoing, cooked mode etc.
 *
 * Kill-line thanks to John T Kohl.
 * 
 * 'tty_io.c'��tty һ�ַ���صĸо�,�ǿ���̨���Ǵ���ͨ��.�ó���ͬ��
 * ʵ���˻��ԡ��淶( �� )ģʽ��.
 * 
 */

#include <ctype.h>
#include <errno.h>
#include <signal.h>

// ���������Ӧ�ź����ź�λͼ�еĶ�Ӧ����λ
#define ALRMMASK ( 1 << ( SIGALRM-1 ) )	// ����( alarm )�ź�����λ.
#define KILLMASK ( 1 << ( SIGKILL-1 ) )	// ��ֹ( kill )�ź�����λ.
#define INTMASK  ( 1 << ( SIGINT -1 ) )	// �����ж�( LONG )�ź�����λ.
#define QUITMASK ( 1 << ( SIGQUIT-1 ) )	// �����˳�( quit )�ź�����λ.
#define TSTPMASK ( 1 << ( SIGTSTP-1 ) )	// tty ������ֹͣ����( tty stop )�ź�����λ.

#include <linux\sched.h>
#include <linux\tty.h>
#include <asm\segment.h>
#include <asm\system.h>

#define _L_FLAG( tty,f )	( ( tty )->termios.c_lflag & f )	// ȡ termios �ṹ�еı���ģʽ��־
#define _I_FLAG( tty,f )	( ( tty )->termios.c_iflag & f )	// ȡ termios �ṹ�е�����ģʽ��־
#define _O_FLAG( tty,f )	( ( tty )->termios.c_oflag & f )	// ȡ termios �ṹ�е����ģʽ��־


// ȡ termios �ṹ�б���ģʽ��־���е�һ����־λ
#define L_CANON( tty )		_L_FLAG( ( tty ),ICANON )			// ȡ����ģʽ��־���й淶( �� )ģʽ��־λ.
#define L_ISIG( tty )		_L_FLAG( ( tty ),ISIG	)			// ȡ�źű�־λ.
#define L_ECHO( tty )		_L_FLAG( ( tty ),ECHO	)			// ȡ�����ַ���־λ.
#define L_ECHOE( tty )		_L_FLAG( ( tty ),ECHOE	)			// �淶ģʽʱ,ȡ���Բ�����־λ.
#define L_ECHOK( tty )		_L_FLAG( ( tty ),ECHOK	)			// �淶ģʽʱ,ȡKILL ������ǰ�б�־λ.
#define L_ECHOCTL( tty )	_L_FLAG( ( tty ),ECHOCTL)			// ȡ���Կ����ַ���־λ.
#define L_ECHOKE( tty )		_L_FLAG( ( tty ),ECHOKE )			// �淶ģʽʱ,ȡKILL �����в����Ա�־λ.

// ȡ termios �ṹ������ģʽ��־�е�һ����־λ
#define I_UCLC( tty )		_I_FLAG( ( tty ),IUCLC )			// ȡ����ģʽ��־���д�д��Сдת����־λ.
#define I_NLCR( tty )		_I_FLAG( ( tty ),INLCR )			// ȡ���з� NL ת�س��� CR ��־λ.
#define I_CRNL( tty )		_I_FLAG( ( tty ),ICRNL )			// ȡ�س��� CR ת���з� NL ��־λ.
#define I_NOCR( tty )		_I_FLAG( ( tty ),IGNCR )			// ȡ���Իس��� CR ��־λ.

// ȡ termios �ṹ�����ģʽ��־�е�һ����־λ
#define O_POST( tty )		_O_FLAG( ( tty ),OPOST )			// ȡ���ģʽ��־����ִ����������־.
#define O_NLCR( tty )		_O_FLAG( ( tty ),ONLCR )			// ȡ���з� NL ת�س����з� CR-NL ��־.
#define O_CRNL( tty )		_O_FLAG( ( tty ),OCRNL )			// ȡ�س��� CR ת���з�NL ��־.
#define O_NLRET( tty )		_O_FLAG( ( tty ),ONLRET)			// ȡ���з�NL ִ�лس����ܵı�־.
#define O_LCUC( tty )		_O_FLAG( ( tty ),OLCUC )			// ȡСдת��д�ַ���־.

// tty ���ݽṹ��tty_table ����
// ���а���������ʼ��������,�ֱ��
// Ӧ����̨-�����ն�1-�����ն�2
// �ĳ�ʼ������
TTY_Struct tty_table[] = {
	{
		{
			ICRNL,										// ������� CR ת��Ϊ NL
			OPOST | ONLCR,								// ������� NL ת��Ϊ CRNL
			0,											// ����ģʽ��־��ʼ��Ϊ0
			ISIG | ICANON | ECHO | ECHOCTL | ECHOKE,	// ����ģʽ��־
			0,											// ����̨termio
			INIT_C_CC									// �����ַ�����
		},
		0,												// ������ʼ������
		0,												// ��ʼֹͣ��־
		con_write,										// tty д����ָ��
		{ 0, 0, 0, 0, "" },								// tty ����̨������.
		{ 0, 0, 0, 0, "" },								// tty ����̨д����.
		{ 0, 0, 0, 0, "" }								// tty ����̨����( �ڶ� )����
	},
	{
		{
			0,											// NULL
			0,											// NULL
			B2400 | CS8,								// ����ģʽ��־.������2400bps,8 λ����λ
			0,											// ����ģʽ��־0 
			0,											// �й��0 
			INIT_C_CC									// �����ַ�����
		},	
		0,												// ������ʼ������.
		0,												// ��ʼֹͣ��־.
		rs_write,										// ����1 tty д����ָ��
		{ 0x3f8, 0, 0, 0, "" },							// �����ն�1 ���������
		{ 0x3f8, 0, 0, 0, "" },							// �����ն�1 д�������
		{ 0    , 0, 0, 0, "" }							// �����ն�1 �����������
	},
	{
		{
			0,											// ����ģʽ��־.0,����ת��
			0,											// ���ģʽ��־.0,����ת��
			B2400 | CS8,								// ����ģʽ��־.������2400bps,8 λ����λ
			0,											// ����ģʽ��־0
			0,											// �й��0.
			INIT_C_CC									// �����ַ�����
		},
		0,												// ������ʼ������.
		0,												// ��ʼֹͣ��־.
		rs_write,										// ����2 tty д����ָ��
		{ 0x2f8, 0, 0, 0, "" },							// �����ն� 2 ���������.
		{ 0x2f8, 0, 0, 0, "" },							// �����ն� 2 д�������.
		{ 0    , 0, 0, 0, "" }							// �����ն� 2 �����������.
	}
};

/*
* these are the tables used by the machine code handlers.
* you can implement pseudo-tty's or something by changing
* them. Currently not done.
*/
struct tty_queue * table_list[] = 
{
	&tty_table[ 0 ].read_q, &tty_table[ 0 ].write_q,
	&tty_table[ 1 ].read_q, &tty_table[ 1 ].write_q,
	&tty_table[ 2 ].read_q, &tty_table[ 2 ].write_q
};

VOID tty_init()
{
	rs_init();
	con_init();
}

// tty �����ն��ַ�������.
// ����:tty - ��Ӧtty �ն˽ṹָ��;mask - �ź�����λ
VOID tty_intr( TTY_Struct * tty, LONG mask )
{
	LONG i;
	// ���tty �������С�ڵ���0,���˳�
	if ( tty->pgrp <= 0 )
	{
		return;
	}
	// ɨ����������,��tty ��Ӧ�������������ָ�����ź�
	for ( i = 0; i < NR_TASKS; i++ )
	{
		// �����������ָ�벻Ϊ��,��������ŵ���tty ���,�����ø�����ָ�����ź�mask
		if ( task[ i ] && task[ i ]->pgrp == tty->pgrp )
		{
			task[ i ]->signal |= mask;
		}
	}
}

// ������л����������ý��̽�����жϵ�˯��״̬.
// ����:queue - ָ�����е�ָ��.
// ������ȡ���л��������ַ�ʱ���ô˺���
static VOID sleep_if_empty( struct tty_queue * queue )
{
	cli();
	// ����ǰ����û���ź�Ҫ������ָ���Ķ��л�������,���ý��̽�����ж�˯��״̬,����
	// ���еĽ��̵ȴ�ָ��ָ��ý���.
	while ( !current->signal && EMPTY( *queue ) )
	{
		interruptible_sleep_on( &queue->proc_list );
	}

	sti();
}

// �����л����������ý��̽�����жϵ�˯��״̬.
// ����:queue - ָ�����е�ָ��.
// �����������л�������д��ʱ���ô˺���

static VOID sleep_if_full( struct tty_queue * queue )
{
	if ( !FULL( *queue ) )
	{
		return;
	}

	cli();

	// �������û���ź���Ҫ�����Ҷ��л������п���ʣ��������<128,���ý��̽�����ж�˯��״̬,
	// ���øö��еĽ��̵ȴ�ָ��ָ��ý���

	while ( !current->signal && LEFT( *queue ) < 128 )
	{
		interruptible_sleep_on( &queue->proc_list );
	}
	sti();
}

// �ȴ�����.
// �������̨�Ķ����л����������ý��̽�����жϵ�˯��״̬.

VOID wait_for_keypress()
{
	sleep_if_empty( &tty_table[ 0 ].secondary );
}

// ���Ƴɹ淶ģʽ�ַ�����.
// ��ָ��tty �ն˶��л������е��ַ����Ƴɹ淶( �� )ģʽ�ַ�������ڸ�������( �淶ģʽ���� )��.
// ����:tty - ָ���ն˵�tty �ṹ
VOID copy_to_cooked( TTY_Struct * tty )
{
	CHAR c;

	// ���tty �Ķ����л��������ղ��Ҹ������л�����Ϊ��,��ѭ��ִ�����д���
	while ( !EMPTY( tty->read_q ) && !FULL( tty->secondary ) )
	{
		// �Ӷ���β��ȡһ�ַ���c,��ǰ��βָ��
		GETCH( tty->read_q, c );

		// ����������ַ�,��������ģʽ��־�����д���.
		// ������ַ��ǻس���CR( 13 ),��:���س�ת���б�־CRNL ��λ�򽫸��ַ�ת��Ϊ���з�NL( 10 );
		// ���������Իس���־NOCR ��λ,����Ը��ַ�,�������������ַ�

		if ( c == 13 )
		{
			if ( I_CRNL( tty ) )
			{
				c = 10;
			}
			else if ( I_NOCR( tty ) )
			{
				continue;
			}
		}
		else if ( c == 10 && I_NLCR( tty ) )// ������ַ��ǻ��з�NL( 10 )���һ���ת�س���־NLCR ��λ,����ת��Ϊ�س���CR( 13 )
		{
			c = 13;
		}

		if ( I_UCLC( tty ) )				// �����дתСд��־UCLC ��λ,�򽫸��ַ�ת��ΪСд�ַ�
		{
			c = tolower( c );
		}

		if ( L_CANON( tty ) )				// �������ģʽ��־���й淶( �� )ģʽ��־CANON ��λ,��������´���
		{
			if ( c == KILL_CHAR( tty ) )	// ������ַ��Ǽ�����ֹ�����ַ�KILL( ^U ),�����ɾ�������д���
			{
				/* deal with killing the input line */

				// ���tty �������в���,���߸������������һ���ַ��ǻ���NL( 10 ),���߸��ַ����ļ������ַ�
				// ( ^D ),��ѭ��ִ�����д���.

				while ( !(
							EMPTY( tty->secondary ) ||
							( c = LAST( tty->secondary ) ) == 10 ||
							c == EOF_CHAR( tty ) 
						 ) 
					  ) 
				{

					// ������ػ��Ա�־ECHO ��λ,��ô:���ַ��ǿ����ַ�( ֵ<32 ),����tty ��д�����з������
					// �ַ�ERASE.�ٷ���һ�������ַ�ERASE,���ҵ��ø�tty ��д����
					if ( L_ECHO( tty ) ) 
					{
						if ( c < 32 )
						{
							PUTCH( 127, tty->write_q );
						}

						PUTCH( 127, tty->write_q );

						tty->write( tty );
					}
					// ��tty ��������ͷָ����� 1 �ֽ�
					DEC( tty->secondary.head );
				}
				continue;	
			}
			// ������ַ���ɾ�������ַ�ERASE( ^H ),��ô
			if ( c == ERASE_CHAR( tty ) ) 
			{
				// ��tty �ĸ�������Ϊ��,���������һ���ַ��ǻ��з�NL( 10 ),�������ļ�������,�������������ַ�
				if (
					  EMPTY( tty->secondary ) ||
					( c = LAST( tty->secondary ) ) == 10 ||
					  c == EOF_CHAR( tty ) 
					)
				{
					continue;
				}
				// ������ػ��Ա�־ECHO ��λ,��ô:���ַ��ǿ����ַ�( ֵ<32 ),����tty ��д�����з������
				// �ַ�ERASE.�ٷ���һ�������ַ�ERASE,���ҵ��ø�tty ��д����
				if ( L_ECHO( tty ) )
				{
					if ( c < 32 )
					{
						PUTCH( 127, tty->write_q );
					}

					PUTCH( 127, tty->write_q );
					tty->write( tty );
				}
				// ��tty ��������ͷָ�����1 �ֽ�,�������������ַ�
				DEC( tty->secondary.head );
				continue;
			}
			//������ַ���ֹͣ�ַ�( ^S ),����tty ֹͣ��־,�������������ַ�
			if ( c == STOP_CHAR( tty ) ) 
			{
				tty->stopped = 1;
				continue;
			}
			// ������ַ���ֹͣ�ַ�( ^Q ),��λtty ֹͣ��־,�������������ַ�.
			if ( c == START_CHAR( tty ) ) 
			{
				tty->stopped = 0;
				continue;
			}
		}
		// ������ģʽ��־����ISIG ��־��λ,�����յ�INTR��QUIT��SUSP ��DSUSP �ַ�ʱ,��ҪΪ����
		// ������Ӧ���ź�
		if ( L_ISIG( tty ) )
		{
			// ������ַ��Ǽ����жϷ�( ^C ),����ǰ���̷��ͼ����ж��ź�,������������һ�ַ�
			if ( c == INTR_CHAR( tty ) ) 
			{
				tty_intr( tty, INTMASK );
				continue;
			}
			// ������ַ��Ǽ����жϷ�( ^\ ),����ǰ���̷��ͼ����˳��ź�,������������һ�ַ�
			if ( c == QUIT_CHAR( tty ) ) 
			{
				tty_intr( tty, QUITMASK );
				continue;
			}
		}
		// ������ַ��ǻ��з�NL( 10 ),�������ļ�������EOF( ^D ),������������ַ�����1.[ ?? ]
		if ( c == 10 || c == EOF_CHAR( tty ) )
		{
			tty->secondary.data++;
		}

		// �������ģʽ��־���л��Ա�־ECHO ��λ,��ô,����ַ��ǻ��з�NL( 10 ),�򽫻��з�NL( 10 )
		// �ͻس���CR( 13 )����tty д���л�������;����ַ��ǿ����ַ�( �ַ�ֵ<32 )���һ��Կ����ַ���־
		// ECHOCTL ��λ,���ַ�'^'���ַ�c+64 ����tty д������( Ҳ������ʾ^C��^H �� );���򽫸��ַ�
		// ֱ�ӷ���tty д���������.�����ø�tty ��д��������
		if ( L_ECHO( tty ) ) 
		{
			if ( c == 10 )
			{
				PUTCH( 10, tty->write_q );
				PUTCH( 13, tty->write_q );
			}
			else if ( c < 32 ) 
			{
				if ( L_ECHOCTL( tty ) ) 
				{
					PUTCH( '^'   , tty->write_q );
					PUTCH( c + 64, tty->write_q );
				}
			}
			else
			{
				PUTCH( c, tty->write_q );
			}
			tty->write( tty );
		}
		// �����ַ����븨��������
		PUTCH( c, tty->secondary );
	}
	// ���ѵȴ��ø���������еĽ���( ����еĻ� )
	wake_up( &tty->secondary.proc_list );
}

// tty ������.
// ����:channel - ���豸��;buf - ������ָ��;nr - �����ֽ���.
// �����Ѷ��ֽ���

LONG tty_read( unsigned channel, CHAR * buf, LONG nr )
{
	TTY_Struct *	tty;
	CHAR			c, *b = buf;
	LONG			minimum, time, flag = 0;
	LONG			oldalarm;

	// ���汾linux �ں˵��ն�ֻ��3 �����豸,�ֱ��ǿ���̨( 0 )�������ն�1( 1 )�ʹ����ն�2( 2 ).
	// �����κδ���2 �����豸�Ŷ��ǷǷ���.д���ֽ�����ȻҲ����С��0 ��
	if ( channel > 2 || nr < 0 ) 
	{
		return -1;
	}

	// tty ָ��ָ�����豸�Ŷ�Ӧttb_table ���е�tty �ṹ
	tty = &tty_table[ channel ];

	// �������ȱ������ԭ��ʱֵ,Ȼ����ݿ����ַ�VTIME ��VMIN ���ö��ַ������ĳ�ʱ��ʱֵ.
	// �ڷǹ淶ģʽ��,������ֵ�ǳ�ʱ��ʱֵ.MIN ��ʾΪ�����������,��Ҫ��ȡ�������ַ���.
	// TIME ��һ��ʮ��֮һ������ļ�ʱֵ.
	// ����ȡ�����е�( ���� )��ʱֵ( �δ��� ).
	oldalarm = current->alarm;
	// �����ö�������ʱ��ʱֵtime ����Ҫ���ٶ�ȡ���ַ�����minimum
	time = 10L * tty->termios.c_cc[ VTIME ];

	minimum = tty->termios.c_cc[ VMIN ];

	// ��������˶���ʱ��ʱֵtime ��û���������ٶ�ȡ����minimum,��ô�ڶ�������һ���ַ�����
	// ��ʱ��ʱ������������̷���.����������minimum=1
	if ( time && !minimum ) 
	{
		minimum = 1;
		// �������ԭ��ʱֵ��0 ����time+��ǰϵͳʱ��ֵС�ڽ���ԭ��ʱֵ�Ļ�,�����������ý��̶�ʱ
		// ֵΪtime+��ǰϵͳʱ��,����flag ��־
		if ( flag = ( !oldalarm || time + jiffies<oldalarm ) )
		{
			current->alarm = time + jiffies;
		}
	}
	// ������õ����ٶ�ȡ�ַ���>�������ַ���,��������ڴ˴�����ȡ���ַ���
	if ( minimum>nr )
	{
		minimum = nr;
	}

	// ���������ֽ���>0,��ѭ��ִ�����²���
	while ( nr > 0 ) 
	{
		// ���flag ��Ϊ0( ������ԭ��ʱֵ��0 ����time+��ǰϵͳʱ��ֵС�ڽ���ԭ��ʱֵ )���ҽ����ж�
		// ʱ�ź�SIGALRM,��λ���̵Ķ�ʱ�źŲ��ж�ѭ��

		if ( flag && ( current->signal & ALRMMASK ) ) 
		{
			current->signal &= ~ALRMMASK;
			break;
		}
		// �����ǰ�������ź�Ҫ����,���˳�,����0.
		if ( current->signal )
		{
			break;
		}
		// ��������������( �淶ģʽ���� )Ϊ��,���������˹淶ģʽ��־���Ҹ����������ַ���Ϊ0 �Լ�
		// ����ģʽ������п��пռ�>20,�������ж�˯��״̬,���غ��������
		if ( EMPTY( tty->secondary ) || ( L_CANON( tty ) && !tty->secondary.data && LEFT( tty->secondary ) > 20 ) )
		{
			sleep_if_empty( &tty->secondary );
			continue;
		}
		// ִ�����²���,ֱ��nr=0 ���߸����������Ϊ��
		do 
		{
			// ȡ������������ַ�c
			GETCH( tty->secondary, c );
			// ������ַ����ļ�������( ^D )�����ǻ��з�NL( 10 ),������������ַ�����1
			if ( c == EOF_CHAR( tty ) || c == 10 )
			{
				tty->secondary.data--;
			}

			// ������ַ����ļ�������( ^D )���ҹ淶ģʽ��־��λ,�򷵻��Ѷ��ַ���,���˳�
			if ( c == EOF_CHAR( tty ) && L_CANON( tty ) )
			{
				return ( b - buf );
			}
			else 
			{
				// ���򽫸��ַ������û����ݶλ�����buf ��,�����ַ�����1,��������ַ�����Ϊ0,���ж�ѭ��.
				put_fs_byte( c, b++ );

				if ( !--nr )
				{
					break;
				}
			}
		} while ( nr > 0 && !EMPTY( tty->secondary ) );

		// �����ʱ��ʱֵtime ��Ϊ0 ���ҹ淶ģʽ��־û����λ( �ǹ淶ģʽ ),��ô

		if ( time && !L_CANON( tty ) )
		{
			// �������ԭ��ʱֵ��0 ����time+��ǰϵͳʱ��ֵС�ڽ���ԭ��ʱֵ�Ļ�,�����������ý��̶�ʱֵ
			// Ϊtime+��ǰϵͳʱ��,����flag ��־.�����ý��̵Ķ�ʱֵ���ڽ���ԭ��ʱֵ
			if ( flag = ( !oldalarm || time + jiffies < oldalarm ) )
			{
				current->alarm = time + jiffies;
			}
			else
			{
				current->alarm = oldalarm;
			}
		}
		// ����淶ģʽ��־��λ,��ô��û�ж���1 ���ַ����ж�ѭ��.�������Ѷ�ȡ�����ڻ��������Ҫ
		// ���ȡ���ַ���,��Ҳ�ж�ѭ��.
		if ( L_CANON( tty ) ) 
		{
			if ( b - buf )
			{
				break;
			}
		}
		else if ( b - buf >= minimum )
		{
			break;
		}
	}
	// �ý��̵Ķ�ʱֵ���ڽ���ԭ��ʱֵ.
	current->alarm = oldalarm;
	// ����������źŲ���û�ж�ȡ�κ��ַ�,�򷵻س����( ��ʱ )
	if ( current->signal && !( b - buf ) )
	{
		return -EINTR;
	}
	return ( b - buf );
}

// tty д����.
// ����:channel - ���豸��;buf - ������ָ��;nr - д�ֽ���.
// ������д�ֽ���.

LONG tty_write( unsigned channel, CHAR * buf, LONG nr )
{
	static int		cr_flag = 0;
	TTY_Struct *	tty;
	CHAR			c, *b = buf;

	// ���汾linux �ں˵��ն�ֻ��3 �����豸,�ֱ��ǿ���̨( 0 )�������ն�1( 1 )�ʹ����ն�2( 2 ).
	// �����κδ���2 �����豸�Ŷ��ǷǷ���.д���ֽ�����ȻҲ����С��0 ��
	if ( channel > 2 || nr < 0 ) 
	{
		return -1;
	}

	// tty ָ��ָ�����豸�Ŷ�Ӧttb_table ���е�tty �ṹ
	tty = channel + tty_table;

	// �ַ��豸��һ��һ���ַ����д����,�����������nr ����0 ʱ��ÿ���ַ�����ѭ������
	while ( nr>0 ) 
	{
		// �����ʱtty ��д��������,��ǰ���̽�����жϵ�˯��״̬
		sleep_if_full( &tty->write_q );
		// �����ǰ�������ź�Ҫ����,���˳�,����0
		if ( current->signal )
		{
			break;
		}
		// ��Ҫд���ֽ���>0 ����tty ��д���в���ʱ,ѭ��ִ�����²���
		while ( nr > 0 && !FULL( tty->write_q ) )
		{
			// ���û����ݶ��ڴ���ȡһ�ֽ�c
			c = get_fs_byte( b );

			// ����ն����ģʽ��־���е�ִ����������־OPOST ��λ,��ִ���������ʱ�������.
			if ( O_POST( tty ) ) 
			{
				// ������ַ��ǻس���'\r'( CR,13 )���һس���ת���з���־OCRNL ��λ,�򽫸��ַ����ɻ��з�
				// '\n'( NL,10 );����������ַ��ǻ��з�'\n'( NL,10 )���һ���ת�س����ܱ�־ONLRET ��λ�Ļ�,
				// �򽫸��ַ����ɻس���'\r'( CR,13 )
				if ( c == '\r' && O_CRNL( tty ) )
				{
					c = '\n';
				}
				else if ( c == '\n' && O_NLRET( tty ) )
				{
					c = '\r';
				}
				// ������ַ��ǻ��з�'\n'���һس���־cr_flag û����λ,����ת�س�-���б�־ONLCR ��λ�Ļ�,
				// ��cr_flag ��λ,����һ�س�������д������.Ȼ�����������һ���ַ�
				if ( c == '\n' && !cr_flag && O_NLCR( tty ) ) 
				{
					cr_flag = 1;
					PUTCH( 13, tty->write_q );
					continue;
				}
				// ���Сдת��д��־OLCUC ��λ�Ļ�,�ͽ����ַ�ת�ɴ�д�ַ�
				if ( O_LCUC( tty ) )
				{
					c = toupper( c );
				}
			}
			// �û����ݻ���ָ��b ǰ��1 �ֽ�;��д�ֽ�����1 �ֽ�;��λcr_flag ��־,�������ֽڷ���ttyд������
			b++;
			nr--;
			cr_flag = 0;
			PUTCH( c, tty->write_q );
		}
		// ���ֽ�ȫ��д��,����д��������,�����ִ�е�����.���ö�Ӧtty ��д����,�������ֽ�Ҫд,
		// ��ȴ�д���в���,���Ե��õ��ȳ���,��ȥִ����������.
		tty->write( tty );

		if ( nr > 0 )
		{
			schedule();
		}
	}
	return ( b - buf );		// ����д����ֽ���
}

/*
* Jeh, sometimes I really like the 386.
* This routine is called from an interrupt,
* and there should be absolutely no problem
* with sleeping even in an interrupt ( I hope ).
* Of course, if somebody proves me wrong, I'll
* hate intel for all time :- ). We'll have to
* be careful and see to reinstating the interrupt
* chips before calling this, though.
*
* I don't think we sleep here under normal circumstances
* anyway, which is good, as the task sleeping might be
* totally innocent.
*/

// tty �жϴ�����ú��� - ִ��tty �жϴ���.
// ����:tty - ָ����tty �ն˺�( 0,1 ��2 ).
// ��ָ��tty �ն˶��л������е��ַ����Ƴɹ淶( �� )ģʽ�ַ�������ڸ�������( �淶ģʽ���� )��.
// �ڴ��ڶ��ַ��ж�( rs_io.s, 109 )�ͼ����ж�( kerboard.S, 69 )�е���
VOID do_tty_interrupt( LONG tty )
{
	copy_to_cooked( tty_table + tty );
}

// �ַ��豸��ʼ������.��,Ϊ�Ժ���չ��׼��
VOID chr_dev_init()
{
}
