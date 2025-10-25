/*
*  linux/fs/inode.c
*
*  ( C ) 1991  Linus Torvalds
*/

#include <string.h>
#include <sys\stat.h>

#include <linux\sched.h>
#include <linux\kernel.h>
#include <linux\mm.h>
#include <asm\system.h>

M_Inode inode_table[ NR_INODE ] = { { 0, }, };// �ڴ���i �ڵ��( NR_INODE=32 �� )

static VOID read_inode ( M_Inode * inode );
static VOID write_inode( M_Inode * inode );

static __inline VOID wait_on_inode( M_Inode * inode )
{
	cli();

	while ( inode->i_lock )
	{
		sleep_on( &inode->i_wait );
	}

	sti();
}

static __inline VOID lock_inode( M_Inode * inode )
{
	cli();

	while ( inode->i_lock )
	{
		sleep_on( &inode->i_wait );
	}

	inode->i_lock = 1;

	sti();
}

static __inline VOID unlock_inode( M_Inode * inode )
{
	inode->i_lock = 0;

	wake_up( &inode->i_wait );
}

VOID invalidate_inodes( LONG dev )
/*++

Routine Description:

	�ͷ��ڴ����豸 dev ������ i �ڵ�.
	ɨ���ڴ��е� i �ڵ������,�����ָ���豸ʹ�õ� i �ڵ���ͷ�֮

Arguments:

	dev - �豸��

Return Value:

	VOID
	
--*/
{
	LONG		i;
	M_Inode *	inode;

	inode = 0 + inode_table;

	for ( i = 0; i < NR_INODE; i++, inode++ )
	{
		wait_on_inode( inode );

		if ( inode->i_dev == dev )
		{
			if ( inode->i_count )
			{
				printk( "inode in use on removed disk\n\r" );
			}

			inode->i_dev = inode->i_dirt = 0;
		}
	}
}

VOID sync_inodes()
/*++

Routine Description:

	ͬ������ i �ڵ�.
	ͬ���ڴ����豸�ϵ����� i �ڵ���Ϣ

Arguments:
	-
Return Value:
	-
--*/
{
	LONG		i;
	M_Inode *	inode;

	inode = 0 + inode_table;

	for ( i = 0; i < NR_INODE; i++, inode++ )
	{	
		wait_on_inode( inode );

		if ( inode->i_dirt && !inode->i_pipe )
		{
			write_inode( inode );
		}
	}
}

static LONG _bmap( M_Inode * inode, LONG block, LONG create )
/*++

Routine Description:

	block λͼ������.�ļ����ݿ�ӳ�䵽�̿�Ĵ������.

Arguments:

	inode	�C �ļ��� i �ڵ�
	block	�C �ļ��е����ݿ��
	create	- ������־

Return Value:

	���ݿ��Ӧ���豸�ϵ��߼����( �̿�� )

--*/
{
	Buffer_Head *	bh;
	LONG			i;

	if ( block < 0 )
	{
		panic( "_bmap: block < 0" );
	}

	// �����Ŵ���ֱ�ӿ��� + ��ӿ��� + ���μ�ӿ���,�����ļ�ϵͳ��ʾ��Χ,������
	if ( block >= 7 + 512 + 512 * 512 )
	{
		panic( "_bmap: block > big" );
	}

	// ����ÿ��С��7,��ʹ��ֱ�ӿ��ʾ 
	if ( block < 7 ) 
	{
		// ���������־��λ,���� i �ڵ��ж�Ӧ�ÿ���߼���( ���� )�ֶ�Ϊ0,������Ӧ�豸����һ����
		// ��( �߼���,���� ),���������߼����( �̿�� )�����߼����ֶ���.Ȼ������i �ڵ��޸�ʱ��,
		// ��i �ڵ����޸ı�־.��󷵻��߼����
		if ( create && !inode->i_zone[ block ] )
		{
			if ( inode->i_zone[ block ] = (USHORT)new_block( inode->i_dev ) ) 
			{
				inode->i_ctime = CURRENT_TIME;
				inode->i_dirt = 1;
			}
		}
		return inode->i_zone[ block ];
	}
	// ����ÿ��>=7,����С��7+512,��˵����һ�μ�ӿ�.�����һ�μ�ӿ���д���
	block -= 7;

	if ( block < 512 ) 
	{
		// ����Ǵ���,���Ҹ�i �ڵ��ж�Ӧ��ӿ��ֶ�Ϊ0,�����ļ����״�ʹ�ü�ӿ�,��������
		// һ���̿����ڴ�ż�ӿ���Ϣ,������ʵ�ʴ��̿�������ӿ��ֶ���.Ȼ������i �ڵ�
		// ���޸ı�־���޸�ʱ��
		if ( create && !inode->i_zone[ 7 ] )
		{
			if ( inode->i_zone[ 7 ] = (USHORT)new_block( inode->i_dev ) ) 
			{
				inode->i_dirt  = 1;
				inode->i_ctime = CURRENT_TIME;
			}
		}

		// ����ʱi �ڵ��ӿ��ֶ���Ϊ0,����������̿�ʧ��,����0 �˳�
		if ( !inode->i_zone[ 7 ] )
		{
			return 0;
		}

		// ��ȡ�豸�ϵ�һ�μ�ӿ�
		if ( !( bh = bread( inode->i_dev, inode->i_zone[ 7 ] ) ) )
		{
			return 0;
		}

		// ȡ�ü�ӿ��ϵ�block ���е��߼����( �̿�� )
		i = ( ( USHORT * )( bh->b_data ) )[ block ];

		// ����Ǵ������Ҽ�ӿ�ĵ�block ���е��߼����Ϊ0 �Ļ�,������һ���̿�( �߼��� ),����
		// ��ӿ��еĵ�block ����ڸ����߼�����.Ȼ����λ��ӿ�����޸ı�־
		if ( create && !i )
		{
			if ( i = new_block( inode->i_dev ) ) 
			{
				( ( USHORT * )( bh->b_data ) )[ block ] = (USHORT)i;
				bh->b_dirt = 1;
			}
		}

		// ����ͷŸü�ӿ�,���ش�����������Ķ�Ӧblock ���߼���Ŀ��
		brelse( bh );
		return i;
	}

	// �������е���,�������ݿ��Ƕ��μ�ӿ�,���������һ�μ�ӿ�����.�����ǶԶ��μ�ӿ�Ĵ���.
	// ��block �ټ�ȥ��ӿ������ɵĿ���( 512 )
	block -= 512;

	// ������´�������i �ڵ�Ķ��μ�ӿ��ֶ�Ϊ0,��������һ���̿����ڴ�Ŷ��μ�ӿ��һ����
	// ��Ϣ,������ʵ�ʴ��̿��������μ�ӿ��ֶ���.֮��,��i �ڵ����޸ı��ƺ��޸�ʱ��.

	if ( create && !inode->i_zone[ 8 ] )
	{
		if ( inode->i_zone[ 8 ] = (USHORT)new_block( inode->i_dev ) )
		{
			inode->i_dirt  = 1;
			inode->i_ctime = CURRENT_TIME;
		}
	}

	// ����ʱi �ڵ���μ�ӿ��ֶ�Ϊ0,����������̿�ʧ��,����0 �˳�
	if ( !inode->i_zone[ 8 ] )
	{
		return 0;
	}

	// ��ȡ�ö��μ�ӿ��һ����
	if ( !( bh = bread( inode->i_dev, inode->i_zone[ 8 ] ) ) )
	{
		return 0;
	}

	// ȡ�ö��μ�ӿ��һ�����ϵ�( block/512 )���е��߼����
	i = ( ( USHORT * )bh->b_data )[ block >> 9 ];

	// ����Ǵ������Ҷ��μ�ӿ��һ�����ϵ�( block/512 )���е��߼����Ϊ0 �Ļ�,��������һ����
	// ��( �߼��� )��Ϊ���μ�ӿ�Ķ�����,���ö��μ�ӿ��һ�����е�( block/512 )����ڸö���
	// ��Ŀ��.Ȼ����λ���μ�ӿ��һ�������޸ı�־.���ͷŶ��μ�ӿ��һ����
	if ( create && !i )
	{
		if ( i = new_block( inode->i_dev ) ) 
		{
			( ( USHORT * )( bh->b_data ) )[ block >> 9 ] = (USHORT)i;

			bh->b_dirt = 1;
		}
	}

	brelse( bh );

	// ������μ�ӿ�Ķ�������Ϊ0,��ʾ������̿�ʧ��,����0 �˳�
	if ( !i )
	{
		return 0;
	}

	// ��ȡ���μ�ӿ�Ķ�����
	if ( !( bh = bread( inode->i_dev, i ) ) )
	{
		return 0;
	}

	// ȡ�ö������ϵ�block ���е��߼����.( ����511 ��Ϊ���޶�block ֵ������511 )
	i = ( ( USHORT * )bh->b_data )[ block & 511 ];

	// ����Ǵ������Ҷ�����ĵ�block ���е��߼����Ϊ0 �Ļ�,������һ���̿�( �߼��� ),��Ϊ
	// ���մ��������Ϣ�Ŀ�.���ö������еĵ�block ����ڸ����߼�����( i ).Ȼ����λ�������
	// ���޸ı�־
	if ( create && !i )
	{
		if ( i = new_block( inode->i_dev ) ) 
		{
			( ( USHORT * )( bh->b_data ) )[ block & 511 ] = (USHORT)i;
			bh->b_dirt = 1;
		}
	}

	// ����ͷŸö��μ�ӿ�Ķ�����,���ش�����������Ķ�Ӧblock ���߼���Ŀ��
	brelse( bh );
	return i;
}

// ����i �ڵ���Ϣȡ�ļ����ݿ�block ���豸�϶�Ӧ���߼����
LONG bmap( M_Inode * inode, LONG block )
{
	return _bmap( inode, block, 0 );
}

// �����ļ����ݿ�block ���豸�϶�Ӧ���߼���,�������豸�϶�Ӧ���߼����
LONG create_block( M_Inode * inode, LONG block )
{
	return _bmap( inode, block, 1 );
}

// �ͷ�һ��i �ڵ�( ��д���豸 )
VOID iput( M_Inode * inode )
{
	if ( !inode )
	{
		return;
	}

	wait_on_inode( inode );

	if ( !inode->i_count )
	{
		panic( "iput: trying to free free inode" );
	}

	// ����ǹܵ�i �ڵ�,���ѵȴ��ùܵ��Ľ���,���ô�����1,������������򷵻�.�����ͷ�
	// �ܵ�ռ�õ��ڴ�ҳ��,����λ�ýڵ�����ü���ֵ�����޸ı�־�͹ܵ���־,������.
	// ����pipe �ڵ�,inode->i_size ����������ڴ�ҳ��ַ.�μ�get_pipe_inode(),228,234 ��.

	if ( inode->i_pipe )
	{
		wake_up( &inode->i_wait );

		if ( --inode->i_count )
		{
			return;
		}

		free_page( inode->i_size );

		inode->i_count	= 0;
		inode->i_dirt	= 0;
		inode->i_pipe	= 0;
		return;
	}
	// ���i �ڵ��Ӧ���豸��=0,�򽫴˽ڵ�����ü����ݼ�1,����
	if ( !inode->i_dev ) 
	{
		inode->i_count--;
		return;
	}
	// ����ǿ��豸�ļ���i �ڵ�,��ʱ�߼����ֶ�0 �����豸��,��ˢ�¸��豸.���ȴ�i �ڵ����.
	if ( S_ISBLK( inode->i_mode ) ) 
	{
		sync_dev( inode->i_zone[ 0 ] );
		wait_on_inode( inode );
	}
repeat:
	// ���i �ڵ�����ü�������1,��ݼ�1
	if ( inode->i_count > 1 ) 
	{
		inode->i_count--;
		return;
	}
	// ���i �ڵ��������Ϊ0,���ͷŸ�i �ڵ�������߼���,���ͷŸ�i �ڵ�
	if ( !inode->i_nlinks )
	{
		truncate  ( inode );
		free_inode( inode );
		return;
	}
	// �����i �ڵ��������޸�,����¸�i �ڵ�,���ȴ���i �ڵ����
	if ( inode->i_dirt ) 
	{
		write_inode  ( inode );	/* we can sleep - so do again */
		wait_on_inode( inode );
		goto repeat;
	}
	// i �ڵ����ü����ݼ�1
	inode->i_count--;
	return;
}

// ��i �ڵ��( inode_table )�л�ȡһ������i �ڵ���.
// Ѱ�����ü���count Ϊ0 ��i �ڵ�,������д�̺�����,������ָ��
M_Inode * get_empty_inode()
{
			M_Inode		*	inode;
	static	M_Inode		*	last_inode = inode_table; // last_inode ָ��i �ڵ���һ��
			LONG			i;

	do 
	{
		// ɨ��i �ڵ��
		inode = NULL;

		for ( i = NR_INODE; i; i-- ) 
		{
			// ���last_inode �Ѿ�ָ��i �ڵ������1 ��֮��,����������ָ��i �ڵ��ʼ��
			if ( ++last_inode >= inode_table + NR_INODE )
			{
				last_inode = inode_table;
			}

			// ���last_inode ��ָ���i �ڵ�ļ���ֵΪ0,��˵�������ҵ�����i �ڵ���.��inode ָ��
			// ��i �ڵ�.�����i �ڵ�����޸ı�־��������־��Ϊ0,�����ǿ���ʹ�ø�i �ڵ�,�����˳�ѭ��.

			if ( !last_inode->i_count ) 
			{
				inode = last_inode;

				if ( !inode->i_dirt && !inode->i_lock )
				{
					break;
				}
			}
		}
		// ���û���ҵ�����i �ڵ�( inode=NULL ),������i �ڵ���ӡ����������ʹ��,������.
		if ( !inode )
		{
			for ( i = 0; i < NR_INODE; i++ )
			{
				printk( "%04x: %6d\t", inode_table[ i ].i_dev,inode_table[ i ].i_num );
			}
				
			panic( "No free inodes in mem" );
		}

		// �ȴ���i �ڵ����( ����ֱ������Ļ� )

		wait_on_inode( inode );

		// �����i �ڵ����޸ı�־����λ�Ļ�,�򽫸�i �ڵ�ˢ��,���ȴ���i �ڵ����
		while ( inode->i_dirt ) 
		{
			write_inode		( inode );
			wait_on_inode	( inode );
		}
	} while ( inode->i_count );// ���i �ڵ��ֱ�����ռ�õĻ�,������Ѱ�ҿ���i �ڵ�
	// ���ҵ�����i �ڵ���.�򽫸�i �ڵ�����������,�������ñ�־Ϊ1,���ظ�i �ڵ�ָ��

	memset( inode, 0, sizeof( *inode ) );

	inode->i_count = 1;

	return inode;
}

// ��ȡ�ܵ��ڵ�.����Ϊi �ڵ�ָ��( �����NULL ��ʧ�� ).
// ����ɨ��i �ڵ��,Ѱ��һ������i �ڵ���,Ȼ��ȡ��һҳ�����ڴ湩�ܵ�ʹ��.
// Ȼ�󽫵õ���i �ڵ�����ü�����Ϊ2( ���ߺ�д�� ),��ʼ���ܵ�ͷ��β,��i �ڵ�Ĺܵ����ͱ�ʾ.

M_Inode * 
get_pipe_inode()
{
	M_Inode * inode;

	if ( !( inode = get_empty_inode() ) )			// ����Ҳ�������i �ڵ��򷵻�NULL
	{
		return NULL;
	}

	if ( !( inode->i_size = get_free_page() ) ) 	// �ڵ��i_size �ֶ�ָ�򻺳���.
	{
		inode->i_count = 0;							// �����û�п����ڴ�,��
		return NULL;								// �ͷŸ�i �ڵ�,������NULL.
	}

	inode->i_count = 2;								/* sum of readers/writers */ 	/* ��/д�����ܼ� */
	PIPE_HEAD( *inode ) = PIPE_TAIL( *inode ) = 0;
	inode->i_pipe = 1;								// �ýڵ�Ϊ�ܵ�ʹ�õı�־.

	return inode;
}

// ���豸�϶�ȡָ���ڵ�ŵ�i �ڵ�.
// nr - i �ڵ��.
M_Inode *iget( LONG dev, LONG nr )
{
	M_Inode *inode, *empty;

	if ( !dev )
	{
		panic( "iget with dev==0" );
	}

	// ��i �ڵ����ȡһ������i �ڵ�
	empty = get_empty_inode();
	
	// ɨ��i �ڵ��.Ѱ��ָ���ڵ�ŵ�i �ڵ�.�������ýڵ�����ô���
	inode = inode_table;

	while ( inode < NR_INODE + inode_table ) 
	{
		// �����ǰɨ���i �ڵ���豸�Ų�����ָ�����豸�Ż��߽ڵ�Ų�����ָ���Ľڵ��,�����ɨ��.
		if ( inode->i_dev != dev || inode->i_num != nr ) 
		{
			inode++;
			continue;
		}

		// �ҵ�ָ���豸�źͽڵ�ŵ�i �ڵ�,�ȴ��ýڵ����( ����������Ļ� )
		wait_on_inode( inode );

		// �ڵȴ��ýڵ�����Ľ׶�,�ڵ����ܻᷢ���仯,�����ٴ��ж�,��������˱仯,���ٴ�����
		// ɨ������i �ڵ��
		if ( inode->i_dev != dev || inode->i_num != nr ) 
		{
			inode = inode_table;
			continue;
		}
		// ����i �ڵ����ü�����1

		inode->i_count++;

		if ( inode->i_mount ) 
		{
			LONG i;

			// �����i �ڵ��������ļ�ϵͳ�İ�װ��,���ڳ����������Ѱ��װ�ڴ�i �ڵ�ĳ�����.���û��
			// �ҵ�,����ʾ������Ϣ,���ͷź�����ʼ��ȡ�Ŀ��нڵ�,���ظ�i �ڵ�ָ��
			for ( i = 0; i < NR_SUPER; i++ )
			{
				if ( super_block[ i ].s_imount == inode )
					break;
			}

			if ( i >= NR_SUPER ) 
			{
				printk( "Mounted inode hasn't got sb\n" );

				if ( empty )
				{
					iput( empty );
				}
				return inode;
			}

			// ����i �ڵ�д��.�Ӱ�װ�ڴ�i �ڵ��ļ�ϵͳ�ĳ�������ȡ�豸��,����i �ڵ��Ϊ1.Ȼ������
			// ɨ������i �ڵ��,ȡ�ñ���װ�ļ�ϵͳ�ĸ��ڵ�
			iput( inode );

			dev		= super_block[ i ].s_dev;
			nr		= ROOT_INO;
			inode	= inode_table;

			continue;
		}

		// �Ѿ��ҵ���Ӧ��i �ڵ�,��˷�����ʱ����Ŀ��нڵ�,���ظ��ҵ���i �ڵ�
		if ( empty )
		{
			iput( empty );
		}
		return inode;
	}
	// �����i �ڵ����û���ҵ�ָ����i �ڵ�,������ǰ������Ŀ���i �ڵ���i �ڵ���н����ýڵ�.
	// ������Ӧ�豸�϶�ȡ��i �ڵ���Ϣ.���ظ�i �ڵ�
	if ( !empty )
		return ( NULL );

	inode			= empty;
	inode->i_dev	= (USHORT)dev;
	inode->i_num	= (USHORT)nr;

	read_inode( inode );

	return inode;
}

// ���豸�϶�ȡָ��i �ڵ����Ϣ���ڴ���( �������� )
static VOID read_inode( M_Inode *inode )
{
	Super_Block *	sb;
	Buffer_Head *	bh;
	LONG			block;

	// ����������i �ڵ�,ȡ�ýڵ������豸�ĳ�����
	lock_inode( inode );

	if ( !( sb = get_super( inode->i_dev ) ) )
	{
		panic( "trying to read inode without dev" );
	}

	// ��i �ڵ����ڵ��߼����= ( ������+������ ) + i �ڵ�λͼռ�õĿ���+ �߼���λͼռ�õĿ���+
	// ( i �ڵ��-1 )/ÿ�麬�е�i �ڵ���

	block = 2 + 
			sb->s_imap_blocks + 
			sb->s_zmap_blocks + ( inode->i_num - 1 ) / INODES_PER_BLOCK;
		
	// ���豸�϶�ȡ��i �ڵ����ڵ��߼���,������inode ָ��ָ���Ӧi �ڵ���Ϣ
	if ( !( bh = bread( inode->i_dev, block ) ) )
	{
		panic( "unable to read i-node block" );
	}

	*( D_Inode * )inode = ( ( D_Inode * )bh->b_data )[
										   ( inode->i_num - 1 ) % INODES_PER_BLOCK 
													];
	// ����ͷŶ���Ļ�����,�������� i �ڵ�
	brelse( bh );

	unlock_inode( inode );
}

// ��ָ�� i �ڵ���Ϣд���豸( д�뻺������Ӧ�Ļ������,��������ˢ��ʱ��д������ )
static VOID write_inode( M_Inode * inode )
{
	Super_Block *	sb;
	Buffer_Head *	bh;
	LONG			block;

	// ����������i �ڵ�,�����i �ڵ�û�б��޸Ĺ����߸�i �ڵ���豸�ŵ�����,�������i �ڵ�,
	// ���˳�
	lock_inode( inode );

	if ( !inode->i_dirt || !inode->i_dev )
	{
		unlock_inode( inode );
		return;
	}

	// ��ȡ��i �ڵ�ĳ�����
	if ( !( sb = get_super( inode->i_dev ) ) )
	{
		panic( "trying to write inode without device" );
	}

	// �� i �ڵ����ڵ��߼����= ( ������+������ ) + i �ڵ�λͼռ�õĿ���+ �߼���λͼռ�õĿ���+
	// ( i �ڵ��-1 )/ÿ�麬�е� i �ڵ���

	block = 2 + 
			sb->s_imap_blocks + 
			sb->s_zmap_blocks + ( inode->i_num - 1 ) / INODES_PER_BLOCK;
			
	// ���豸�϶�ȡ�� i �ڵ����ڵ��߼���
	if ( !( bh = bread( inode->i_dev, block ) ) )
	{
		panic( "unable to read i-node block" );
	}

	// ���� i �ڵ���Ϣ���Ƶ��߼����Ӧ�� i �ڵ������
	( ( D_Inode * )bh->b_data )[ ( inode->i_num - 1 ) % INODES_PER_BLOCK ] = *( D_Inode * )inode;

	// �û��������޸ı�־,��i �ڵ��޸ı�־����.Ȼ���ͷŸú���i �ڵ�Ļ�����,��������i �ڵ�.
	bh->b_dirt		= 1;
	inode->i_dirt	= 0;

	brelse( bh );

	unlock_inode( inode );
}
