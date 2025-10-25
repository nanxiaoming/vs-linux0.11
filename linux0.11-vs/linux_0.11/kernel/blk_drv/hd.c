/*
*  linux/kernel/hd.c
*
*  ( C ) 1991  Linus Torvalds
*/

/*
* This is the low-level hd interrupt support. It traverses the
* request-list, using interrupts to jump between functions. As
* all the functions are called within interrupts, we may not
* sleep. Special care is recommended.
*
*  modified by Drew Eckhardt to check nr of hd's from the CMOS.
*/

#include <linux\config.h>
#include <linux\sched.h>
#include <linux\fs.h>
#include <linux\kernel.h>
#include <linux\hdreg.h>
#include <asm\system.h>
#include <asm\io.h>
#include <asm\segment.h>

#define MAJOR_NR 3	// Ӳ�����豸����3
#include "blk.h"

/* Max read/write errors/sector */

#define MAX_ERRORS	7			// ��/дһ������ʱ��������������
#define MAX_HD		2			// ϵͳ֧�ֵ����Ӳ����

static VOID recal_intr();		// Ӳ���жϳ����ڸ�λ����ʱ����õ�����У������

static LONG hd_recalibrate	= 1;	// ����У����־.
static LONG hd_reset		= 1;	// ��λ��־.

__inline UCHAR Hd_CMOS_Read( USHORT addr )
{
	UCHAR _v;

	outb_p( 0x80 | addr, 0x70 );

	_v = inb_p( 0x71 );

	return _v;
}

/*
 * This struct defines the HD's and their types.
 * 
 * ����ṹ������Ӳ�̲���������
 * ���ֶηֱ��Ǵ�ͷ����ÿ�ŵ�����������������дǰԤ��������š���ͷ��½������š������ֽ�.
 * 
 */

struct hd_i_struct
{
	LONG head, sect, cyl, wpcom, lzone, ctl;
};

#ifdef HD_TYPE	// ����Ѿ���include/linux/config.h �ж�����HD_TYPE

struct hd_i_struct hd_info[] = { HD_TYPE };	// ȡ����õĲ�����Ϊhd_info[]������
#define NR_HD ( ( sizeof ( hd_info ) )/( sizeof ( struct hd_i_struct ) ) )
#else
struct hd_i_struct hd_info[] = { { 0, 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0, 0 } };
static LONG NR_HD = 0;
#endif

// ����Ӳ�̷����ṹ.����ÿ��������������ʼ�����š�������������.
// ���� 5 �ı���������( ����hd[ 0 ]��hd[ 5 ]�� )��������Ӳ���еĲ���
static struct hd_struct 
{
	LONG start_sect;
	LONG nr_sects;
} hd[ 5 * MAX_HD ] = { { 0, 0 }, };

// ���˿� port,���� nr ��,������ buf ��
static __inline VOID port_read( USHORT port, VOID *buf, LONG nr )
{
	__asm	mov		dx, port
	__asm	mov		edi, buf
	__asm	mov		ecx, nr
	__asm	cld
	__asm	rep		insw
}

// д�˿� port,��д nr ��,�� buf ��ȡ����
static __inline VOID port_write( USHORT port, VOID *buf, LONG nr )
{
	__asm	mov		dx, port
	__asm	mov		esi, buf
	__asm	mov		ecx, nr
	__asm	cld
	__asm	rep		outsw
}

extern VOID hd_interrupt();
extern VOID rd_load();

LONG sys_setup( CHAR * BIOS )
/*++

Routine Description:

	�ú����Ĳ����ɳ�ʼ������ init/main.c �� init �ӳ�������Ϊָ��0x90080 ��,�˴������
	setup.s ����� BIOS ȡ�õ� 2 ��Ӳ�̵Ļ���������( 32 �ֽ� ).
	
	Ӳ�̲�������Ϣ�μ������б���˵��.
	��������Ҫ�����Ƕ�ȡ CMOS ��Ӳ�̲�������Ϣ,��������Ӳ�̷����ṹ hd ,������ RAM �����̺͸��ļ�ϵͳ
	
Arguments:

	BIOS - bios ����

Return Value:

	0 - �ɹ�

--*/
{
	static LONG				callable = 1;
	LONG					i, drive;
	UCHAR					cmos_disks;
	struct partition	*	p;
	Buffer_Head			*	bh;

	// ��ʼ��ʱcallable=1,�����иú���ʱ��������Ϊ0,ʹ������ֻ��ִ��һ��
	if ( !callable )
	{
		return -1;
	}

	callable = 0;

	// ���û����config.h �ж���Ӳ�̲���,�ʹ�0x90080������
#ifndef HD_TYPE

	for ( drive = 0; drive < 2; drive++ ) 
	{
		hd_info[ drive ].cyl	= *( USHORT * )BIOS;			// ������.
		hd_info[ drive ].head	= *( UCHAR	* )( 2  + BIOS );	// ��ͷ��.
		hd_info[ drive ].wpcom	= *( USHORT * )( 5  + BIOS );	// дǰԤ���������.
		hd_info[ drive ].ctl	= *( UCHAR	* )( 8  + BIOS );	// �����ֽ�.
		hd_info[ drive ].lzone	= *( USHORT * )( 12 + BIOS );	// ��ͷ��½�������.
		hd_info[ drive ].sect	= *( UCHAR	* )( 14 + BIOS );	// ÿ�ŵ�������.
		BIOS += 16;												// ÿ��Ӳ�̵Ĳ�����16 �ֽ�,���� BIOS ָ����һ����.
	}

	// setup.s ������ȡ BIOS �е�Ӳ�̲�������Ϣʱ,���ֻ�� 1 ��Ӳ��,�ͻὫ��Ӧ�� 2 ��Ӳ�̵�
	// 16�ֽ�ȫ������.�������ֻҪ�жϵ� 2 ��Ӳ���������Ƿ�Ϊ 0 �Ϳ���֪����û�е� 2 ��Ӳ����.

	if ( hd_info[ 1 ].cyl )
	{
		NR_HD = 2;		// Ӳ������Ϊ2
	}
	else
	{
		NR_HD = 1;
	}
#endif

	// ����ÿ��Ӳ�̵���ʼ�����ź���������.���б�� i*5 ����μ����������й�˵��

	for ( i = 0; i < NR_HD; i++ ) 
	{
		hd[ i * 5 ].start_sect = 0;				// Ӳ����ʼ������
		hd[ i * 5 ].nr_sects	= hd_info[ i ].head * hd_info[ i ].sect * hd_info[ i ].cyl;
	}

	/*
	 * We querry CMOS about hard disks : it could be that
	 * we have a SCSI/ESDI/etc controller that is BIOS
	 * compatable with ST-506, and thus showing up in our
	 * BIOS table, but not register compatable, and therefore
	 * not present in CMOS.
	 * 
	 * Furthurmore, we will assume that our ST-506 drives
	 * <if any> are the primary drives in the system, and
	 * the ones reflected as drive 1 or 2.
	 * 
	 * The first drive is stored in the high nibble of CMOS
	 * byte 0x12, the second in the low nibble.  This will be
	 * either a 4 bit drive type or 0xf indicating use byte 0x19
	 * for an 8 bit type, drive 1, 0x1a for drive 2 in CMOS.
	 * 
	 * Needless to say, a non-zero value means we have
	 * an AT controller hard disk for that drive.
	 * 
	 */

	/*
	 * ���Ƕ� CMOS �й�Ӳ�̵���Ϣ��Щ����:���ܻ�������������,������һ��SCSI/ESDI/�ȵ�
	 * ������,������ ST-506 ��ʽ�� BIOS ���ݵ�,�������������ǵ� BIOS ��������,��ȴ�ֲ�
	 * �ǼĴ������ݵ�,�����Щ������ CMOS ���ֲ�����.
	 * 
	 * ����,���Ǽ��� ST-506 ������( ����еĻ� )��ϵͳ�еĻ���������,Ҳ���������� 1 �� 2 ���ֵ�������.
	 * 
	 * �� 1 ����������������� CMOS �ֽ� 0x12 �ĸ߰��ֽ���,�� 2 ������ڵͰ��ֽ���.�� 4 λ�ֽ�
	 * ��Ϣ����������������,Ҳ���ܽ���0xf.0xf ��ʾʹ�� CMOS �� 0x19 �ֽ���Ϊ������ 1 �� 8 λ
	 * �����ֽ�,ʹ�� CMOS �� 0x1A �ֽ���Ϊ������ 2 �������ֽ�.
	 * 
	 * ��֮,һ������ֵ��ζ��������һ�� AT ������Ӳ�̼��ݵ�������.
	 * 
	 */

	 // �����������ԭ�������Ӳ�̵����Ƿ��� AT ���������ݵ�.�й� CMOS ��Ϣ��μ�4.2.3.1 ��.
	if ( ( cmos_disks = Hd_CMOS_Read( 0x12 ) ) & 0xf0 )
	{
		if ( cmos_disks & 0x0f )
		{
			NR_HD = 2;
		}
		else
		{
			NR_HD = 1;
		}
	}
	else
	{
		NR_HD = 0;
	}

	// �� NR_HD = 0 , ������Ӳ�̶����� AT ���������ݵ�,Ӳ�����ݽṹ����.
	// �� NR_HD = 1 , �򽫵� 2 ��Ӳ�̵Ĳ�������.

	for ( i = NR_HD; i < 2; i++ ) 
	{
		hd[ i * 5 ].start_sect = 0;
		hd[ i * 5 ].nr_sects   = 0;
	}

	// ��ȡÿһ��Ӳ���ϵ�1 ������( ��1 ���������� ),��ȡ���еķ�������Ϣ.
	// �������ú���bread()��Ӳ�̵�1 ������( fs/buffer.c,267 ),�����е�0x300 ��Ӳ�̵����豸��
	// ( �μ��б���˵�� ).Ȼ�����Ӳ��ͷ1 ������λ��0x1fe ���������ֽ��Ƿ�Ϊ'55AA'���ж�
	// ��������λ��0x1BE ��ʼ�ķ������Ƿ���Ч.��󽫷�������Ϣ����Ӳ�̷������ݽṹhd ��.

	for ( drive = 0; drive < NR_HD; drive++ ) 
	{
		// 0x300, 0x305 �߼��豸��
		if ( !( bh = bread( 0x300 + drive * 5, 0 ) ) ) 
		{
			printk( "Unable to read partition table of drive %d\n\r",drive );
			panic( "" );
		}

		// �ж�Ӳ����Ϣ��Ч��־'55AA'
		if ( bh->b_data[ 510 ] != 0x55 || ( UCHAR )
			 bh->b_data[ 511 ] != 0xAA )
		{
			printk( "Bad partition table on drive %d\n\r", drive );
			panic( "" );
		}

		p = ( struct partition* )( 0x1BE + ( CHAR* )bh->b_data );// ������λ��Ӳ�̵�1 ������ 0x1BE ��

		for ( i = 1; i<5; i++, p++ ) 
		{
			hd[ i + 5 * drive ].start_sect	= p->start_sect;
			hd[ i + 5 * drive ].nr_sects	= p->nr_sects;
		}
		brelse( bh );		// �ͷ�Ϊ���Ӳ�̿��������ڴ滺����ҳ
	}

	if ( NR_HD )
	{
		printk( "Partition table%s ok.\n\r", ( NR_HD>1 ) ? "s" : "" );
	}

	rd_load();			// ����( ���� )RAMDISK( kernel/blk_drv/ramdisk.c )

	mount_root();		// ��װ���ļ�ϵͳ( fs/super.c ).

	return ( 0 );
}

// �жϲ�ѭ���ȴ�����������.
// ��Ӳ�̿�����״̬�Ĵ����˿�HD_STATUS( 0x1f7 ),��ѭ�������������������λ�Ϳ�����æλ.

static LONG controller_ready()
{
	LONG retries = 10000;

	while ( --retries && ( inb_p( HD_STATUS ) & 0xc0 ) != 0x40 );

	return ( retries );	// ���صȴ�ѭ���Ĵ���
}

// ���Ӳ��ִ��������״̬.( win_��ʾ����˹��Ӳ�̵���д )
// ��ȡ״̬�Ĵ����е�����ִ�н��״̬.����0 ��ʾ����,1 ����.���ִ�������,
// ���ٶ�����Ĵ���HD_ERROR( 0x1f1 )
static LONG win_result()
{
	LONG i = inb_p( HD_STATUS );	// ȡ״̬��Ϣ

	if ( ( i & ( BUSY_STAT | READY_STAT | WRERR_STAT | SEEK_STAT | ERR_STAT ) ) == ( READY_STAT | SEEK_STAT ) )
	{
		return 0; /* ok */
	}

	if ( i & 1 ) 
	{
		i = inb( HD_ERROR );	// ��ERR_STAT ��λ,���ȡ����Ĵ���
	}

	return 1;
}

// ��Ӳ�̿��������������( �μ��б���˵�� ).
// ���ò���:drive - Ӳ�̺�( 0-1 ); nsect - ��д������;
// sect - ��ʼ����; head - ��ͷ��;
// cyl - �����; cmd - ������;
// *intr_addr() - Ӳ���жϴ�������н����õ�C ������
static VOID hd_out( 
	ULONG			drive, 
	ULONG			nsect, 
	ULONG			sect,
	ULONG			head, 
	ULONG			cyl, 
	ULONG			cmd,
	VOID( *intr_addr )() )
{
	register LONG port;

	if ( drive > 1 || head > 15 )	// �����������( 0,1 )>1 ���ͷ��>15,�����֧��
	{
		panic( "Trying to write bad sector" );
	}

	if ( !controller_ready() )	// ����ȴ�һ��ʱ�����δ���������,����
	{
		panic( "HD controller not ready" );
	}

	do_hd = intr_addr;	// do_hd ����ָ�뽫��Ӳ���жϳ����б�����

	outb_p( (UCHAR)hd_info[ drive ].ctl, HD_CMD );	// ����ƼĴ���( 0x3f6 )��������ֽ�

	port = HD_DATA;// ��dx Ϊ���ݼĴ����˿�( 0x1f0 )

	outb_p( (UCHAR)(hd_info[ drive ].wpcom >> 2	 )	, (USHORT)(++port) );		// ����:дԤ���������( ���4 )
	outb_p( (UCHAR)(nsect						 )	, (USHORT)(++port) );		// ����:��/д��������.
	outb_p( (UCHAR)(sect						 )	, (USHORT)(++port) );		// ����:��ʼ����.
	outb_p( (UCHAR)(cyl							 )	, (USHORT)(++port) );		// ����:����ŵ�8 λ.
	outb_p( (UCHAR)(cyl >> 8					 )	, (USHORT)(++port) );		// ����:����Ÿ�8 λ.
	outb_p( (UCHAR)(0xA0 | ( drive << 4 ) | head )	, (USHORT)(++port) );		// ����:��������+��ͷ��
	outb  ( (UCHAR)(cmd							 )	, (USHORT)(++port) );		// ����:Ӳ�̿�������
}

// �ȴ�Ӳ�̾���.Ҳ��ѭ���ȴ���״̬������æ��־λ��λ.�����о�����Ѱ��������־
// ��λ,��ɹ�,����0.������һ��ʱ����Ϊæ,�򷵻�1.
static LONG drive_busy()
{
	ULONG i;

	for ( i = 0; i < 10000; i++ ) // ѭ���ȴ�������־λ��λ
	{
		if ( READY_STAT == ( inb_p( HD_STATUS ) & ( BUSY_STAT | READY_STAT ) ) )
		{
			break;
		}
	}

	i  = inb( HD_STATUS );					// ��ȡ��������״̬�ֽ�
	i &= BUSY_STAT | READY_STAT | SEEK_STAT;// ���æλ������λ��Ѱ������λ

	if ( i == ( READY_STAT | SEEK_STAT ) )	// �����о�����Ѱ��������־,�򷵻�0
	{
		return 0;
	}

	printk( "HD controller times out\n\r" );// ����ȴ���ʱ,��ʾ��Ϣ.������1

	return 1;
}

// ��ϸ�λ( ����У�� )Ӳ�̿�����
static VOID reset_controller()
{
	LONG	i;

	outb( 4, HD_CMD );	// ����ƼĴ����˿ڷ��Ϳ����ֽ�( 4-��λ )

	for ( i = 0; i < 100; i++ ) 
	{
		nop();
	}

	outb( hd_info[ 0 ].ctl & 0x0f, HD_CMD ); // �ٷ��������Ŀ����ֽ�( ����ֹ���ԡ��ض� )

	if ( drive_busy() ) // ���ȴ�Ӳ�̾�����ʱ,����ʾ������Ϣ
	{
		printk( "HD-controller still busy\n\r" );
	}

	if ( ( i = inb( HD_ERROR ) ) != 1 ) // ȡ����Ĵ���,��������1( �޴��� )�����
	{
		printk( "HD-controller reset failed: %02x\n\r", i );
	}
}

// ��λӲ��nr.���ȸ�λ( ����У�� )Ӳ�̿�����.Ȼ����Ӳ�̿���������"��������������",
// ����recal_intr()����Ӳ���жϴ�������е��õ�����У��������
static VOID reset_hd( LONG nr )
{
	reset_controller();

	hd_out( nr,
			hd_info[ nr ].sect,
			hd_info[ nr ].sect, 
			hd_info[ nr ].head - 1,
			hd_info[ nr ].cyl, 
			WIN_SPECIFY, 
			&recal_intr );
}

// ����Ӳ���жϵ��ú���.
// ��������Ӳ���ж�ʱ,Ӳ���жϴ�������е��õ�Ĭ��C ������.�ڱ����ú���ָ��Ϊ��ʱ
// ���øú���.�μ�( kernel/system_call.s,241 �� )
VOID unexpected_hd_interrupt()
{
	printk( "Unexpected HD interrupt\n\r" );
}

// ��дӲ��ʧ�ܴ�����ú���
static VOID bad_rw_intr()
{
	if ( ++CURRENT->errors >= MAX_ERRORS )
	{
		end_request( 0 );
	}
	// ��Ӧ���������±�־��λ( û�и��� )
	if ( CURRENT->errors > MAX_ERRORS / 2 )
	{
		hd_reset = 1;
	}
}

// �������жϵ��ú���.����ִ��Ӳ���жϴ�������б�����
static VOID read_intr()
{
	if ( win_result() )	// ��������æ����д�������ִ�д�
	{ 
		bad_rw_intr();	// ��дӲ��ʧ�ܴ���
		do_hd_request();// Ȼ���ٴ�����Ӳ������Ӧ( ��λ )����

		return;
	}

	port_read( HD_DATA, CURRENT->buffer, 256 );	// �����ݴ����ݼĴ����ڶ�������ṹ������

	CURRENT->errors		= 0;	// ��������.
	CURRENT->buffer    += 512;	// ����������ָ��,ָ���µĿ���.
	CURRENT->sector++;			// ��ʼ�����ż�1,

	if ( --CURRENT->nr_sectors ) 
	{
		do_hd = &read_intr;		// �ٴ���Ӳ�̵���C ����ָ��Ϊread_intr()
		return;
	}

	end_request( 1 );	// ��ȫ�����������Ѿ�����,���������������

	do_hd_request();// ִ������Ӳ���������.
}

// д�����жϵ��ú���.��Ӳ���жϴ�������б�����.
// ��д����ִ�к�,�����Ӳ���ж��ź�,ִ��Ӳ���жϴ������,��ʱ��Ӳ���жϴ�������е��õ�
// C ����ָ��do_hd()�Ѿ�ָ��write_intr(),��˻���д�������( ����� )��,ִ�иú���.

static VOID write_intr()
{
	if ( win_result() )
	{
		bad_rw_intr();	 //Ӳ�̶�дʧ�ܴ���
		do_hd_request(); //����Ӳ������Ӧ( ��λ )����
		return;
	}

	if ( --CURRENT->nr_sectors )
	{
		CURRENT->sector++;
		CURRENT->buffer += 512;
		do_hd = &write_intr;						// ��Ӳ���жϳ�����ú���ָ��Ϊwrite_intr
		port_write( HD_DATA, CURRENT->buffer, 256 );// �������ݼĴ����˿�д256 �ֽ�
		return;
	}

	end_request( 1 );	// ��ȫ�����������Ѿ�д��,���������������,
	do_hd_request();	// ִ������Ӳ���������.
}

// Ӳ������У��( ��λ )�жϵ��ú���.��Ӳ���жϴ�������б�����.
// ���Ӳ�̿��������ش�����Ϣ,�����Ƚ���Ӳ�̶�дʧ�ܴ���,Ȼ������Ӳ������Ӧ( ��λ )����.

static VOID recal_intr()
{
	if ( win_result() )
	{
		bad_rw_intr();
	}
	do_hd_request();
}

// ִ��Ӳ�̶�д�������
VOID do_hd_request()
{
	LONG	i, r;
	ULONG	block, dev;
	ULONG	sec, head, cyl;
	ULONG	nsect;

	INIT_REQUEST;					// ���������ĺϷ��� �μ�kernel/blk_drv/blk.h

	dev   = MINOR( CURRENT->dev );	// CURRENT ����Ϊ( blk_dev[ MAJOR_NR ].current_request )
	block = CURRENT->sector;		// �������ʼ����.

	// ������豸�Ų����ڻ�����ʼ�������ڸ÷���������-2,�����������,����ת�����repeat ��
	// ( ������INIT_REQUEST ��ʼ�� ).��Ϊһ��Ҫ���д2 ������( 512*2 �ֽ� ),���������������
	// ���ܴ��ڷ�����������ڶ���������.
	if ( dev >= 5 * ( ULONG )NR_HD || block + 2 > ( ULONG )hd[ dev ].nr_sects ) 
	{
		end_request( 0 );
		goto repeat;
	}

	block += hd[ dev ].start_sect;	// ��������Ŀ��Ӧ������Ӳ���ϵľ���������
	dev   /= 5;						// ��ʱdev ����Ӳ�̺�( 0 ��1 )

	//��Ӳ����Ϣ�ṹ�и�����ʼ�����ź�ÿ�ŵ������������ڴŵ��е�������( sec )�����������( cyl )�ʹ�ͷ��( head )
		 
	sec		= block % hd_info[ dev ].sect;
	block  /= hd_info[ dev ].sect;
	head	= block % hd_info[ dev ].head;
	cyl		= block / hd_info[ dev ].head;

	sec++;

	nsect	= CURRENT->nr_sectors;	// ����/д��������

	// ���reset ��1,��ִ�и�λ����.��λӲ�̺Ϳ�����,������Ҫ����У����־,����
	if ( hd_reset )
	{
		hd_reset		= 0;
		hd_recalibrate	= 1;
		reset_hd( CURRENT_DEV );
		return;
	}

	// �������У����־( recalibrate )��λ,�����ȸ�λ�ñ�־,Ȼ����Ӳ�̿�������������У������.

	if ( hd_recalibrate ) 
	{
		hd_recalibrate = 0;

		hd_out( dev, hd_info[ CURRENT_DEV ].sect, 0, 0, 0,WIN_RESTORE, &recal_intr );
			
		return;
	}

	// �����ǰ������д��������,����д����,ѭ����ȡ״̬�Ĵ�����Ϣ���ж���������־
	// DRQ_STAT �Ƿ���λ.DRQ_STAT ��Ӳ��״̬�Ĵ������������λ( include/linux/hdreg.h,27 ).

	if ( CURRENT->cmd == WRITE ) 
	{
		hd_out( dev, nsect, sec, head, cyl, WIN_WRITE, &write_intr );

		for ( i = 0; i < 3000 && !( r = inb_p( HD_STATUS )&DRQ_STAT ); i++ )
			/* nothing */;

		// ����������λ��λ���˳�ѭ��.���ȵ�ѭ������Ҳû����λ,��˴�дӲ�̲���ʧ��,ȥ����
		// ��һ��Ӳ������.������Ӳ�̿��������ݼĴ����˿�HD_DATA д��1 ������������
		if ( !r ) 
		{
			bad_rw_intr();
			goto repeat;
		}

		port_write( HD_DATA, CURRENT->buffer, 256 );
	}
	else if ( CURRENT->cmd == READ )
	{
		hd_out( dev, nsect, sec, head, cyl, WIN_READ, &read_intr );
	}
	else
	{
		panic( "unknown hd-command" );
	}	
}

//Ӳ��ϵͳ��ʼ��
VOID hd_init()
{
	blk_dev[ MAJOR_NR ].request_fn = DEVICE_REQUEST;

	set_intr_gate( 0x2E, &hd_interrupt );		// ����Ӳ���ж������� LONG 0x2E( 46 )

	outb_p	( inb_p( 0x21 ) & 0xfb, 0x21 );		// ��λ��������8259A int2 ������λ,�����Ƭ�����ж������ź�
	outb	( inb_p( 0xA1 ) & 0xbf, 0xA1 );		// ��λӲ�̵��ж���������λ( �ڴ�Ƭ�� ),����Ӳ�̿����������ж������ź�
}
