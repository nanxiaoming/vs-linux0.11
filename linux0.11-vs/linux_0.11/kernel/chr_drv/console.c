/*
*  linux/kernel/console.c
*
*  ( C ) 1991  Linus Torvalds
*/

/*
*	console.c
*
* This module implements the console io functions
*	'VOID con_init()'
*	'VOID con_write( struct tty_queue * queue )'
* Hopefully this will be a rather complete VT102 implementation.
*
* Beeping thanks to John T Kohl.
*/

/*
* console.c
*
* ��ģ��ʵ�ֿ���̨�����������
* 'VOID con_init()'
* 'VOID con_write( struct tty_queue * queue )'
* ϣ������һ���ǳ�������VT102 ʵ��.
*
* ��лJohn T Kohl 
*/

/*
 *  NOTE!!! We sometimes disable and enable interrupts for a short while
 * ( to put a word in video IO ), but this will work even for keyboard
 * interrupts. We know interrupts aren't enabled when getting a keyboard
 * interrupt, as we use trap-gates. Hopefully all is well.
 */

/*
 * ע��!!! ������ʱ���ݵؽ�ֹ�������ж�( �ڽ�һ����( word )�ŵ���ƵIO ),����ʹ
 * ���ڼ����ж���Ҳ�ǿ��Թ�����.��Ϊ����ʹ��������,��������֪���ڻ��һ��
 * �����ж�ʱ�ж��ǲ������.ϣ��һ�о�����.
 */

/*
 * Code to check for different video-cards mostly by Galen Hunt,
 * <g-hunt@ee.utah.edu>
 */

 /*
  * ��ⲻͬ��ʾ���Ĵ���������Galen Hunt ��д��,
  * <g-hunt@ee.utah.edu>
  */
#include <linux\sched.h>
#include <linux\tty.h>
#include <asm\io.h>
#include <asm\system.h>

/*
 * These are set up by the setup-routine at boot-time:
 */

// �μ���boot/setup.s ��ע��,��setup �����ȡ�������Ĳ�����
#define ORIG_X					( *( UCHAR  * )0x90000 )				// ����к�
#define ORIG_Y					( *( UCHAR  * )0x90001 )				// ����к�
#define ORIG_VIDEO_PAGE			( *( USHORT * )0x90004 )				// ��ʾҳ��
#define ORIG_VIDEO_MODE			( ( *( USHORT * )0x90006 ) & 0xff )		// ��ʾģʽ
#define ORIG_VIDEO_COLS 		( ( ( *( USHORT * )0x90006 ) & 0xff00 ) >> 8 )// �ַ�����
#define ORIG_VIDEO_LINES		( 25 )									// ��ʾ����
#define ORIG_VIDEO_EGA_AX		( *( USHORT * )0x90008 )				// [ ?? ]
#define ORIG_VIDEO_EGA_BX		( *( USHORT * )0x9000a )				// ��ʾ�ڴ��С��ɫ��ģʽ.
#define ORIG_VIDEO_EGA_CX		( *( USHORT * )0x9000c )				// ��ʾ�����Բ���.

#define VIDEO_TYPE_MDA			0x10	/* Monochrome Text Display		*/	/* ��ɫ�ı�		*/
#define VIDEO_TYPE_CGA			0x11	/* CGA Display 					*/	/* CGA ��ʾ��	*/
#define VIDEO_TYPE_EGAM			0x20	/* EGA/VGA in Monochrome Mode	*/	/* EGA/VGA ��ɫ	*/
#define VIDEO_TYPE_EGAC			0x21	/* EGA/VGA in Color Mode		*/	/* EGA/VGA ��ɫ	*/

#define NPAR 16

extern VOID keyboard_interrupt();

static UCHAR	video_type;			/* Type of display being used	*/	//ʹ�õ���ʾ����
static ULONG	video_num_columns;	/* Number of text columns		*/	//��Ļ�ı�����
static ULONG	video_size_row;		/* Bytes per row				*/	//ÿ��ʹ�õ��ֽ���
static ULONG	video_num_lines;	/* Number of test lines			*/	//��Ļ�ı�����
static UCHAR	video_page;			/* Initial video page			*/	//��ʼ��ʾҳ��
static ULONG	video_mem_start;	/* Start of video RAM			*/	//��ʾ�ڴ���ʼ��ַ
static ULONG	video_mem_end;		/* End of video RAM ( sort of )	*/	//��ʾ�ڴ����( ĩ�� )��ַ
static USHORT	video_port_reg;		/* Video register select port	*/	//��ʾ���������Ĵ����˿�
static USHORT	video_port_val;		/* Video register value port	*/	//��ʾ�������ݼĴ����˿�
static USHORT	video_erase_char;	/* Char+Attrib to erase with	*/	//�����ַ��������ַ�( 0x0720 )

static ULONG	origin;				/* Used for EGA/VGA fast scroll	*/
static ULONG	scr_end;			/* Used for EGA/VGA fast scroll	*/
static ULONG	pos;				// ��ǰ����Ӧ����ʾ�ڴ�λ��.
static ULONG	x, y;				// ��ǰ���λ��.
static ULONG	top, bottom;		// ����ʱ�����к�;�����к�
// state ���ڱ�������ESC ת������ʱ�ĵ�ǰ����.npar,par[]���ڴ��ESC ���е��м䴦�����
static ULONG	state = 0;			// ANSI ת���ַ����д���״̬
static ULONG	npar, par[ NPAR ];	// ANSI ת���ַ����в��������Ͳ�������
static ULONG	ques = 0;
static UCHAR	attr = 0x07;		// �ַ�����( �ڵװ��� )

static VOID sysbeep();	// ϵͳ��������

/*
 * this is what the terminal answers to a ESC-Z or csi0c
 * query ( = vt100 response ).
 */
#define RESPONSE "\033[ ?1;2c"

/* NOTE! gotoxy thinks x==video_num_columns is ok */
/* ע�⣡gotoxy ������Ϊx==video_num_columns,������ȷ�� */
// ���ٹ�굱ǰλ��.
// ����:new_x - ��������к�;new_y - ��������к�.
// ���µ�ǰ���λ�ñ���x,y,������pos ָ��������ʾ�ڴ��еĶ�Ӧλ��
static __inline VOID gotoxy( ULONG new_x, ULONG new_y )
{
	// �������Ĺ���кų�����ʾ������,���߹���кų�����ʾ���������,���˳�
	if ( new_x > video_num_columns || new_y >= video_num_lines )
	{
		return;
	}

	// ���µ�ǰ������;���¹��λ�ö�Ӧ������ʾ�ڴ���λ�ñ���pos
	x = new_x;
	y = new_y;
	pos = origin + y*video_size_row + ( x << 1 );
}

// ���ù�����ʼ��ʾ�ڴ��ַ
static __inline VOID set_origin()
{
	cli();
	// ����ѡ����ʾ�������ݼĴ���r12,Ȼ��д�������ʼ��ַ���ֽ�.�����ƶ�9 λ,��ʾ�����ƶ�
	// 8 λ,�ٳ���2( 2 �ֽڴ�����Ļ��1 �ַ� ).�������Ĭ����ʾ�ڴ������

	outb_p( 12, video_port_reg );
	outb_p( 0xff & ( ( origin - video_mem_start ) >> 9 ), video_port_val );

	// ��ѡ����ʾ�������ݼĴ���r13,Ȼ��д�������ʼ��ַ���ֽ�.�����ƶ�1 λ��ʾ����2

	outb_p( 13, video_port_reg );
	outb_p( 0xff & ( ( origin - video_mem_start ) >> 1 ), video_port_val );

	sti();
}

// ���Ͼ�һ��( ��Ļ���������ƶ� ).
// ����Ļ���������ƶ�һ��.�μ������б��˵��
static VOID scrup()
{
	ULONG c, D, S;

	// �����ʾ������EGA,��ִ�����²���
	if ( video_type == VIDEO_TYPE_EGAC || video_type == VIDEO_TYPE_EGAM )
	{
		// ����ƶ���ʼ��top=0,�ƶ������bottom=video_num_lines=25,���ʾ�������������ƶ�.
		if ( !top && bottom == video_num_lines ) 
		{
			// ������Ļ��ʾ��Ӧ�ڴ����ʼλ��ָ��origin Ϊ������һ����Ļ�ַ���Ӧ���ڴ�λ��,ͬʱҲ����
			// ��ǰ����Ӧ���ڴ�λ���Լ���Ļĩ��ĩ���ַ�ָ��scr_end ��λ��
			origin	+= video_size_row;
			pos		+= video_size_row;
			scr_end += video_size_row;

			// �����Ļĩ�����һ����ʾ�ַ�����Ӧ����ʾ�ڴ�ָ��scr_end ������ʵ����ʾ�ڴ��ĩ��,��
			// ��Ļ�����ڴ������ƶ�����ʾ�ڴ����ʼλ��video_mem_start ��,���ڳ��ֵ�����������ո��ַ�.

			if ( scr_end > video_mem_end ) 
			{

				// %0 - eax( �����ַ�+���� );%1 - ecx( ( ��ʾ���ַ�����-1 )����Ӧ���ַ���/2,���Գ����ƶ� );
				// %2 - edi( ��ʾ�ڴ���ʼλ��video_mem_start );%3 - esi( ��Ļ���ݶ�Ӧ���ڴ���ʼλ��origin ).
				// �ƶ�����:[ edi ]->[ esi ],�ƶ�ecx ������

				c = ( video_num_lines - 1 ) * video_num_columns >> 1;

				__asm	mov		ax, video_erase_char;
				__asm	mov		ecx, c
				__asm	mov		edi, video_mem_start
				__asm	mov		esi, origin
				__asm	cld
				__asm	rep		movsd					// �ظ�����,����ǰ��Ļ�ڴ������ƶ�����ʾ�ڴ���ʼ��
				__asm	mov		ecx, video_num_columns	// ecx=1 ���ַ���
				__asm	rep		stosw					// ������������ո��ַ�

				// ������Ļ�ڴ������ƶ�������,���µ�����ǰ��Ļ��Ӧ�ڴ����ʼָ�롢���λ��ָ�����Ļĩ��
				// ��Ӧ�ڴ�ָ��scr_end
				scr_end -= origin - video_mem_start;
				pos		-= origin - video_mem_start;
				origin   = video_mem_start;
			}
			else 
			{

				// ������������Ļĩ�˶�Ӧ���ڴ�ָ��scr_end û�г�����ʾ�ڴ��ĩ��video_mem_end,��ֻ����
				// ��������������ַ�( �ո��ַ� ).
				// %0 - eax( �����ַ�+���� );%1 - ecx( ��ʾ���ַ����� );%2 - edi( ��Ļ��Ӧ�ڴ����һ�п�ʼ�� );

				D = scr_end - video_size_row;

				__asm	mov		ax, video_erase_char
				__asm	mov		ecx, video_num_columns
				__asm	mov		edi, D
				__asm	cld
				__asm	rep		stosw
			}
			// ����ʾ��������д���µ���Ļ���ݶ�Ӧ���ڴ���ʼλ��ֵ
			set_origin();
		}
		else 
		{

			// �����ʾ���������ƶ�.Ҳ����ʾ��ָ����top ��ʼ�������������ƶ�1 ��( ɾ��1 �� ).��ʱֱ��
			// ����Ļ��ָ����top ����Ļĩ�������ж�Ӧ����ʾ�ڴ����������ƶ�1 ��,�����³��ֵ����������
			// ���ַ�.
			// %0-eax( �����ַ�+���� );%1-ecx( top ����1 �п�ʼ����Ļĩ�е���������Ӧ���ڴ泤���� );
			// %2-edi( top ���������ڴ�λ�� );%3-esi( top+1 ���������ڴ�λ�� )
			c = ( bottom - top - 1 ) * video_num_columns >> 1;
			D = origin + video_size_row * top;
			S = origin + video_size_row * ( top + 1 );

			__asm	mov		ax, video_erase_char
			__asm	mov		ecx, c
			__asm	mov		edi, D
			__asm	mov		esi, S
			__asm	cld
			__asm	rep		movsd
			__asm	mov		ecx, video_num_columns
			__asm	rep		stosw
		}
	}
	else		/* Not EGA/VGA */
	{
		// �����ʾ���Ͳ���EGA( ��MDA ),��ִ�������ƶ�����.��ΪMDA ��ʾ���ƿ����Զ�����������ʾ��Χ
		// �����,Ҳ�����Զ�����ָ��,�������ﲻ����Ļ���ݶ�Ӧ�ڴ泬����ʾ�ڴ�������������.����
		// ������EGA �������ƶ������ȫһ��.
		c = ( bottom - top - 1 ) * video_num_columns >> 1;
		D = origin + video_size_row * top;
		S = origin + video_size_row * ( top + 1 );

		__asm	mov		ax, video_erase_char
		__asm	mov		ecx, c
		__asm	mov		edi, D
		__asm	mov		esi, S
		__asm	cld
		__asm	rep		movsd
		__asm	mov		ecx, video_num_columns
		__asm	rep		stosw
	}
}


// ���¾�һ��( ��Ļ���������ƶ� ).
// ����Ļ���������ƶ�һ��,��Ļ��ʾ�����������ƶ�1 ��,�ڱ��ƶ���ʼ�е��Ϸ�����һ����.�μ�
// �����б��˵��.��������scrup()����,ֻ��Ϊ�����ƶ���ʾ�ڴ�����ʱ���������ݸ��Ǵ�����
// ��,�������Է�������е�,Ҳ������Ļ������2 �е����һ���ַ���ʼ����
static VOID scrdown()
{
	ULONG c, D, S;

	// �����ʾ������EGA,��ִ�����в���.
	// [ ??����if ��else �Ĳ�����ȫһ����!Ϊʲô��Ҫ�ֱ�����?�ѵ��������л��й�? ]
	if ( video_type == VIDEO_TYPE_EGAC || video_type == VIDEO_TYPE_EGAM )
	{
		// %0-eax( �����ַ�+���� );%1-ecx( top �п�ʼ����Ļĩ��-1 �е���������Ӧ���ڴ泤���� );
		// %2-edi( ��Ļ���½����һ������λ�� );%3-esi( ��Ļ������2 �����һ������λ�� ).
		// �ƶ�����:[ esi ]??[ edi ],�ƶ�ecx ������.
		c = ( bottom - top - 1 ) * video_num_columns >> 1;
		D = origin + video_size_row * bottom - 4;
		S = origin + video_size_row * ( bottom - 1 ) - 4;

		__asm	mov		ax, video_erase_char
		__asm	mov		ecx, c
		__asm	mov		edi, D
		__asm	mov		esi, S
		__asm	std
		__asm	rep		movsd
		__asm	add		edi, 2	/* %edi has been decremented by 4 */
		__asm	mov		ecx, video_num_columns
		__asm	rep		stosw
	}
	else		/* Not EGA/VGA */ // �������EGA ��ʾ����,��ִ�����²���( Ŀǰ��������ȫһ�� )
	{
		c = ( bottom - top - 1 ) * video_num_columns >> 1;
		D = origin + video_size_row * bottom - 4;
		S = origin + video_size_row * ( bottom - 1 ) - 4;

		__asm	mov		ax, video_erase_char
		__asm	mov		ecx, c
		__asm	mov		edi, D
		__asm	mov		esi, S
		__asm	std
		__asm	rep		movsd
		__asm	add		edi, 2	/* %edi has been decremented by 4 */
		__asm	rep		stosw
	}
}

// ���λ������һ��( lf - line feed ���� )
static VOID lf()
{
	// ������û�д��ڵ�����2 ��֮��,��ֱ���޸Ĺ�굱ǰ�б���y++,����������Ӧ��ʾ�ڴ�λ��
	// pos( ������Ļһ���ַ�����Ӧ���ڴ泤�� )

	if ( y + 1 < bottom )
	{
		y++;
		pos += video_size_row;
		return;
	}
	// ������Ҫ����Ļ��������һ��
	scrup();
}

// �������һ��( ri - reverse line feed ������ )
static VOID ri()
{
	// �����겻�ڵ�1 ����,��ֱ���޸Ĺ�굱ǰ�б���y--,����������Ӧ��ʾ�ڴ�λ��pos,��ȥ
	// ��Ļ��һ���ַ�����Ӧ���ڴ泤���ֽ���
	if ( y > top )
	{
		y--;
		pos -= video_size_row;
		return;
	}
	// ������Ҫ����Ļ��������һ��
	scrdown();
}

// ���ص���1 ��( 0 �� )���( cr - carriage return �س� )
static VOID cr()
{
	// ������ڵ��к�*2 ��0 �е���������ж�Ӧ���ڴ��ֽڳ���
	pos -= x << 1;
	x = 0;
}

// �������ǰһ�ַ�( �ÿո���� )( del - delete ɾ�� )
static VOID del()
{
	// ������û�д���0 ��,�򽫹���Ӧ�ڴ�λ��ָ��pos ����2 �ֽ�( ��Ӧ��Ļ��һ���ַ� ),Ȼ��
	// ����ǰ��������ֵ��1,�����������λ���ַ�����
	if ( x ) 
	{
		pos -= 2;
		x--;
		*( USHORT * )pos = video_erase_char;
	}
}

// ɾ����Ļ������λ����صĲ���,����ĻΪ��λ.csi - ��������������( Control Sequence
// Introducer ).
// ANSI ת������:'ESC [ sJ'( s = 0 ɾ����굽��Ļ�׶�;1 ɾ����Ļ��ʼ����괦;2 ����ɾ�� ).
// ����:par - ��Ӧ����s
static VOID csi_J( LONG par )
{
	LONG count;
	LONG start;

	// ���ȸ�����������ֱ�������Ҫɾ�����ַ�����ɾ����ʼ����ʾ�ڴ�λ��
	switch ( par )
	{
	case 0:	/* erase from cursor to end of display */ /* ������굽��Ļ�׶� */

		count = ( scr_end - pos ) >> 1;
		start = pos;
		break;

	case 1:	/* erase from start to cursor */		/* ɾ������Ļ��ʼ����괦���ַ� */

		count = ( pos - origin ) >> 1;
		start = origin;
		break;

	case 2: /* erase whole display */				/* ɾ��������Ļ�ϵ��ַ� */

		count = video_num_columns * video_num_lines;
		start = origin;
		break;

	default:
		return;
	}

	// Ȼ��ʹ�ò����ַ���дɾ���ַ��ĵط�.
	// %0 - ecx( Ҫɾ�����ַ���count );%1 - edi( ɾ��������ʼ��ַ );%2 - eax( ����Ĳ����ַ� ).

	__asm	mov		ecx, count
	__asm	mov		edi, start
	__asm	mov		ax, video_erase_char
	__asm	cld
	__asm	rep		stosw
}

// ɾ����������λ����صĲ���,��һ��Ϊ��λ.
// ANSI ת���ַ�����:'ESC [ sK'( s = 0 ɾ������β;1 �ӿ�ʼɾ��;2 ���ж�ɾ�� )

static VOID csi_K( LONG par )
{
	LONG count;
	LONG start;

	// ���ȸ�����������ֱ�������Ҫɾ�����ַ�����ɾ����ʼ����ʾ�ڴ�λ��
	switch ( par )
	{
	case 0:	/* erase from cursor to end of line */

		if ( x >= video_num_columns )
		{
			return;
		}
		count = video_num_columns - x;
		start = pos;
		break;

	case 1:	/* erase from start of line to cursor */

		start = pos - ( x << 1 );
		count = ( x < video_num_columns ) ? x : video_num_columns;
		break;

	case 2: /* erase whole line */

		start = pos - ( x << 1 );
		count = video_num_columns;
		break;

	default:
		return;
	}
	__asm	mov		ecx, count
	__asm	mov		edi, start
	__asm	mov		ax, video_erase_char
	__asm	cld
	__asm	rep		stosw
}

// ������( ���� )( �������������ַ���ʾ��ʽ,����Ӵ֡����»��ߡ���˸�����Ե� ).
// ANSI ת���ַ�����:'ESC [ nm'.n = 0 ������ʾ;1 �Ӵ�;4 ���»���;7 ����;27 ������ʾ.

VOID csi_m()
{
	ULONG i;

	for ( i = 0; i <= npar; i++ )
	{
		switch ( par[ i ] ) 
		{
		case 0:
			attr = 0x07; 
			break;
		case 1:
			attr = 0x0f; 
			break;
		case 4:
			attr = 0x0f; 
			break;
		case 7:
			attr = 0x70; 
			break;
		case 27:
			attr = 0x07; 
			break;
		}
	}
}

// ����������ʾ���.
// ������ʾ�ڴ����Ӧλ��pos,������ʾ������������ʾλ��
static __inline VOID set_cursor()
{
	cli();

	// ����ʹ�������Ĵ����˿�ѡ����ʾ�������ݼĴ���r14( ��굱ǰ��ʾλ�ø��ֽ� ),Ȼ��д����
	// ��ǰλ�ø��ֽ�( �����ƶ�9 λ��ʾ���ֽ��Ƶ����ֽ��ٳ���2 ).�������Ĭ����ʾ�ڴ������.

	outb_p( 14, video_port_reg );
	outb_p( 0xff & ( ( pos - video_mem_start ) >> 9 ), video_port_val );

	// ��ʹ�������Ĵ���ѡ��r15,������굱ǰλ�õ��ֽ�д������

	outb_p( 15, video_port_reg );
	outb_p( 0xff & ( ( pos - video_mem_start ) >> 1 ), video_port_val );

	sti();
}

// ���Ͷ��ն�VT100 ����Ӧ����.
// ����Ӧ���з�������������
static VOID respond( struct tty_struct * tty )
{
	CHAR * p = RESPONSE;

	cli();

	while ( *p )
	{
		PUTCH( *p, tty->read_q );
		p++;
	}

	sti();

	copy_to_cooked( tty );	// ת���ɹ淶ģʽ( ���븨�������� )
}

// �ڹ�괦����һ�ո��ַ�
static VOID insert_char()
{
	ULONG		i = x;
	USHORT		tmp, old = video_erase_char;
	USHORT *	p = ( USHORT * )pos;

	// ��꿪ʼ�������ַ�����һ��,���������ַ������ڹ�����ڴ�.
	// ��һ���϶����ַ��Ļ�,�������һ���ַ����������??
	while ( i++ < video_num_columns )
	{
		tmp = *p;
		*p  = old;
		old = tmp;
		p++;
	}
}

// �ڹ�괦����һ��( ���꽫�����µĿ����� ).
// ����Ļ�ӹ�������е���Ļ�����¾�һ��

static VOID insert_line()
{
	LONG oldtop, oldbottom;

	oldtop		= top;
	oldbottom	= bottom;
	top			= y;
	bottom		= video_num_lines;

	scrdown();

	top			= oldtop;
	bottom		= oldbottom;
}

// ɾ����괦��һ���ַ�
static VOID delete_char()
{
	ULONG		i;
	USHORT *	p = ( USHORT * )pos;

	// �����곬����Ļ������,�򷵻�
	if ( x >= video_num_columns )
	{
		return;
	}

	// �ӹ����һ���ַ���ʼ����ĩ�����ַ�����һ��
	i = x;

	while ( ++i < video_num_columns ) 
	{
		*p = *( p + 1 );
		p++;
	}
	// ���һ���ַ�����������ַ�( �ո��ַ� ).
	*p = video_erase_char;
}

// ɾ�����������.
// �ӹ�������п�ʼ��Ļ�����Ͼ�һ��
static VOID delete_line()
{
	LONG oldtop, oldbottom;

	oldtop		= top;				// ����ԭtop,bottom ֵ
	oldbottom	= bottom;
	top			= y;				// ������Ļ����ʼ��
	bottom		= video_num_lines;	// ������Ļ�������
	scrup();						// �ӹ�꿪ʼ��,��Ļ�������Ϲ���һ��
	top			= oldtop;			// �ָ�ԭtop,bottom ֵ
	bottom		= oldbottom;
}

// �ڹ�괦����nr ���ַ�.
// ANSI ת���ַ�����:'ESC [ n@ '.
// ���� nr = ����n
static VOID csi_at( ULONG nr )
{
	// ���������ַ�������һ���ַ���,���Ϊһ���ַ���;�������ַ���nr Ϊ0,�����1 ���ַ�.

	if ( nr > video_num_columns )
	{
		nr = video_num_columns;
	}
	else if ( !nr )
	{
		nr = 1;
	}
	// ѭ������ָ�����ַ���
	while ( nr-- )
	{
		insert_char();
	}
}

// �ڹ��λ�ô�����nr ��.
// ANSI ת���ַ�����'ESC [ nL'
static VOID csi_L( ULONG nr )
{
	// ������������������Ļ�������,���Ϊ��Ļ��ʾ����;����������nr Ϊ0,�����1 ��.

	if ( nr > video_num_lines )
	{
		nr = video_num_lines;
	}
	else if ( !nr )
	{
		nr = 1;
	}

	// ѭ������ָ������nr
	while ( nr-- )
	{
		insert_line();
	}
}

// ɾ����괦�� nr ���ַ�.
// ANSI ת������:'ESC [ nP'
static VOID csi_P( ULONG nr )
{
	if ( nr > video_num_columns )
	{
		nr = video_num_columns;
	}
	else if ( !nr )
	{
		nr = 1;
	}
	while ( nr-- )
	{
		delete_char();
	}
}

// ɾ����괦��nr ��.
// ANSI ת������:'ESC [ nM'
static VOID csi_M( ULONG nr )
{
	// ���ɾ��������������Ļ�������,���Ϊ��Ļ��ʾ����;��ɾ��������nr Ϊ0,��ɾ��1 ��.

	if ( nr > video_num_lines )
	{
		nr = video_num_lines;
	}
	else if ( !nr )
	{
		nr = 1;
	}
	// ѭ��ɾ��ָ������nr
	while ( nr-- )
	{
		delete_line();
	}
}

static LONG saved_x = 0;	// ����Ĺ���к�
static LONG saved_y = 0;	// ����Ĺ���к�

// ���浱ǰ���λ��
static VOID save_cur()
{
	saved_x = x;
	saved_y = y;
}

// �ָ�����Ĺ��λ��
static VOID restore_cur()
{
	gotoxy( saved_x, saved_y );
}

// ����̨д����.
// ���ն˶�Ӧ��tty д���������ȡ�ַ�,����ʾ����Ļ��
VOID con_write( struct tty_struct * tty )
{
	LONG	nr;
	CHAR	c;

	// ����ȡ��д��������������ַ���nr,Ȼ�����ÿ���ַ����д���
	nr = CHARS( tty->write_q );

	while ( nr-- )
	{
		// ��д������ȡһ�ַ�c,����ǰ���������ַ���״̬state �ֱ���.״̬֮���ת����ϵΪ:
		// state = 0:��ʼ״̬;����ԭ��״̬4;����ԭ��״̬1,���ַ�����'[ ';
		// 1:ԭ��״̬0,�����ַ���ת���ַ�ESC( 0x1b = 033 = 27 );
		// 2:ԭ��״̬1,�����ַ���'[ ';
		// 3:ԭ��״̬2;����ԭ��״̬3,�����ַ���';'������.
		// 4:ԭ��״̬3,�����ַ�����';'������;
		GETCH( tty->write_q, c );

		switch ( state ) 
		{
		case 0:
			// ����ַ����ǿ����ַ�( c>31 ),����Ҳ������չ�ַ�( c<127 ),��
			if ( c > 31 && c < 127 ) 
			{
				// ����ǰ��괦����ĩ�˻�ĩ������,�򽫹���Ƶ�����ͷ��.���������λ�ö�Ӧ���ڴ�ָ��pos.

				if ( x >= video_num_columns ) 
				{
					x -= video_num_columns;
					pos -= video_size_row;
					lf();
				}
				// ���ַ�c д����ʾ�ڴ���pos ��,�����������1 ��,ͬʱҲ��pos ��Ӧ���ƶ�2 ���ֽ�
				*( short* )pos = ( attr << 8 ) + c;
				pos += 2;
				x++;
			}
			else if ( c == 27 )	// ����ַ� c ��ת���ַ� ESC,��ת��״̬ state �� 1
			{
				state = 1;
			}
			else if ( c == 10 || c == 11 || c == 12 )	// ����ַ�c �ǻ��з�( 10 ),���Ǵ�ֱ�Ʊ��VT( 11 ),�����ǻ�ҳ��FF( 12 ),���ƶ���굽��һ��.
			{
				lf();
			}
			else if ( c == 13 )	// ����ַ�c �ǻس���CR( 13 ),�򽫹���ƶ���ͷ��( 0 �� )
			{
				cr();
			}
			else if ( c == ERASE_CHAR( tty ) )	// ����ַ�c ��DEL( 127 ),�򽫹���ұ�һ�ַ�����( �ÿո��ַ���� ),��������Ƶ�������λ��
			{
				del();
			}
			else if ( c == 8 ) 		// ����ַ�c ��BS( backspace,8 ),�򽫹������1 ��,����Ӧ��������Ӧ�ڴ�λ��ָ��pos
			{
				if ( x ) 
				{
					x--;
					pos -= 2;
				}
			}
			else if ( c == 9 ) 
			{
				// ����ַ�c ��ˮƽ�Ʊ��TAB( 9 ),�򽫹���Ƶ�8 �ı�������.����ʱ�������������Ļ�������,
				// �򽫹���Ƶ���һ����

				c	 = 8 - ( x & 7 );
				x	+= c;
				pos += c << 1;

				if ( x > video_num_columns )
				{
					x   -= video_num_columns;
					pos -= video_size_row;

					lf();
				}
				c = 9;
			}
			else if ( c == 7 )	// ����ַ�c �������BEL( 7 ),����÷�������,������������
			{
				sysbeep();
			}
			break;

		case 1:

			state = 0;

			if ( c == '[ ' )
			{
				state = 2;
			}
			else if ( c == 'E' )
			{
				gotoxy( 0, y + 1 );
			}
			else if ( c == 'M' )
			{	
				ri();
			}
			else if ( c == 'D' )
			{
				lf();
			}
			else if ( c == 'Z' )
			{
				respond( tty );
			}
			else if ( x == '7' )
			{
				save_cur();
			}
			else if ( x == '8' )
			{
				restore_cur();
			}
			break;

		case 2:

			for ( npar = 0; npar < NPAR; npar++ )
			{
				par[ npar ] = 0;
			}

			npar  = 0;
			state = 3;

			if ( ques = ( c == '?' ) )
			{
				break;
			}

		case 3:

			if ( c == ';' && npar < NPAR - 1 ) 
			{
				npar++;
				break;
			}
			else if ( c >= '0' && c <= '9' ) 
			{
				par[ npar ] = 10 * par[ npar ] + c - '0';
				break;
			}
			else 
			{
				state = 4;
			}

		case 4:

			state = 0;

			switch ( c )
			{
			case 'G': 
			case '`':

				if ( par[ 0 ] ) 
				{
					par[ 0 ]--;
				}

				gotoxy( par[ 0 ], y );
				break;

			case 'A':

				if ( !par[ 0 ] )
				{
					par[ 0 ]++;
				}

				gotoxy( x, y - par[ 0 ] );
				break;

			case 'B':
			case 'e':

				if ( !par[ 0 ] ) 
				{
					par[ 0 ]++;
				}
				gotoxy( x, y + par[ 0 ] );
				break;

			case 'C':
			case 'a':

				if ( !par[ 0 ] ) 
				{
					par[ 0 ]++;
				}
				gotoxy( x + par[ 0 ], y );
				break;

			case 'D':

				if ( !par[ 0 ] )
				{
					par[ 0 ]++;
				}
				gotoxy( x - par[ 0 ], y );
				break;

			case 'E':

				if ( !par[ 0 ] ) 
				{
					par[ 0 ]++;
				}
				gotoxy( 0, y + par[ 0 ] );
				break;

			case 'F':

				if ( !par[ 0 ] ) 
				{
					par[ 0 ]++;
				}
				gotoxy( 0, y - par[ 0 ] );
				break;

			case 'd':

				if ( par[ 0 ] ) 
				{
					par[ 0 ]--;
				}
				gotoxy( x, par[ 0 ] );
				break;

			case 'H':
			case 'f':

				if ( par[ 0 ] )
				{
					par[ 0 ]--;
				}
				if ( par[ 1 ] ) 
				{
					par[ 1 ]--;
				}

				gotoxy( par[ 1 ], par[ 0 ] );
				break;

			case 'J':

				csi_J( par[ 0 ] );
				break;

			case 'K':

				csi_K( par[ 0 ] );
				break;

			case 'L':

				csi_L( par[ 0 ] );
				break;

			case 'M':

				csi_M( par[ 0 ] );
				break;

			case 'P':

				csi_P( par[ 0 ] );
				break;

			case '@':

				csi_at( par[ 0 ] );
				break;

			case 'm':

				csi_m();
				break;

			case 'r':

				if ( par[ 0 ] ) 
				{
					par[ 0 ]--;
				}
				if ( !par[ 1 ] ) 
				{
					par[ 1 ] = video_num_lines;
				}

				if ( par[ 0 ] < par[ 1 ] && par[ 1 ] <= video_num_lines ) 
				{	
					top		= par[ 0 ];
					bottom	= par[ 1 ];
				}
				break;

			case 's':

				save_cur();
				break;

			case 'u':

				restore_cur();
				break;
			}
		}
	}
	set_cursor();
}

/*
 *  VOID con_init();
 *
 * This routine initalizes console interrupts, and does nothing
 * else. If you want the screen to clear, call tty_write with
 * the appropriate escape-sequece.
 *
 * Reads the information preserved by setup.s to determine the current display
 * type and sets everything accordingly.
 */

/*
 * VOID con_init();
 * ����ӳ����ʼ������̨�ж�,����ʲô������.�����������Ļ�ɾ��Ļ�,��ʹ��
 * �ʵ���ת���ַ����е���tty_write()����.
 *
 * ��ȡ setup.s ���򱣴����Ϣ,����ȷ����ǰ��ʾ������,��������������ز���.
 */

VOID con_init()
{
	register UCHAR	a;
	CHAR *			display_desc = "????";
	CHAR *			display_ptr;

	video_num_columns	= ORIG_VIDEO_COLS;				// ��ʾ����ʾ�ַ�����.
	video_size_row		= video_num_columns * 2;		// ÿ����ʹ���ֽ���.
	video_num_lines		= ORIG_VIDEO_LINES;				// ��ʾ����ʾ�ַ�����.
	video_page			= ( UCHAR )ORIG_VIDEO_PAGE;		// ��ǰ��ʾҳ��.
	video_erase_char	= 0x0720;						// �����ַ�( 0x20 ��ʾ�ַ�, 0x07 ������ ).

	// ���ԭʼ��ʾģʽ����7,���ʾ�ǵ�ɫ��ʾ��
	if ( ORIG_VIDEO_MODE == 7 )			/* Is this a monochrome display? */
	{
		video_mem_start = 0xb0000;		// ���õ���ӳ���ڴ���ʼ��ַ.
		video_port_reg	= 0x3b4;		// ���õ��������Ĵ����˿�.
		video_port_val	= 0x3b5;		// ���õ������ݼĴ����˿�.

		// ����BIOS �ж�LONG 0x10 ����0x12 ��õ���ʾģʽ��Ϣ,�ж���ʾ����ɫ��ʾ�����ǲ�ɫ��ʾ��.
		// ���ʹ�������жϹ������õ���BX �Ĵ�������ֵ������0x10,��˵����EGA ��.��˳�ʼ
		// ��ʾ����ΪEGA ��ɫ;��ʹ��ӳ���ڴ�ĩ�˵�ַΪ0xb8000;������ʾ�������ַ���Ϊ'EGAm'.
		// ��ϵͳ��ʼ���ڼ���ʾ�������ַ�������ʾ����Ļ�����Ͻ�
		if ( ( ORIG_VIDEO_EGA_BX & 0xff ) != 0x10 )
		{
			video_type		= VIDEO_TYPE_EGAM;	// ������ʾ����( EGA ��ɫ ).
			video_mem_end	= 0xb8000;			// ������ʾ�ڴ�ĩ�˵�ַ.
			display_desc	= "EGAm";			// ������ʾ�����ַ���.
		}
		else
		{
			video_type		= VIDEO_TYPE_MDA;	// ������ʾ����( MDA ��ɫ )
			video_mem_end	= 0xb2000;			// ������ʾ�ڴ�ĩ�˵�ַ.
			display_desc	= "*MDA";			// ������ʾ�����ַ���.
		}
	}
	else										/* If not, it is color. */
	{
		// �����ʾģʽ��Ϊ7,��Ϊ��ɫģʽ.��ʱ���õ���ʾ�ڴ���ʼ��ַΪ0xb800;��ʾ���������Ĵ�
		// ���˿ڵ�ַΪ0x3d4;���ݼĴ����˿ڵ�ַΪ0x3d5
		video_mem_start		= 0xb8000;			// ��ʾ�ڴ���ʼ��ַ
		video_port_reg		= 0x3d4;			// ���ò�ɫ��ʾ�����Ĵ����˿�
		video_port_val		= 0x3d5;			// ���ò�ɫ��ʾ���ݼĴ����˿�

		// ���ж���ʾ�����.��� BX ������0x10,��˵���� EGA ��ʾ��
		if ( ( ORIG_VIDEO_EGA_BX & 0xff ) != 0x10 )
		{
			video_type		= VIDEO_TYPE_EGAC;	// ������ʾ����( EGA ��ɫ ).
			video_mem_end	= 0xbc000;			// ������ʾ�ڴ�ĩ�˵�ַ.
			display_desc	= "EGAc";			// ������ʾ�����ַ���.
		}
		else
		{
			// ���BX �Ĵ�����ֵ����0x10,��˵����CGA ��ʾ��.��������Ӧ����
			video_type		= VIDEO_TYPE_CGA;	// ������ʾ����( CGA ).
			video_mem_end	= 0xba000;			// ������ʾ�ڴ�ĩ�˵�ַ.
			display_desc	= "*CGA";			// ������ʾ�����ַ���.
		}
	}

	/* Let the user known what kind of display driver we are using */
	/* ���û�֪����������ʹ����һ����ʾ�������� */

	// ����Ļ�����Ͻ���ʾ��ʾ�����ַ���.���õķ�����ֱ�ӽ��ַ���д����ʾ�ڴ����Ӧλ�ô�.
	// ���Ƚ���ʾָ�� display_ptr ָ����Ļ��һ���Ҷ˲� 4 ���ַ���( ÿ���ַ��� 2 ���ֽ�,��˼� 8 ).

	display_ptr = ( ( CHAR * )video_mem_start ) + video_size_row - 8;

	// Ȼ��ѭ�������ַ����е��ַ�,����ÿ����һ���ַ����տ�һ�������ֽ�
	while ( *display_desc )
	{
		*display_ptr++ = *display_desc++;
		display_ptr++;		//�����ֽ�
	}

	/* Initialize the variables used for scrolling ( mostly EGA/VGA )	*/
	// ��ʼ�����ڹ����ı���( ��Ҫ����EGA/VGA )
	origin = video_mem_start;
	scr_end = video_mem_start + video_num_lines * video_size_row;
	top = 0;
	bottom = video_num_lines;

	gotoxy( ORIG_X, ORIG_Y );					// ��ʼ�����λ��x,y �Ͷ�Ӧ���ڴ�λ��pos
	set_trap_gate( 0x21, &keyboard_interrupt );	// ���ü����ж�������
	outb_p( inb_p( 0x21 ) & 0xfd, 0x21 );		// ȡ��8259A �жԼ����жϵ�����,����IRQ1
	a = inb_p( 0x61 );							// �ӳٶ�ȡ���̶˿�0x61( 8255A �˿�PB )
	outb_p( a | 0x80, 0x61 );					// ���ý�ֹ���̹���( λ7 ��λ )
	outb( a, 0x61 );							// ��������̹���,���Ը�λ���̲���
}
/* from bsd-net-2: */

// ֹͣ����.
// ��λ8255A PB �˿ڵ�λ1 ��λ0
VOID sysbeepstop()
{
	/* disable counter 2 */ /* ��ֹ��ʱ��2 */
	outb( inb_p( 0x61 ) & 0xFC, 0x61 );
}

LONG beepcount = 0;

// ��ͨ����.
// 8255A оƬPB �˿ڵ�λ1 �����������Ŀ����ź�;λ0 ����8253 ��ʱ��2 �����ź�,�ö�ʱ����
// �����������������,��Ϊ������������Ƶ��.���Ҫʹ����������,��Ҫ����:���ȿ���PB �˿�
// λ1 ��λ0( ��λ ),Ȼ�����ö�ʱ������һ���Ķ�ʱƵ�ʼ���
static VOID sysbeep()
{
	/* enable counter 2 */		/* ������ʱ��2 */
	outb_p( inb_p( 0x61 ) | 3, 0x61 );
	/* set command for counter 2, 2 byte write */	/* �����ö�ʱ��2 ���� */
	outb_p( 0xB6, 0x43 );
	/* send 0x637 for 750 HZ */	/* ����Ƶ��Ϊ750HZ,����Ͷ�ʱֵ0x637 */
	outb_p( 0x37, 0x42 );
	outb( 0x06, 0x42 );
	/* 1/8 second */			/* ����ʱ��Ϊ1/8 �� */
	beepcount = HZ / 8;
}
