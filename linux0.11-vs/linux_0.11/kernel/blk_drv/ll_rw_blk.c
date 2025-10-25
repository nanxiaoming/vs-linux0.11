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
	{ NULL, NULL },		/* no_dev	*/	// 0 - 无设备.
	{ NULL, NULL },		/* dev mem	*/	// 1 - 内存.
	{ NULL, NULL },		/* dev fd	*/	// 2 - 软驱设备.
	{ NULL, NULL },		/* dev hd	*/	// 3 - 硬盘设备.
	{ NULL, NULL },		/* dev ttyx */	// 4 - ttyx 设备.
	{ NULL, NULL },		/* dev tty	*/	// 5 - tty 设备.
	{ NULL, NULL }		/* dev lp	*/	// 6 - lp 打印机设备.
};

// 锁定指定的缓冲区bh.如果指定的缓冲区已经被其它任务锁定,则使自己睡眠( 不可中断地等待 ),
// 直到被执行解锁缓冲区的任务明确地唤醒
static __inline VOID lock_buffer( Buffer_Head * bh )
{
	cli();

	while ( bh->b_lock )			// 如果缓冲区已被锁定,则睡眠,直到缓冲区解锁
	{
		sleep_on( &bh->b_wait );
	}

	bh->b_lock = 1;					// 立刻锁定该缓冲区

	sti();
}

// 释放( 解锁 )锁定的缓冲区
static __inline VOID unlock_buffer( Buffer_Head * bh )
{
	if ( !bh->b_lock )				// 如果该缓冲区并没有被锁定,则打印出错信息
	{
		printk( "ll_rw_block.c: buffer not locked\n\r" );
	}

	bh->b_lock = 0;					// 清锁定标志

	wake_up( &bh->b_wait );			// 唤醒等待该缓冲区的任务
}

/*
 * add-request adds a request to the linked list.
 * It disables interrupts so that it can muck with the
 * request-lists in peace.
 */
/*
 * add-request()向连表中加入一项请求.它关闭中断,
 * 这样就能安全地处理请求连表了
 */
// 向链表中加入请求项.参数dev 指定块设备,req 是请求的结构信息
static VOID add_request( BLK_DEV *dev, Request *req )
{
	Request *tmp;

	req->next = NULL;

	cli();

	if ( req->bh )
	{
		req->bh->b_dirt = 0;		// 清缓冲区"脏"标志
	}

	if ( !( tmp = dev->current_request ) ) 
	{
		dev->current_request = req;

		sti();

		( dev->request_fn )();	// 执行设备请求函数,对于硬盘( 3 )是do_hd_request()

		return;
	}

	// 
	// IN_ORDER 的目的是保证电梯算法,为新的请求找到位置
	// 
	for ( ; tmp->next; tmp = tmp->next )
	{
		if ( ( IN_ORDER( tmp, req ) || !IN_ORDER( tmp, tmp->next ) ) && IN_ORDER( req, tmp->next ) )
		{
			break;
		}
	}

	//
	// 将当前请求插入请求链表中的合适位置
	//
	req->next = tmp->next;
	tmp->next = req;

	sti();
}

static VOID make_request( LONG major, LONG rw, Buffer_Head * bh )
/*++

Routine Description:

	make_request.构建一个IO请求.并且放入到请求队列中的合适位置.

Arguments:

	major	- 设备号
	rw		- 读写
	bh		- buffer描述

Return Value:

	VOID

--*/
{
	Request *	req;
	LONG		rw_ahead;

	/* WRITEA/READA is special case - it is not really needed, so if the */
	/* buffer is locked, we just forget about it, else it's a normal read */
	/* WRITEA/READA 是特殊的情况 - 它们并不是必要的,所以如果缓冲区已经上锁,*/
	/* 我们就不管它而退出,否则的话就执行一般的读/写操作. */
	// 这里'READ'和'WRITE'后面的'A'字符代表英文单词Ahead,表示提前预读/写数据块的意思.
	// 当指定的缓冲区正在使用,已被上锁时,就放弃预读/写请求

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

	// 锁定缓冲区,如果缓冲区已经上锁,则当前任务( 进程 )就会睡眠,直到被明确地唤醒
	lock_buffer( bh );

	// 如果
	// 1.写并且缓冲区数据不脏,
	// 2.读并且缓冲区数据是更新过的,
	// 则这个请求.将缓冲区解锁并退出

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

	/* 我们不能让队列中全都是写请求项:我们需要为读请求保留一些空间:
	 * 读操作是优先的.请求队列的后三分之一空间是为读准备的.
	 */
	// 请求项是从请求数组末尾开始搜索空项填入的.根据上述要求,对于读命令请求,可以直接
	// 从队列末尾开始操作,而写请求则只能从队列的 2/3 处向头上搜索空项填入
	if ( rw == READ )
	{
		req = request + NR_REQUEST;
	}
	else
	{
		req = request + ( ( NR_REQUEST * 2 ) / 3 );
	}
	/* find an empty request */

	/* 搜索一个空请求项 */
	// 从后向前搜索,当请求结构request 的dev 字段值=-1 时,表示该项未被占用.
	while ( --req >= request )
	{
		if ( req->dev < 0 )
		{
			break;
		}
	}
	/* if none found, sleep on new requests: check for rw_ahead */

	/* 如果没有找到空闲项,则让该次新请求睡眠:需检查是否提前读/写 */
	// 如果没有一项是空闲的( 此时request 数组指针已经搜索越过头部 ),则查看此次请求是否是
	// 提前读/写( READA 或 WRITEA ),如果是则放弃此次请求.否则让本次请求睡眠( 等待请求队列腾出空项 ),
	// 过一会再来搜索请求队列
	if ( req < request ) 
	{
		// 如果请求队列中没有空项
		if ( rw_ahead ) 
		{
			unlock_buffer( bh );		// 如果是提前读/写请求,则解锁缓冲区,退出.
			return;
		}

		sleep_on( &wait_for_request );	// 否则让本次请求睡眠,过会再查看请求队列.
		goto repeat;
	}

	/* 向空闲请求项中填写请求信息,并将其加入队列中 */
	// 请求结构参见( kernel/blk_drv/blk.h,23 ).
	/* fill up the request-info, and add it to the queue */

	req->dev		= bh->b_dev;				// 设备号.
	req->cmd		= rw;						// 命令( READ/WRITE ).
	req->errors		= 0;						// 操作时产生的错误次数.
	req->sector		= bh->b_blocknr << 1;		// 起始扇区.( 1 块=2 扇区 )
	req->nr_sectors = 2;						// 读写扇区数.
	req->buffer		= bh->b_data;				// 数据缓冲区.
	req->waiting	= NULL;						// 任务等待操作执行完成的地方.
	req->bh			= bh;						// 缓冲区头指针.
	req->next		= NULL;						// 指向下一请求项.

	add_request( major + blk_dev, req );		// 将请求项加入队列中( blk_dev[ major ],req ).
}

VOID ll_rw_block( LONG rw, Buffer_Head *bh )
/*++

Routine Description:

	ll_rw_block.低层读写数据块函数.

	该函数主要是在 fs/buffer.c 中被调用.实际的读写操作是由设备的 request_fn() 函数完成.
	对于硬盘操作,该函数是 do_hd_request().

Arguments:

	rw - 读写
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
	块设备初始化函数
	初始化请求数组,将所有请求项置为空闲项( dev = -1 ).有 32 项( NR_REQUEST = 32 )

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
