/*
*  linux/kernel/floppy.c
*
*  ( C ) 1991  Linus Torvalds
*/

/*
* 02.12.91 - Changed to static variables to indicate need for fp_reset
* and recalibrate. This makes some things easier ( output_byte fp_reset
* checking etc ), and means less interrupt jumping in case of errors,
* so the code is hopefully easier to understand.
*/

/*
* This file is certainly a mess. I've tried my best to get it working,
* but I don't like programming floppies, and I have only one anyway.
* Urgel. I should check for more errors, and do more graceful error
* recovery. Seems there are problems with several drives. I've tried to
* correct them. No promises.
*/

/*
* As with hd.c, all routines within this file can ( and will ) be called
* by interrupts, so extreme caution is needed. A hardware interrupt
* handler may not sleep, or a kernel panic will happen. Thus I cannot
* call "floppy-on" directly, but have to set a special timer interrupt
* etc.
*
* Also, I'm not certain this works on more than 1 floppy. Bugs may
* abund.
*/

/*
* ��ͬhd.c �ļ�һ��,���ļ��е������ӳ����ܹ����жϵ���,������Ҫ�ر�
* ��С��.Ӳ���жϴ�������ǲ���˯�ߵ�,�����ں˾ͻ�ɵ��( ���� )?.��˲���
* ֱ�ӵ���"floppy-on",��ֻ������һ�������ʱ���жϵ�.
*
* ����,�Ҳ��ܱ�֤�ó������ڶ���1 ��������ϵͳ�Ϲ���,�п��ܴ��ڴ���.
*/
#include <linux\sched.h>
#include <linux\fs.h>
#include <linux\kernel.h>
#include <linux\fdreg.h>
#include <asm\system.h>
#include <asm\io.h>
#include <asm\segment.h>

#define MAJOR_NR 2		// ���������豸����2
#include "blk.h"		// ���豸ͷ�ļ�

static LONG fp_recalibrate = 0;
static LONG fp_reset = 0;
static LONG seek = 0;

extern UCHAR current_DOR;	// ��ǰ��������Ĵ���( Digital Output Register )

// �ֽ�ֱ�����
static __inline immoutb_p( LONG val, USHORT port )
{
	__asm mov	eax, val
	__asm mov	dx, port
	__asm out	dx, al
	__asm jmp	LN1
LN1 :
	__asm jmp	LN2
LN2 :
	;
}

// �������������ڼ����������豸��.���豸�� = TYPE*4 + DRIVE.���㷽���μ��б��
#define TYPE( x ) ( ( x )>>2 )
#define DRIVE( x ) ( ( x )&0x03 )
/*
* Note that MAX_ERRORS=8 doesn't imply that we retry every bad read
* max 8 times - some types of errors increase the errorcount by 2,
* so we might actually retry only 5-6 times before giving up.
*/

/*
 * ע��,���涨��MAX_ERRORS=8 ������ʾ��ÿ�ζ����������8 �� - ��Щ����
 * �Ĵ��󽫰ѳ������ֵ��2,��������ʵ�����ڷ�������֮ǰֻ�賢��5-6 �鼴��.
 */

#define MAX_ERRORS 8

/*
 * globals used by 'result()'
 */

/* �����Ǻ���'result()'ʹ�õ�ȫ�ֱ��� */
// ��Щ״̬�ֽ��и�����λ�ĺ�����μ�include/linux/fdreg.h ͷ�ļ�
#define MAX_REPLIES 7		// FDC ��෵��7 �ֽڵĽ����Ϣ
static UCHAR reply_buffer[ MAX_REPLIES ] = {0};	// ���FDC ���صĽ����Ϣ
#define ST0 ( reply_buffer[ 0 ] )	// ���ؽ��״̬�ֽ�0
#define ST1 ( reply_buffer[ 1 ] )	// ���ؽ��״̬�ֽ�1
#define ST2 ( reply_buffer[ 2 ] )	// ���ؽ��״̬�ֽ�2
#define ST3 ( reply_buffer[ 3 ] )	// ���ؽ��״̬�ֽ�3

/*
 * This struct defines the different floppy types. Unlike minix
 * linux doesn't have a "search for right type"-type, as the code
 * for that is convoluted and weird. I've got enough problems with
 * this driver as it is.
 *
 * The 'stretch' tells if the tracks need to be boubled for some
 * types ( ie 360kB diskette in 1.2MB drive etc ). Others should
 * be self-explanatory.
 */

/*
 * ��������̽ṹ�����˲�ͬ����������.��minix ��ͬ����,linux û��
 * "������ȷ������"-����,��Ϊ���䴦��Ĵ������˷ѽ��ҹֵֹ�.������
 * �Ѿ���������������������.
 *
 * ��ĳЩ���͵�����( ������1.2MB �������е�360kB ���̵� ),'stretch'����
 * ���ŵ��Ƿ���Ҫ���⴦��.��������Ӧ����������.
 */

// ���̲�����:
// size ��С( ������ );
// sect ÿ�ŵ�������;
// head ��ͷ��;
// track �ŵ���;
// stretch �Դŵ��Ƿ�Ҫ���⴦��( ��־ );
// gap ������϶����( �ֽ��� );
// rate ���ݴ�������;
// spec1 ����( ��4 λ��������,����λ��ͷж��ʱ�� )
static struct floppy_struct
{
	ULONG size, sect, head, track, stretch;
	UCHAR gap, rate, spec1;
} floppy_type[] = {
	{ 0		, 0		, 0	, 0		, 0	, 0x00	, 0x00, 0x00 },		/* no testing			*/
	{ 720	, 9		, 2	, 40	, 0	, 0x2A	, 0x02, 0xDF },		/* 360kB PC diskettes	*/
	{ 2400	, 15	, 2	, 80	, 0	, 0x1B	, 0x00, 0xDF },		/* 1.2 MB AT-diskettes	*/
	{ 720	, 9		, 2	, 40	, 1	, 0x2A	, 0x02, 0xDF },		/* 360kB in 720kB drive */
	{ 1440	, 9		, 2	, 80	, 0	, 0x2A	, 0x02, 0xDF },		/* 3.5" 720kB diskette	*/
	{ 720	, 9		, 2	, 40	, 1	, 0x23	, 0x01, 0xDF },		/* 360kB in 1.2MB drive */
	{ 1440	, 9		, 2	, 80	, 0	, 0x23	, 0x01, 0xDF },		/* 720kB in 1.2MB drive */
	{ 2880	, 18	, 2	, 80	, 0	, 0x1B	, 0x00, 0xCF },		/* 1.44MB diskette		*/
};
/*
* Rate is 0 for 500kb/s, 2 for 300kbps, 1 for 250kbps
* Spec1 is 0xSH, where S is stepping rate ( F=1ms, E=2ms, D=3ms etc ),
* H is head unload time ( 1=16ms, 2=32ms, etc )
*
* Spec2 is ( HLD<<1 | ND ), where HLD is head load time ( 1=2ms, 2=4 ms etc )
* and ND is set means no DMA. Hardcoded to 6 ( HLD=6ms, use DMA ).
*/

/*
 * ��������rate:0 ��ʾ500kb/s,1 ��ʾ300kbps,2 ��ʾ250kbps.
 * ����spec1 ��0xSH,����S �ǲ�������( F-1 ����,E-2ms,D=3ms �� ),
 * H �Ǵ�ͷж��ʱ��( 1=16ms,2=32ms �� )
 *
 * spec2 ��( HLD<<1 | ND ),����HLD �Ǵ�ͷ����ʱ��( 1=2ms,2=4ms �� )
 * ND ��λ��ʾ��ʹ��DMA( No DMA ),�ڳ�����Ӳ�����6( HLD=6ms,ʹ��DMA ).
 */

extern VOID floppy_interrupt();
extern CHAR tmp_floppy_area[ 1024 ];

/*
 * These are global variables, as that's the easiest way to give
 * information to interrupts. They are the data used for the current
 * request.
 */

/*
 * ������һЩȫ�ֱ���,��Ϊ���ǽ���Ϣ�����жϳ�����򵥵ķ�ʽ.������
 * ���ڵ�ǰ���������.
 */
static LONG						cur_spec1				= -1;
static LONG						cur_rate				= -1;
static struct floppy_struct *	floppy					= floppy_type;
static UCHAR					current_drive			= 0;
static UCHAR					sector					= 0;
static UCHAR					head					= 0;
static UCHAR					track					= 0;
static UCHAR					seek_track				= 0;
static UCHAR					current_track			= 255;
static UCHAR					command					= 0;
UCHAR							selected				= 0;
Task_Struct *					wait_on_floppy_select	= NULL;

// �ͷ�( ȡ��ѡ���� )����( ���� ).
// ��������Ĵ���( DOR )�ĵ�2 λ����ָ��ѡ�������( 0-3 ��ӦA-D )
VOID floppy_deselect( ULONG nr )
{
	if ( nr != ( current_DOR & 3 ) )
	{
		printk( "floppy_deselect: drive not selected\n\r" );
	}

	selected = 0;

	wake_up( &wait_on_floppy_select );
}

/*
 * floppy-change is never called from an interrupt, so we can relax a bit
 * here, sleep etc. Note that floppy-on tries to set current_DOR to point
 * to the desired drive, but it will probably not survive the sleep if
 * several floppies are used at the same time: thus the loop.
 */

/*
 * floppy-change()���Ǵ��жϳ����е��õ�
 * ע��floppy-on()�᳢������current_DOR ָ�������������,����ͬʱʹ�ü���
 * ����ʱ����˯��:��˴�ʱֻ��ʹ��ѭ����ʽ.
 */
// ���ָ�����������̸������.������̸������򷵻�1,���򷵻�0.
LONG floppy_change( ULONG nr )
{
repeat:

	floppy_on( nr );	// ����ָ������nr( kernel/sched.c,251 )

	// �����ǰѡ�����������ָ��������nr,�����Ѿ�ѡ������������,���õ�ǰ���������жϵȴ�״̬
	while ( ( current_DOR & 3 ) != nr && selected )
	{
		interruptible_sleep_on( &wait_on_floppy_select );
	}

	// �����ǰû��ѡ�������������ߵ�ǰ���񱻻���ʱ,��ǰ������Ȼ����ָ��������nr,��ѭ���ȴ�.
	if ( ( current_DOR & 3 ) != nr )
	{
		goto repeat;
	}

	// ȡ��������Ĵ���ֵ,������λ( λ7 )��λ,���ʾ�����Ѹ���,��ʱ�ر���ﲢ�˳�����1.
	// ����ر�����˳�����0

	if ( inb( FD_DIR ) & 0x80 ) 
	{
		floppy_off( nr );
		return 1;
	}

	floppy_off( nr );
	return 0;
}

// ����( ��ʼ�� )����DMA ͨ��
static __inline copy_buffer( VOID *from, VOID *to )
{
	__asm mov	ecx, BLOCK_SIZE / 4
	__asm mov	esi, from
	__asm mov	edi, to
	__asm cld
	__asm rep	movsw
}

// ����( ��ʼ�� )����DMA ͨ��
static VOID setup_DMA()
{
	LONG addr = ( LONG )CURRENT->buffer;	// ��ǰ��������������ڴ���λ��( ��ַ )
	CHAR a;

	cli();

	// ��������������ڴ�1M ���ϵĵط�,��DMA ������������ʱ��������( tmp_floppy_area ���� )
	// ( ��Ϊ8237A оƬֻ����1M ��ַ��Χ��Ѱַ ).�����д������,���轫���ݸ��Ƶ�����ʱ����.

	if ( addr >= 0x100000 ) {
		addr = ( LONG )tmp_floppy_area;
		if ( command == FD_WRITE )
			copy_buffer( CURRENT->buffer, tmp_floppy_area );
	}

	/* mask DMA 2 *//* ����DMA ͨ��2 */
	// ��ͨ�����μĴ����˿�Ϊ0x10.λ0-1 ָ��DMA ͨ��( 0--3 ),λ2:1 ��ʾ����,0 ��ʾ��������.

	immoutb_p( 4 | 2, 10 );
	/* output command byte. I don't know why, but everyone ( minix, */
	/* sanches & canton ) output this twice, first to 12 then to 11 */

	/* ��������ֽ�.���ǲ�֪��Ϊʲô,����ÿ����( minix,*/
	/* sanches ��canton )���������,������12 ��,Ȼ����11 �� */
	// ����Ƕ���������DMA �������˿�12 ��11 д��ʽ��( ����0x46,д��0x4A )
	a = ( CHAR )( ( command == FD_READ ) ? DMA_READ : DMA_WRITE );
	__asm mov	al, a
	__asm out	12, al
	__asm jmp	LN1
LN1 :
	__asm jmp	LN2
LN2 :
	__asm out	11, al
	__asm jmp	LN3
LN3 :
	__asm jmp	LN4
LN4 :
	/* 8 low bits of addr */
	/* 8 low bits of addr *//* ��ַ��0-7 λ */
	// ��DMA ͨ��2 д���/��ǰ��ַ�Ĵ���( �˿�4 ).

	immoutb_p( addr, 4 );
	addr >>= 8;
	
	/* bits 8-15 of addr */
	//bits 8-15 of addr *//* ��ַ��8-15 λ 

	immoutb_p( addr, 4 );
	addr >>= 8;
	/* bits 16-19 of addr */
	// DMA ֻ������1M �ڴ�ռ���Ѱַ,���16-19 λ��ַ�����ҳ��Ĵ���( �˿�0x81 )
	immoutb_p( addr, 0x81 );
	/* low 8 bits of count-1 ( 1024-1=0x3ff ) */
	// ��DMA ͨ��2 д���/��ǰ�ֽڼ�����ֵ( �˿�5 )
	immoutb_p( 0xff, 5 );
	/* high 8 bits of count-1 */
	// һ�ι�����1024 �ֽ�( �������� )
	immoutb_p( 3, 5 );
	/* activate DMA 2 */
	/* activate DMA 2 *//* ����DMA ͨ��2 ������ */
	// ��λ��DMA ͨ��2 ������,����DMA2 ����DREQ �ź�
	immoutb_p( 0 | 2, 10 );
	sti();
}

// �����̿��������һ���ֽ�����( �������� )
static VOID output_byte( CHAR byte )
{
	LONG counter;
	UCHAR status;

	if ( fp_reset )
		return;

	// ѭ����ȡ��״̬������FD_STATUS( 0x3f4 )��״̬.���״̬��STATUS_READY ����STATUS_DIR=0
	// ( CPU??FDC ),�������ݶ˿����ָ���ֽ�
	for ( counter = 0; counter < 10000; counter++ ) 
	{
		status = inb_p( FD_STATUS ) & ( STATUS_READY | STATUS_DIR );

		if ( status == STATUS_READY ) 
		{
			outb( byte, FD_DATA );
			return;
		}
	}
	// �����ѭ��1 ��ν��������ܷ���,���ø�λ��־,����ӡ������Ϣ
	fp_reset = 1;
	printk( "Unable to send byte to FDC\n\r" );
}


// ��ȡFDC ִ�еĽ����Ϣ.
// �����Ϣ���7 ���ֽ�,�����reply_buffer[]��.���ض���Ľ���ֽ���,������ֵ=-1
// ��ʾ����
static LONG result()
{
	LONG i = 0, counter, status;

	if ( fp_reset )
	{
		return -1;
	}

	for ( counter = 0; counter < 10000; counter++ ) 
	{
		status = inb_p( FD_STATUS )&( STATUS_DIR | STATUS_READY | STATUS_BUSY );

		if ( status == STATUS_READY )
		{
			return i;
		}

		if ( status == ( STATUS_DIR | STATUS_READY | STATUS_BUSY ) ) 
		{
			if ( i >= MAX_REPLIES )
			{
				break;
			}
			reply_buffer[ i++ ] = inb_p( FD_DATA );
		}
	}

	fp_reset = 1;

	printk( "Getstatus times out\n\r" );

	return -1;
}

// ���̲��������жϵ��ú���.�������жϴ���������

static VOID bad_flp_intr()
{
	CURRENT->errors++;

	// �����ǰ�����������������������������,��ȡ��ѡ����ǰ����,��������������( ������ ).

	if ( CURRENT->errors > MAX_ERRORS ) 
	{
		floppy_deselect( current_drive );
		end_request( 0 );
	}

	// �����ǰ�������������������������������һ��,���ø�λ��־,����������и�λ����,
	// Ȼ������.��������������У��һ��,����.
	if ( CURRENT->errors > MAX_ERRORS / 2 )
	{
		fp_reset = 1;
	}
	else
	{
		fp_recalibrate = 1;
	}
}

/*
* Ok, this interrupt is called after a DMA read/write has succeeded,
* so we check the results, and copy any buffers.
*/

/*
 * OK,������жϴ���������DMA ��/д�ɹ�����õ�,�������ǾͿ��Լ��ִ�н��,
 * �����ƻ������е�����.
 */
// ���̶�д�����ɹ��жϵ��ú���
static VOID rw_interrupt()
{
	// ������ؽ���ֽ���������7,����״̬�ֽ�0��1 ��2 �д��ڳ����־,������д����
	// ����ʾ������Ϣ,�ͷŵ�ǰ������,��������ǰ������.�����ִ�г����������.
	// Ȼ�����ִ�������������.
	// ( 0xf8 = ST0_INTR | ST0_SE  | ST0_ECE | ST0_NR )
	// ( 0xbf = ST1_EOC  | ST1_CRC | ST1_OR  | ST1_ND | ST1_WP | ST1_MAM,Ӧ����0xb7 )
	// ( 0x73 = ST2_CM   | ST2_CRC | ST2_WC  | ST2_BC | ST2_MAM )

	if ( result() != 7 || ( ST0 & 0xf8 ) || ( ST1 & 0xbf ) || ( ST2 & 0x73 ) ) 
	{
		if ( ST1 & 0x02 ) 
		{
			printk( "Drive %d is write protected\n\r", current_drive );
			floppy_deselect( current_drive );
			end_request( 0 );
		}
		else
		{
			bad_flp_intr();
		}

		do_fd_request();

		return;
	}

	// �����ǰ������Ļ�����λ��1M ��ַ����,��˵���˴����̶����������ݻ�������ʱ��������,
	// ��Ҫ���Ƶ�������Ļ�������( ��ΪDMA ֻ����1M ��ַ��ΧѰַ )
	if ( command == FD_READ && ( ULONG )( CURRENT->buffer ) >= 0x100000 )
		copy_buffer( tmp_floppy_area, CURRENT->buffer );
	// �ͷŵ�ǰ����,������ǰ������( �ø��±�־ ),�ټ���ִ����������������
	floppy_deselect( current_drive );
	end_request( 1 );
	do_fd_request();
}

// ����DMA ��������̲�������Ͳ���( ���1 �ֽ�����+ 0~7 �ֽڲ��� )
__inline VOID setup_rw_floppy()
{
	setup_DMA();									// ��ʼ������DMA ͨ��.

	do_floppy = rw_interrupt;						// �������жϵ��ú���ָ��.

	output_byte( command					);		// ���������ֽ�.
	output_byte( head << 2 | current_drive	);		// ���Ͳ���( ��ͷ��+�������� ).
	output_byte( track						);		// ���Ͳ���( �ŵ��� ).
	output_byte( head						);		// ���Ͳ���( ��ͷ�� ).
	output_byte( sector						);		// ���Ͳ���( ��ʼ������ ).
	output_byte( 2							);		// ���Ͳ���( �ֽ���( N=2 )512 �ֽ� )./* sector size = 512 */
	output_byte( (CHAR)floppy->sect			);		// ���Ͳ���( ÿ�ŵ������� ).
	output_byte( floppy->gap				);		// ���Ͳ���( ����������� ).
	output_byte( 0xFF						);		/* sector size ( 0xff when n!=0 ? ) */

	// ���Ͳ���( ��N=0 ʱ,����������ֽڳ��� ),��������.
	// ���ڷ�������Ͳ���ʱ��������,�����ִ����һ���̲�������.
	if ( fp_reset )
	{
		do_fd_request();
	}
}

/*
 * This is the routine called after every seek ( or recalibrate ) interrupt
 * from the floppy controller. Note that the "unexpected interrupt" routine
 * also does a recalibrate, but doesn't come here.
 */

/*
 * ���ӳ�������ÿ�����̿�����Ѱ��( ������У�� )�жϺ󱻵��õ�.ע��
 * "unexpected interrupt"( �����ж� )�ӳ���Ҳ��ִ������У������,�����ڴ˵�.
 */
// Ѱ�������жϵ��ú���.
// ���ȷ��ͼ���ж�״̬����,���״̬��ϢST0 �ʹ�ͷ���ڴŵ���Ϣ.��������ִ�д������
// ��⴦���ȡ���������̲���������.�������״̬��Ϣ���õ�ǰ�ŵ�����,Ȼ����ú���
// setup_rw_floppy()����DMA ��������̶�д����Ͳ���
static VOID seek_interrupt()
{
	/* sense drive status *//* ����ж�״̬ */
	// ���ͼ���ж�״̬����,�����������.���ؽ����Ϣ�����ֽ�:ST0 �ʹ�ͷ��ǰ�ŵ���
	output_byte( FD_SENSEI );

	// ������ؽ���ֽ���������2,����ST0 ��ΪѰ������,���ߴ�ͷ���ڴŵ�( ST1 )�������趨�ŵ�,
	// ��˵�������˴���,����ִ�м������������,Ȼ�����ִ������������,���˳�
	if ( result() != 2 || ( ST0 & 0xF8 ) != 0x20 || ST1 != seek_track ) 
	{
		bad_flp_intr();
		do_fd_request();
		return;
	}

	current_track = ST1;	// ���õ�ǰ�ŵ�.
	setup_rw_floppy();		// ����DMA ��������̲�������Ͳ���
}

/*
 * This routine is called when everything should be correctly set up
 * for the transfer ( ie floppy motor is on and the correct floppy is
 * selected ).
 */

/*
 * �ú������ڴ��������������Ϣ����ȷ���úú󱻵��õ�( Ҳ����������ѿ���
 * ������ѡ������ȷ������( ���� ).
 */
static VOID transfer()
{
	// ���ȿ���ǰ�����������Ƿ����ָ���������Ĳ���,�����Ǿͷ����������������������Ӧ
	// ����( ����1:��4 λ��������,����λ��ͷж��ʱ��;����2:��ͷ����ʱ�� )
	if ( cur_spec1 != floppy->spec1 ) 
	{
		cur_spec1 = floppy->spec1;
		output_byte( FD_SPECIFY );	// �������ô��̲�������
		output_byte( (CHAR)cur_spec1 );		/* hut etc */
		output_byte( 6 );			/* Head load time =6ms, DMA */
	}

	// �жϵ�ǰ���ݴ��������Ƿ���ָ����������һ��,�����Ǿͷ���ָ������������ֵ�����ݴ���
	// ���ʿ��ƼĴ���( FD_DCR )
	if ( cur_rate != floppy->rate )
	{
		outb_p( (UCHAR)cur_rate = floppy->rate, FD_DCR );
	}
	// �����ؽ����Ϣ��������,���ٵ�������������,������
	if ( fp_reset ) 
	{
		do_fd_request();
		return;
	}

	// ��Ѱ����־Ϊ��( ����ҪѰ�� ),������DMA ��������Ӧ��д��������Ͳ���,Ȼ�󷵻�

	if ( !seek )
	{
		setup_rw_floppy();
		return;
	}

	// ����ִ��Ѱ������.�������жϴ�����ú���ΪѰ���жϺ���

	do_floppy = seek_interrupt;

	// �����ʼ�ŵ��Ų����������ʹ�ͷѰ������Ͳ���
	if ( seek_track ) 
	{
		output_byte( FD_SEEK );						// ���ʹ�ͷѰ������
		output_byte( head << 2 | current_drive );	//���Ͳ���:��ͷ��+��ǰ������
		output_byte( seek_track );					// ���Ͳ���:�ŵ���
	}
	else
	{
		output_byte( FD_RECALIBRATE );				// ��������У������
		output_byte( head << 2 | current_drive );	//���Ͳ���:��ͷ��+��ǰ������
	}

	// �����λ��־����λ,�����ִ������������
	if ( fp_reset )
	{
		do_fd_request();
	}
}

/*
 * Special case - used after a unexpected interrupt ( or fp_reset )
 */

// ��������У���жϵ��ú���.
// ���ȷ��ͼ���ж�״̬����( �޲��� ),������ؽ����������,���ø�λ��־,����λ����
// У����־.Ȼ���ٴ�ִ����������
static VOID recal_interrupt()
{
	output_byte( FD_SENSEI );						// ���ͼ���ж�״̬����

	if ( result() != 2 || ( ST0 & 0xE0 ) == 0x60 )	// ������ؽ���ֽ���������2
	{
		fp_reset = 1;								// �쳣����,���ø�λ��־
	}
	else
	{
		fp_recalibrate = 0;						// ����λ����У����־
	}
	do_fd_request();							// ִ������������
}

// ���������ж������жϵ��ú���.
// ���ȷ��ͼ���ж�״̬����( �޲��� ),������ؽ����������,���ø�λ��־,����������
// У����־
VOID unexpected_floppy_interrupt()
{
	output_byte( FD_SENSEI );						// ���ͼ���ж�״̬����

	if ( result() != 2 || ( ST0 & 0xE0 ) == 0x60 )	// ������ؽ���ֽ���������2 ������
	{
		fp_reset = 1;								
	}
	else											// ����������У����־
	{
		fp_recalibrate = 1;
	}
}

// ��������У��������.
// �����̿�����FDC ��������У������Ͳ���,����λ����У����־

static VOID recalibrate_floppy()
{
	fp_recalibrate	= 0;							// ��λ����У����־.
	current_track	= 0;							// ��ǰ�ŵ��Ź���.
	do_floppy		= recal_interrupt;				// �������жϵ��ú���ָ��ָ������У�����ú���.

	output_byte( FD_RECALIBRATE );					// ��������:����У��.
	output_byte( head << 2 | current_drive );		// ���Ͳ���:( ��ͷ�ż� )��ǰ��������.

	if ( fp_reset )									// �������( ��λ��־����λ )�����ִ����������.
	{
		do_fd_request();
	}
}

// ���̿�����FDC ��λ�жϵ��ú���.�������жϴ�������е���.
// ���ȷ��ͼ���ж�״̬����( �޲��� ),Ȼ��������صĽ���ֽ�.���ŷ����趨������������
// ����ز���,����ٴε���ִ����������

static VOID reset_interrupt()
{
	output_byte( FD_SENSEI );			// ���ͼ���ж�״̬����.
	result();							// ��ȡ����ִ�н���ֽ�.
	output_byte( FD_SPECIFY );			// �����趨������������.
	output_byte( (UCHAR)cur_spec1 );	// hut etc */// ���Ͳ���.
	output_byte( 6 );					// Head load time =6ms, DMA
	do_fd_request();					// ����ִ����������.
}

/*
* fp_reset is done by pulling bit 2 of DOR low for a while.
*/

/* FDC ��λ��ͨ������������Ĵ���( DOR )λ2 ��0 һ���ʵ�ֵ� */
// ��λ���̿�����
static VOID reset_floppy()
{
	LONG i;

	fp_reset		= 0;
	cur_spec1		= -1;
	cur_rate		= -1;
	fp_recalibrate	= 1;

	printk( "Reset-floppy called\n\r" );

	cli();

	do_floppy = reset_interrupt;			// �����������жϴ�������е��õĺ���

	outb_p( current_DOR & ~0x04, FD_DOR );	// �����̿�����FDC ִ�и�λ����

	for ( i = 0; i < 100; i++ )
		__asm nop;

	outb( current_DOR, FD_DOR );			// ���������̿�����

	sti();
}

// ����������ʱ�жϵ��ú���.
// ���ȼ����������Ĵ���( DOR ),ʹ��ѡ��ǰָ����������.Ȼ�����ִ�����̶�д����
// ����transfer()
static VOID floppy_on_interrupt()
{
	/* We cannot do a floppy-select, as that might sleep. We just force it */
	//���ǲ�����������ѡ�������,��Ϊ���������ܻ��������˯��.����ֻ����ʹ���Լ�ѡ��
	selected = 1;

	// �����ǰ������������������Ĵ���DOR �еĲ�ͬ,����������DOR Ϊ��ǰ������current_drive.
	// ��ʱ�ӳ� 2 ���δ�ʱ��,Ȼ��������̶�д���亯��transfer().����ֱ�ӵ������̶�д���亯��.

	if ( current_drive != ( current_DOR & 3 ) ) 
	{
		current_DOR &= 0xFC;
		current_DOR |= current_drive;

		outb( current_DOR, FD_DOR );	// ����������Ĵ��������ǰDOR.

		add_timer( 2, &transfer );		// ��Ӷ�ʱ����ִ�д��亯��.
	}
	else
	{
		transfer();					// ִ�����̶�д���亯��
	}
}

// ���̶�д���������
VOID do_fd_request()
{
	ULONG block;

	seek = 0;

	// �����λ��־����λ,��ִ�����̸�λ����,������
	if ( fp_reset )
	{
		reset_floppy();
		return;
	}

	// �������У����־����λ,��ִ����������У������,������

	if ( fp_recalibrate )
	{
		recalibrate_floppy();
		return;
	}

	// ���������ĺϷ���( �μ�kernel/blk_drv/blk.h,127 )
	INIT_REQUEST;
	// ��������ṹ�������豸���е���������( MINOR( CURRENT->dev )>>2 )��Ϊ����ȡ�����̲�����

	floppy = ( MINOR( CURRENT->dev ) >> 2 ) + floppy_type;

	if ( current_drive != CURRENT_DEV )
	{
		seek = 1;
	}
	// �����ǰ������������������ָ����������,���ñ�־seek,��ʾ��Ҫ����Ѱ������.
	// Ȼ�����������豸Ϊ��ǰ������

	current_drive = CURRENT_DEV;

	// ���ö�д��ʼ����.��Ϊÿ�ζ�д���Կ�Ϊ��λ( 1 ��2 ������ ),������ʼ������Ҫ�����
	// ������������С2 ������.��������ô�����������,ִ����һ��������.

	block = CURRENT->sector;			// ȡ��ǰ��������������ʼ������??block

	if ( block + 2 > floppy->size )
	{
		end_request( 0 );				// ���block+2 ���ڴ�����������,��
		goto repeat;					// ������������������
	}

	// ���Ӧ�ڴŵ��ϵ�������,��ͷ��,�ŵ���,��Ѱ�ŵ���( ������������ͬ��ʽ���� )
	sector		= (UCHAR)( block % floppy->sect );			// ��ʼ������ÿ�ŵ�������ȡģ,�ôŵ���������.
	block 	   /= floppy->sect;								// ��ʼ������ÿ�ŵ�������ȡ��,����ʼ�ŵ���.
	head		= (UCHAR)( block % floppy->head );			// ��ʼ�ŵ����Դ�ͷ��ȡģ,�ò����Ĵ�ͷ��.
	track		= (UCHAR)( block / floppy->head );			// ��ʼ�ŵ����Դ�ͷ��ȡ��,�ò����Ĵŵ���.
	seek_track	= track << floppy->stretch;					// ��Ӧ���������������ͽ��е���,��Ѱ����
	
	// ���Ѱ�����뵱ǰ��ͷ���ڴŵ���ͬ,������ҪѰ����־seek
	if ( seek_track != current_track )
	{
		seek = 1;
	}

	sector++;					// ������ʵ�����������Ǵ�1 ����

	if ( CURRENT->cmd == READ )	// ������������Ƕ�����,�������̶�������
	{
		command = FD_READ;
	}
	else if ( CURRENT->cmd == WRITE )	// �������������д����,��������д������
	{
		command = FD_WRITE;
	}
	else
	{
		panic( "do_fd_request: unknown command" );
	}

	// ��Ӷ�ʱ��,����ָ�������������������������ӳٵ�ʱ��( �δ��� ),����ʱʱ�䵽ʱ�͵���
	// ����floppy_on_interrupt()
	add_timer( ticks_to_floppy_on( current_drive ), &floppy_on_interrupt );
}

// ����ϵͳ��ʼ��.
// �������̿��豸����������( do_fd_request() ),�����������ж���( LONG 0x26,��ӦӲ��
// �ж������ź�IRQ6 ),Ȼ��ȡ���Ը��ж��źŵ�����,�������̿�����FDC �����ж������ź�.

VOID floppy_init()
{
	blk_dev[ MAJOR_NR ].request_fn = DEVICE_REQUEST;	// = do_fd_request().

	set_trap_gate( 0x26, &floppy_interrupt );			//���������ж��� LONG 0x26( 38 ).

	outb( inb_p( 0x21 )&~0x40, 0x21 );					// ��λ���̵��ж���������λ,�������̿����������ж������ź�
}
