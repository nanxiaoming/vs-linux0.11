/*
*  linux/kernel/blk_drv/ramdisk.c
*
*  Written by Theodore Ts'o, 12/2/91
*/

// ��Theodore Ts'o ����,12/2/91
// Theodore Ts'o ( Ted Ts'o )��linux �����е���������.Linux �����緶Χ�ڵ�����Ҳ�����ܴ��
// ����,����Linux ����ϵͳ������ʱ,���ͻ��ż��������Ϊlinux �ķ�չ�ṩ��maillist,��
// �ڱ����޵�������������linux ��ftp վ��( tsx-11.mit.edu ),����������ȻΪ���linux �û�
// �ṩ����.����linux �����������֮һ�������ʵ����ext2 �ļ�ϵͳ.���ļ�ϵͳ�ѳ�Ϊ
// linux ��������ʵ�ϵ��ļ�ϵͳ��׼.��������Ƴ���ext3 �ļ�ϵͳ,���������ļ�ϵͳ��
// �ȶ��Ժͷ���Ч��.��Ϊ�������Ƴ�,��97 ��( 2002 ��5 �� )��linuxjournal �ڿ�������Ϊ
// �˷�������,�����������˲ɷ�.Ŀǰ,��ΪIBM linux �������Ĺ���,���������й�LSB
// ( Linux Standard Base )�ȷ���Ĺ���.( ������ҳ:http://thunk.org/tytso/ )

#include <string.h>

#include <linux\config.h>
#include <linux\sched.h>
#include <linux\fs.h>
#include <linux\kernel.h>
#include <asm\system.h>
#include <asm\segment.h>
#include <asm\memory.h>

#define MAJOR_NR 1	// �ڴ����豸����1
#include "blk.h"

CHAR	*rd_start;
LONG	rd_length = 0;

// ִ��������( ramdisk )��д����.����ṹ��do_hd_request()����( kernel/blk_drv/hd.c,294 ).

VOID do_rd_request()
{
	LONG	len;
	CHAR	*addr;

	INIT_REQUEST;

	// �������ȡ��ramdisk ����ʼ������Ӧ���ڴ���ʼλ�ú��ڴ泤��.
	// ����sector << 9 ��ʾsector * 512,CURRENT ����Ϊ( blk_dev[ MAJOR_NR ].current_request ).

	addr = rd_start + ( CURRENT->sector << 9 );
	len = CURRENT->nr_sectors << 9;

	// ������豸�Ų�Ϊ1 ���߶�Ӧ�ڴ���ʼλ��>������ĩβ,�����������,����ת��repeat ��
	// ( ������28 �е�INIT_REQUEST �ڿ�ʼ�� ).
	if ( ( MINOR( CURRENT->dev ) != 1 ) || ( addr + len > rd_start + rd_length ) ) {
		end_request( 0 );
		goto repeat;
	}
	// �����д����( WRITE ),���������л����������ݸ��Ƶ�addr ��,����Ϊlen �ֽ�
	if ( CURRENT->cmd == WRITE ) 
	{
		// ����Ƕ�����( READ ),��addr ��ʼ�����ݸ��Ƶ��������л�������,����Ϊlen �ֽ�
		memcpy( addr,
				CURRENT->buffer,
				len );
	}
	else if ( CURRENT->cmd == READ )
	{
		memcpy( CURRENT->buffer,
				addr,
				len );
	}
	else
		panic( "unknown ramdisk-command" );

	// ������ɹ�����,�ø��±�־.�����������豸����һ������
	end_request( 1 );
	goto repeat;
}

/*
 * Returns amount of memory which needs to be reserved.
 */

/* �����ڴ�������ramdisk ������ڴ��� */
// �����̳�ʼ������.ȷ�����������ڴ��е���ʼ��ַ,����.��������������������.
LONG rd_init( LONG mem_start, LONG length )
{
	LONG	i;
	CHAR	*cp;

	blk_dev[ MAJOR_NR ].request_fn = DEVICE_REQUEST;
	rd_start = ( CHAR * )mem_start;
	rd_length = length;
	cp = rd_start;
	for ( i = 0; i < length; i++ )
		*cp++ = '\0';
	return( length );
}

/*
 * If the root device is the ram disk, try to load it.
 * In order to do this, the root device is originally set to the
 * floppy, and we later change it to be ram disk.
 */

/*
 * ������ļ�ϵͳ�豸( root device )��ramdisk �Ļ�,���Լ�����.root device ԭ����ָ��
 * ���̵�,���ǽ����ĳ�ָ��ramdisk.
 */
// ���ظ��ļ�ϵͳ��ramdisk
VOID rd_load()
{
	Buffer_Head *	bh;
	Super_Block		s;
	LONG			block = 256;	/* Start at block 256 */
	LONG			i = 1;
	LONG			nblocks;
	CHAR		*	cp;		/* Move pointer */

	if ( !rd_length )			// ���ramdisk �ĳ���Ϊ��,���˳�
	{
		return;
	}

	printk( "Ram disk: %d bytes, starting at 0x%x\n", rd_length,( LONG )rd_start );
		
	if ( MAJOR( ROOT_DEV ) != 2 )	// �����ʱ���ļ��豸��������,���˳�
	{
		return;
	}

	// �����̿�256+1,256,256+2.breada()���ڶ�ȡָ�������ݿ�,���������Ҫ���Ŀ�,Ȼ�󷵻�
	// �������ݿ�Ļ�����ָ��.�������NULL,���ʾ���ݿ鲻�ɶ�( fs/buffer.c,322 ).
	// ����block+1 ��ָ�����ϵĳ�����.

	bh = breada( ROOT_DEV, block + 1, block, block + 2, -1 );

	if ( !bh ) 
	{
		printk( "Disk error while looking for ramdisk!\n" );
		return;
	}

	// �� s ָ�򻺳����еĴ��̳�����.( d_super_block �����г�����ṹ )

	*( ( D_Super_Block * ) &s ) = *( ( D_Super_Block * ) bh->b_data );

	brelse( bh );

	if ( s.s_magic != SUPER_MAGIC )	// �����������ħ������,��˵������minix �ļ�ϵͳ
	{
		/* No ram disk image present, assume normal floppy boot */
		return;
	}

	// ���� = �߼�����( ������ ) * 2^( ÿ���ο����Ĵη� ).
	// ������ݿ��������ڴ����������������ɵĿ���,���ܼ���,��ʾ������Ϣ������.������ʾ
	// �������ݿ���Ϣ.

	nblocks = s.s_nzones << s.s_log_zone_size;

	if ( nblocks > ( rd_length >> BLOCK_SIZE_BITS ) ) 
	{
		printk( "Ram disk image too big!  ( %d blocks, %d avail )\n",nblocks, rd_length >> BLOCK_SIZE_BITS );
		return;
	}

	printk( "Loading %d bytes into ram disk... 0000k",nblocks << BLOCK_SIZE_BITS );
		
	// cp ָ����������ʼ��,Ȼ�󽫴����ϵĸ��ļ�ϵͳӳ���ļ����Ƶ���������
	cp = rd_start;

	while ( nblocks ) 
	{
		if ( nblocks > 2 )	// ������ȡ�Ŀ�������3 ������ó�ǰԤ����ʽ�����ݿ�.
		{
			bh = breada( ROOT_DEV, block, block + 1, block + 2, -1 );
		}
		else				// ����͵����ȡ
		{
			bh = bread( ROOT_DEV, block );
		}

		if ( !bh )
		{
			printk( "I/O error on block %d, aborting load\n",block );
			return;
		}

		memcpy( cp, bh->b_data, BLOCK_SIZE );	// ���������е����ݸ��Ƶ� cp ��

		brelse( bh );

		printk( "\010\010\010\010\010%4dk", i );

		cp += BLOCK_SIZE;

		block++;
		nblocks--;
		i++;
	}

	printk( "\010\010\010\010\010done \n" );

	ROOT_DEV = 0x0101;			// �޸�ROOT_DEV ʹ��ָ��������ramdisk
}
