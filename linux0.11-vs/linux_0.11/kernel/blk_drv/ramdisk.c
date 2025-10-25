/*
*  linux/kernel/blk_drv/ramdisk.c
*
*  Written by Theodore Ts'o, 12/2/91
*/

// 由Theodore Ts'o 编制,12/2/91
// Theodore Ts'o ( Ted Ts'o )是linux 社区中的著名人物.Linux 在世界范围内的流行也有他很大的
// 功劳,早在Linux 操作系统刚问世时,他就怀着极大的热情为linux 的发展提供了maillist,并
// 在北美洲地区最早设立了linux 的ftp 站点( tsx-11.mit.edu ),而且至今仍然为广大linux 用户
// 提供服务.他对linux 作出的最大贡献之一是提出并实现了ext2 文件系统.该文件系统已成为
// linux 世界中事实上的文件系统标准.最近他又推出了ext3 文件系统,大大提高了文件系统的
// 稳定性和访问效率.作为对他的推崇,第97 期( 2002 年5 月 )的linuxjournal 期刊将他作为
// 了封面人物,并对他进行了采访.目前,他为IBM linux 技术中心工作,并从事着有关LSB
// ( Linux Standard Base )等方面的工作.( 他的主页:http://thunk.org/tytso/ )

#include <string.h>

#include <linux\config.h>
#include <linux\sched.h>
#include <linux\fs.h>
#include <linux\kernel.h>
#include <asm\system.h>
#include <asm\segment.h>
#include <asm\memory.h>

#define MAJOR_NR 1	// 内存主设备号是1
#include "blk.h"

CHAR	*rd_start;
LONG	rd_length = 0;

// 执行虚拟盘( ramdisk )读写操作.程序结构与do_hd_request()类似( kernel/blk_drv/hd.c,294 ).

VOID do_rd_request()
{
	LONG	len;
	CHAR	*addr;

	INIT_REQUEST;

	// 下面语句取得ramdisk 的起始扇区对应的内存起始位置和内存长度.
	// 其中sector << 9 表示sector * 512,CURRENT 定义为( blk_dev[ MAJOR_NR ].current_request ).

	addr = rd_start + ( CURRENT->sector << 9 );
	len = CURRENT->nr_sectors << 9;

	// 如果子设备号不为1 或者对应内存起始位置>虚拟盘末尾,则结束该请求,并跳转到repeat 处
	// ( 定义在28 行的INIT_REQUEST 内开始处 ).
	if ( ( MINOR( CURRENT->dev ) != 1 ) || ( addr + len > rd_start + rd_length ) ) {
		end_request( 0 );
		goto repeat;
	}
	// 如果是写命令( WRITE ),则将请求项中缓冲区的内容复制到addr 处,长度为len 字节
	if ( CURRENT->cmd == WRITE ) 
	{
		// 如果是读命令( READ ),则将addr 开始的内容复制到请求项中缓冲区中,长度为len 字节
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

	// 请求项成功后处理,置更新标志.并继续处理本设备的下一请求项
	end_request( 1 );
	goto repeat;
}

/*
 * Returns amount of memory which needs to be reserved.
 */

/* 返回内存虚拟盘ramdisk 所需的内存量 */
// 虚拟盘初始化函数.确定虚拟盘在内存中的起始地址,长度.并对整个虚拟盘区清零.
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
 * 如果根文件系统设备( root device )是ramdisk 的话,则尝试加载它.root device 原先是指向
 * 软盘的,我们将它改成指向ramdisk.
 */
// 加载根文件系统到ramdisk
VOID rd_load()
{
	Buffer_Head *	bh;
	Super_Block		s;
	LONG			block = 256;	/* Start at block 256 */
	LONG			i = 1;
	LONG			nblocks;
	CHAR		*	cp;		/* Move pointer */

	if ( !rd_length )			// 如果ramdisk 的长度为零,则退出
	{
		return;
	}

	printk( "Ram disk: %d bytes, starting at 0x%x\n", rd_length,( LONG )rd_start );
		
	if ( MAJOR( ROOT_DEV ) != 2 )	// 如果此时根文件设备不是软盘,则退出
	{
		return;
	}

	// 读软盘块256+1,256,256+2.breada()用于读取指定的数据块,并标出还需要读的块,然后返回
	// 含有数据块的缓冲区指针.如果返回NULL,则表示数据块不可读( fs/buffer.c,322 ).
	// 这里block+1 是指磁盘上的超级块.

	bh = breada( ROOT_DEV, block + 1, block, block + 2, -1 );

	if ( !bh ) 
	{
		printk( "Disk error while looking for ramdisk!\n" );
		return;
	}

	// 将 s 指向缓冲区中的磁盘超级块.( d_super_block 磁盘中超级块结构 )

	*( ( D_Super_Block * ) &s ) = *( ( D_Super_Block * ) bh->b_data );

	brelse( bh );

	if ( s.s_magic != SUPER_MAGIC )	// 如果超级块中魔数不对,则说明不是minix 文件系统
	{
		/* No ram disk image present, assume normal floppy boot */
		return;
	}

	// 块数 = 逻辑块数( 区段数 ) * 2^( 每区段块数的次方 ).
	// 如果数据块数大于内存中虚拟盘所能容纳的块数,则不能加载,显示出错信息并返回.否则显示
	// 加载数据块信息.

	nblocks = s.s_nzones << s.s_log_zone_size;

	if ( nblocks > ( rd_length >> BLOCK_SIZE_BITS ) ) 
	{
		printk( "Ram disk image too big!  ( %d blocks, %d avail )\n",nblocks, rd_length >> BLOCK_SIZE_BITS );
		return;
	}

	printk( "Loading %d bytes into ram disk... 0000k",nblocks << BLOCK_SIZE_BITS );
		
	// cp 指向虚拟盘起始处,然后将磁盘上的根文件系统映象文件复制到虚拟盘上
	cp = rd_start;

	while ( nblocks ) 
	{
		if ( nblocks > 2 )	// 如果需读取的块数多于3 快则采用超前预读方式读数据块.
		{
			bh = breada( ROOT_DEV, block, block + 1, block + 2, -1 );
		}
		else				// 否则就单块读取
		{
			bh = bread( ROOT_DEV, block );
		}

		if ( !bh )
		{
			printk( "I/O error on block %d, aborting load\n",block );
			return;
		}

		memcpy( cp, bh->b_data, BLOCK_SIZE );	// 将缓冲区中的数据复制到 cp 处

		brelse( bh );

		printk( "\010\010\010\010\010%4dk", i );

		cp += BLOCK_SIZE;

		block++;
		nblocks--;
		i++;
	}

	printk( "\010\010\010\010\010done \n" );

	ROOT_DEV = 0x0101;			// 修改ROOT_DEV 使其指向虚拟盘ramdisk
}
