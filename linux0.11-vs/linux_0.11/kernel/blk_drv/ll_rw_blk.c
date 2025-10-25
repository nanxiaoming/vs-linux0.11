/*
*  linux/kernel/blk_dev/ll_rw.c
*
* ( C ) 1991 Linus Torvalds
*/

/*
* This handles all read/write requests to block devices
*/
#include <errno.h>
#include <linux\sched.h>
#include <linux\kernel.h>
#include <asm\system.h>

#include "blk.h"

/*
* The request-struct contains all necessary data
* to load a nr of sectors into memory
*/
Request request[ NR_REQUEST ];

/*
* used to wait on when there are no free requests
*/
Task_Struct * wait_for_request = NULL;

/* blk_dev_struct is:
*	do_request-address
*	next-request
*/
BLK_DEV blk_dev[ NR_BLK_DEV ] = {
	{ NULL, NULL },		/* no_dev	*/	// 0 - ���豸.
	{ NULL, NULL },		/* dev mem	*/	// 1 - �ڴ�.
	{ NULL, NULL },		/* dev fd	*/	// 2 - �����豸.
	{ NULL, NULL },		/* dev hd	*/	// 3 - Ӳ���豸.
	{ NULL, NULL },		/* dev ttyx */	// 4 - ttyx �豸.
	{ NULL, NULL },		/* dev tty	*/	// 5 - tty �豸.
	{ NULL, NULL }		/* dev lp	*/	// 6 - lp ��ӡ���豸.
};

// ����ָ���Ļ�����bh.���ָ���Ļ������Ѿ���������������,��ʹ�Լ�˯��( �����жϵصȴ� ),
// ֱ����ִ�н�����������������ȷ�ػ���
static __inline VOID lock_buffer( Buffer_Head * bh )
{
	cli();

	while ( bh->b_lock )			// ����������ѱ�����,��˯��,ֱ������������
	{
		sleep_on( &bh->b_wait );
	}

	bh->b_lock = 1;					// ���������û�����

	sti();
}

// �ͷ�( ���� )�����Ļ�����
static __inline VOID unlock_buffer( Buffer_Head * bh )
{
	if ( !bh->b_lock )				// ����û�������û�б�����,���ӡ������Ϣ
	{
		printk( "ll_rw_block.c: buffer not locked\n\r" );
	}

	bh->b_lock = 0;					// ��������־

	wake_up( &bh->b_wait );			// ���ѵȴ��û�����������
}

/*
 * add-request adds a request to the linked list.
 * It disables interrupts so that it can muck with the
 * request-lists in peace.
 */
/*
 * add-request()�������м���һ������.���ر��ж�,
 * �������ܰ�ȫ�ش�������������
 */
// �������м���������.����dev ָ�����豸,req ������Ľṹ��Ϣ
static VOID add_request( BLK_DEV *dev, Request *req )
{
	Request *tmp;

	req->next = NULL;

	cli();

	if ( req->bh )
	{
		req->bh->b_dirt = 0;		// �建����"��"��־
	}

	if ( !( tmp = dev->current_request ) ) 
	{
		dev->current_request = req;

		sti();

		( dev->request_fn )();	// ִ���豸������,����Ӳ��( 3 )��do_hd_request()

		return;
	}

	// 
	// IN_ORDER ��Ŀ���Ǳ�֤�����㷨,Ϊ�µ������ҵ�λ��
	// 
	for ( ; tmp->next; tmp = tmp->next )
	{
		if ( ( IN_ORDER( tmp, req ) || !IN_ORDER( tmp, tmp->next ) ) && IN_ORDER( req, tmp->next ) )
		{
			break;
		}
	}

	//
	// ����ǰ����������������еĺ���λ��
	//
	req->next = tmp->next;
	tmp->next = req;

	sti();
}

static VOID make_request( LONG major, LONG rw, Buffer_Head * bh )
/*++

Routine Description:

	make_request.����һ��IO����.���ҷ��뵽��������еĺ���λ��.

Arguments:

	major	- �豸��
	rw		- ��д
	bh		- buffer����

Return Value:

	VOID

--*/
{
	Request *	req;
	LONG		rw_ahead;

	/* WRITEA/READA is special case - it is not really needed, so if the */
	/* buffer is locked, we just forget about it, else it's a normal read */
	/* WRITEA/READA ���������� - ���ǲ����Ǳ�Ҫ��,��������������Ѿ�����,*/
	/* ���ǾͲ��������˳�,����Ļ���ִ��һ��Ķ�/д����. */
	// ����'READ'��'WRITE'�����'A'�ַ�����Ӣ�ĵ���Ahead,��ʾ��ǰԤ��/д���ݿ����˼.
	// ��ָ���Ļ���������ʹ��,�ѱ�����ʱ,�ͷ���Ԥ��/д����

	if ( rw_ahead = ( rw == READA || rw == WRITEA ) ) 
	{
		if ( bh->b_lock )
		{
			return;
		}
		if ( rw == READA )
		{
			rw = READ;
		}
		else
		{
			rw = WRITE;
		}
	}

	if ( rw != READ && rw != WRITE )
	{
		panic( "Bad block dev command, must be R/W/RA/WA" );
	}

	// ����������,����������Ѿ�����,��ǰ����( ���� )�ͻ�˯��,ֱ������ȷ�ػ���
	lock_buffer( bh );

	// ���
	// 1.д���һ��������ݲ���,
	// 2.�����һ����������Ǹ��¹���,
	// ���������.���������������˳�

	if ( ( rw == WRITE && !bh->b_dirt ) || ( rw == READ && bh->b_uptodate ) ) 
	{
		unlock_buffer( bh );
		return;
	}

repeat:

	/*
	 * we don't allow the write-requests to fill up the queue completely:
	 * we want some room for reads: they take precedence. The last third
	 * of the requests are only for reads.
	 */

	/* ���ǲ����ö�����ȫ����д������:������ҪΪ��������һЩ�ռ�:
	 * �����������ȵ�.������еĺ�����֮һ�ռ���Ϊ��׼����.
	 */
	// �������Ǵ���������ĩβ��ʼ�������������.��������Ҫ��,���ڶ���������,����ֱ��
	// �Ӷ���ĩβ��ʼ����,��д������ֻ�ܴӶ��е� 2/3 ����ͷ��������������
	if ( rw == READ )
	{
		req = request + NR_REQUEST;
	}
	else
	{
		req = request + ( ( NR_REQUEST * 2 ) / 3 );
	}
	/* find an empty request */

	/* ����һ���������� */
	// �Ӻ���ǰ����,������ṹrequest ��dev �ֶ�ֵ=-1 ʱ,��ʾ����δ��ռ��.
	while ( --req >= request )
	{
		if ( req->dev < 0 )
		{
			break;
		}
	}
	/* if none found, sleep on new requests: check for rw_ahead */

	/* ���û���ҵ�������,���øô�������˯��:�����Ƿ���ǰ��/д */
	// ���û��һ���ǿ��е�( ��ʱrequest ����ָ���Ѿ�����Խ��ͷ�� ),��鿴�˴������Ƿ���
	// ��ǰ��/д( READA �� WRITEA ),�����������˴�����.�����ñ�������˯��( �ȴ���������ڳ����� ),
	// ��һ�����������������
	if ( req < request ) 
	{
		// ������������û�п���
		if ( rw_ahead ) 
		{
			unlock_buffer( bh );		// �������ǰ��/д����,�����������,�˳�.
			return;
		}

		sleep_on( &wait_for_request );	// �����ñ�������˯��,�����ٲ鿴�������.
		goto repeat;
	}

	/* ���������������д������Ϣ,�������������� */
	// ����ṹ�μ�( kernel/blk_drv/blk.h,23 ).
	/* fill up the request-info, and add it to the queue */

	req->dev		= bh->b_dev;				// �豸��.
	req->cmd		= rw;						// ����( READ/WRITE ).
	req->errors		= 0;						// ����ʱ�����Ĵ������.
	req->sector		= bh->b_blocknr << 1;		// ��ʼ����.( 1 ��=2 ���� )
	req->nr_sectors = 2;						// ��д������.
	req->buffer		= bh->b_data;				// ���ݻ�����.
	req->waiting	= NULL;						// ����ȴ�����ִ����ɵĵط�.
	req->bh			= bh;						// ������ͷָ��.
	req->next		= NULL;						// ָ����һ������.

	add_request( major + blk_dev, req );		// ����������������( blk_dev[ major ],req ).
}

VOID ll_rw_block( LONG rw, Buffer_Head *bh )
/*++

Routine Description:

	ll_rw_block.�Ͳ��д���ݿ麯��.

	�ú�����Ҫ���� fs/buffer.c �б�����.ʵ�ʵĶ�д���������豸�� request_fn() �������.
	����Ӳ�̲���,�ú����� do_hd_request().

Arguments:

	rw - ��д
	bh - Buffer head

Return Value:

	VOID

--*/
{
	ULONG major;

	if ( ( ( major = MAJOR( bh->b_dev ) ) >= NR_BLK_DEV ) || ( !( blk_dev[ major ].request_fn ) ) )
	{
		printk( "Trying to read nonexistent block-device\n\r" );
		return;
	}

	make_request( major, rw, bh );
}

VOID blk_dev_init()
/*++

Routine Description:

	blk_dev_init
	���豸��ʼ������
	��ʼ����������,��������������Ϊ������( dev = -1 ).�� 32 ��( NR_REQUEST = 32 )

Arguments:

	VOID

Return Value:

	VOID

--*/
{
	LONG i;

	for ( i = 0; i < NR_REQUEST; i++ ) 
	{
		request[ i ].dev = -1;
		request[ i ].next = NULL;
	}
}
