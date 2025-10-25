/*
 *  linux/fs/truncate.c
 *
 *  ( C ) 1991  Linus Torvalds
 */

#include <linux\sched.h>

#include <sys\stat.h>

// 释放一次间接块
static VOID free_ind( LONG dev, LONG block )
{
	Buffer_Head *bh;
	USHORT *p;
	LONG i;

	if ( !block )
		return;
	// 读取一次间接块,并释放其上表明使用的所有逻辑块,然后释放该一次间接块的缓冲区
	if ( bh = bread( dev, block ) ) {
		p = ( USHORT * )bh->b_data;
		for ( i = 0; i < 512; i++, p++ ) // 每个逻辑块上可有512 个块号
			if ( *p )
				free_block( dev, *p );
		brelse( bh );
	}
	free_block( dev, block );
}

// 释放二次间接块
static VOID free_dind( LONG dev, LONG block )
{
	Buffer_Head *bh;
	USHORT *p;
	LONG i;

	if ( !block )
		return;
	// 读取二次间接块的一级块,并释放其上表明使用的所有逻辑块,然后释放该一级块的缓冲区
	if ( bh = bread( dev, block ) ) {
		p = ( USHORT* )bh->b_data;

		// 释放所有一次间接块
		for ( i = 0; i < 512; i++, p++ )
			if ( *p )
				free_ind( dev, *p );
		brelse( bh );
	}
	// 最后释放设备上的二次间接块
	free_block( dev, block );
}

VOID truncate( M_Inode * inode )
{
	LONG i;

	if ( !( S_ISREG( inode->i_mode ) || S_ISDIR( inode->i_mode ) ) )
		return;
	// 释放i 节点的7 个直接逻辑块,并将这7 个逻辑块项全置零
	for ( i = 0; i < 7; i++ )
		if ( inode->i_zone[ i ] ) {
			free_block( inode->i_dev, inode->i_zone[ i ] );
			inode->i_zone[ i ] = 0;
		}
	free_ind( inode->i_dev, inode->i_zone[ 7 ] );	// 释放一次间接块.
	free_dind( inode->i_dev, inode->i_zone[ 8 ] );	// 释放二次间接块.
	inode->i_zone[ 7 ] = inode->i_zone[ 8 ] = 0;	// 逻辑块项7、8 置零
	inode->i_size = 0;
	inode->i_dirt = 1;
	inode->i_mtime = inode->i_ctime = CURRENT_TIME;
}
