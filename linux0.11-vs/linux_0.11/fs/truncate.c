/*
 *  linux/fs/truncate.c
 *
 *  ( C ) 1991  Linus Torvalds
 */

#include <linux\sched.h>

#include <sys\stat.h>

// �ͷ�һ�μ�ӿ�
static VOID free_ind( LONG dev, LONG block )
{
	Buffer_Head *bh;
	USHORT *p;
	LONG i;

	if ( !block )
		return;
	// ��ȡһ�μ�ӿ�,���ͷ����ϱ���ʹ�õ������߼���,Ȼ���ͷŸ�һ�μ�ӿ�Ļ�����
	if ( bh = bread( dev, block ) ) {
		p = ( USHORT * )bh->b_data;
		for ( i = 0; i < 512; i++, p++ ) // ÿ���߼����Ͽ���512 �����
			if ( *p )
				free_block( dev, *p );
		brelse( bh );
	}
	free_block( dev, block );
}

// �ͷŶ��μ�ӿ�
static VOID free_dind( LONG dev, LONG block )
{
	Buffer_Head *bh;
	USHORT *p;
	LONG i;

	if ( !block )
		return;
	// ��ȡ���μ�ӿ��һ����,���ͷ����ϱ���ʹ�õ������߼���,Ȼ���ͷŸ�һ����Ļ�����
	if ( bh = bread( dev, block ) ) {
		p = ( USHORT* )bh->b_data;

		// �ͷ�����һ�μ�ӿ�
		for ( i = 0; i < 512; i++, p++ )
			if ( *p )
				free_ind( dev, *p );
		brelse( bh );
	}
	// ����ͷ��豸�ϵĶ��μ�ӿ�
	free_block( dev, block );
}

VOID truncate( M_Inode * inode )
{
	LONG i;

	if ( !( S_ISREG( inode->i_mode ) || S_ISDIR( inode->i_mode ) ) )
		return;
	// �ͷ�i �ڵ��7 ��ֱ���߼���,������7 ���߼�����ȫ����
	for ( i = 0; i < 7; i++ )
		if ( inode->i_zone[ i ] ) {
			free_block( inode->i_dev, inode->i_zone[ i ] );
			inode->i_zone[ i ] = 0;
		}
	free_ind( inode->i_dev, inode->i_zone[ 7 ] );	// �ͷ�һ�μ�ӿ�.
	free_dind( inode->i_dev, inode->i_zone[ 8 ] );	// �ͷŶ��μ�ӿ�.
	inode->i_zone[ 7 ] = inode->i_zone[ 8 ] = 0;	// �߼�����7��8 ����
	inode->i_size = 0;
	inode->i_dirt = 1;
	inode->i_mtime = inode->i_ctime = CURRENT_TIME;
}
