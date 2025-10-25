/*
 *  linux/fs/bitmap.c
 *
 *  ( C ) 1991  Linus Torvalds
 */

/* bitmap.c contains the code that handles the inode and block bitmaps */
#include <string.h>

#include <linux\sched.h>
#include <linux\kernel.h>

// ��ָ����ַ( addr )����һ���ڴ�����.Ƕ��������.
// ����:eax = 0,ecx = ���ݿ��СBLOCK_SIZE/4,edi = addr.
static __inline VOID clear_block( VOID *addr )
{
	__asm mov	edi, addr
	__asm mov	ecx, BLOCK_SIZE / 4
	__asm xor	eax, eax
	__asm cld
	__asm rep	stosd
}

// ��λָ����ַ��ʼ�ĵ�nr ��λƫ�ƴ��ı���λ( nr ���Դ���32�� ).����ԭ����λ( 0 �� 1 ).
// ����:%0 - eax( ����ֵ ),%1 - eax( 0 );%2 - nr,λƫ��ֵ;%3 - ( addr ),addr ������.
static __inline LONG set_bit( LONG nr, VOID *addr )
{
	register LONG res;

	__asm xor	eax, eax
	__asm mov	edi, addr
	__asm mov	edx, nr
	__asm bts	DWORD PTR[ edi ], edx
	__asm setb	al
	__asm mov	res, eax

	return res;
}

// ��λָ����ַ��ʼ�ĵ�nr λƫ�ƴ��ı���λ.����ԭ����λ�ķ���( 1 ��0 ).
// ����:%0 - eax( ����ֵ ),%1 - eax( 0 );%2 - nr,λƫ��ֵ;%3 - ( addr ),addr ������.
static __inline LONG clear_bit( LONG nr, VOID *addr )
{
	register LONG res;

	__asm xor	eax, eax
	__asm mov	edi, addr
	__asm mov	edx, nr
	__asm btr	DWORD PTR[ edi ], edx
	__asm setnb	al
	__asm mov	res, eax

	return res;
}

// ��addr ��ʼѰ�ҵ�1 ��0 ֵ����λ.
// ����:%0 - ecx( ����ֵ );%1 - ecx( 0 );%2 - esi( addr ).
// ��addr ָ����ַ��ʼ��λͼ��Ѱ�ҵ�1 ����0 �ı���λ,���������addr �ı���λƫ��ֵ����.
static __inline LONG find_first_zero( VOID *addr )
{
	LONG __res;

	__asm xor	ecx, ecx
	__asm mov	esi, addr
	__asm cld				//�巽��λ
LN1 :						//
	__asm lodsd				//ȡ[ esi ] -> eax 
	__asm not	eax			//eax ��ÿλȡ�� 
	__asm bsf	edx, eax	//��λ0 ɨ��eax ����1 �ĵ�1 ��λ,��ƫ��ֵ -> edx (Bit Scan Forward) 
	__asm je	LN2			//���eax ��ȫ��0,����ǰ��ת�����2 ��( 40 �� ) 
	__asm add	ecx, edx	//ƫ��ֵ����ecx( ecx ����λͼ���׸���0 �ı���λ��ƫ��ֵ ) 
	__asm jmp	LN3			//��ǰ��ת�����3 ��( ���� ) 
LN2 :						//
	__asm add	ecx, 32		//û���ҵ�0 ����λ,��ecx ����1 �����ֵ�λƫ����32
	__asm cmp	ecx, 8192	//�Ѿ�ɨ����8192 λ( 1024 �ֽ� )����? 
	__asm jl	LN1			//����û��ɨ����1 ������,����ǰ��ת�����1 ��,����
LN3 :
	__asm mov	__res, ecx	//��ʱecx ����λƫ����

	return __res;
}

//
// �ͷ��豸dev ���������е��߼���block.
// ��λָ���߼���block ���߼���λͼ����λ.
// ����:dev ���豸��,block ���߼����( �̿�� ).
//
VOID free_block( LONG dev, LONG block )
{
	Super_Block * sb;
	Buffer_Head * bh;

	// ȡָ���豸dev �ĳ�����,���ָ���豸������,���������.
	if ( !( sb = get_super( dev ) ) )
	{
		panic( "trying to free block on nonexistent device" );
	}

	// ���߼����С���׸��߼���Ż��ߴ����豸�����߼�����,�����,����.
	if ( block < sb->s_firstdatazone || block >= sb->s_nzones )
	{
		panic( "trying to free block not in datazone" );
	}

	// ��hash ����Ѱ�Ҹÿ�����.���ҵ������ж�����Ч��,�������޸ĺ͸��±�־,�ͷŸ����ݿ�.
	// �öδ������Ҫ��;��������߼��鵱ǰ�����ڸ��ٻ�����,���ͷŶ�Ӧ�Ļ����.
	bh = get_hash_table( dev, block );

	if ( bh )
	{
		if ( bh->b_count != 1 ) 
		{
			printk( "trying to free block ( %04x:%d ), count=%d\n",dev, block, bh->b_count );
			return;
		}
		bh->b_dirt		= 0;	// ��λ��( ���޸� )��־λ.
		bh->b_uptodate	= 0;	// ��λ���±�־.

		brelse( bh );
	}

	// ����block ����������ʼ����������߼����( ��1 ��ʼ���� ).Ȼ����߼���( ���� )λͼ���в���,
	// ��λ��Ӧ�ı���λ.����Ӧ����λԭ������0,�����,����.
	block -= sb->s_firstdatazone - 1;

	if ( clear_bit( block & 8191, sb->s_zmap[ block / 8192 ]->b_data ) )
	{
		printk( "block ( %04x:%d ) ", dev, block + sb->s_firstdatazone - 1 );
		panic( "free_block: bit already cleared" );
	}
	// ����Ӧ�߼���λͼ���ڻ��������޸ı�־.
	sb->s_zmap[ block / 8192 ]->b_dirt = 1;
}

LONG new_block( LONG dev )
/*++

Routine Description:

	���豸 dev ����һ���߼���( �̿�,���� ).

Arguments:

	dev - �豸��

Return Value:

	�߼����( �̿�� )

--*/
{
	Buffer_Head		*	bh;
	Super_Block		*	sb;
	LONG				i, j;

	// ���豸dev ȡ������,���ָ���豸������,���������
	if ( !( sb = get_super( dev ) ) )
	{
		panic( "trying to get new block from nonexistant device" );
	}

	// ɨ���߼���λͼ,Ѱ���׸� 0 ����λ,Ѱ�ҿ����߼���,��ȡ���ø��߼���Ŀ��
	j = 8192;

	for ( i = 0; i < 8; i++ )
	{
		if ( bh = sb->s_zmap[ i ] )
		{
			if ( ( j = find_first_zero( bh->b_data ) ) < 8192 )
			{
				break;
			}
		}
	}

	// ���ȫ��ɨ���껹û�ҵ�( i >= 8 �� j >= 8192 )����λͼ���ڵĻ������Ч( bh=NULL )�򷵻�0,
	// �˳�( û�п����߼��� )
	if ( i >= 8 || !bh || j >= 8192 )
	{
		return 0;
	}

	// �������߼����Ӧ�߼���λͼ�еı���λ,����Ӧ����λ�Ѿ���λ,�����,����
	if ( set_bit( j, bh->b_data ) )
	{
		panic( "new_block: bit already set" );
	}

	// �ö�Ӧ������������޸ı�־.������߼�����ڸ��豸�ϵ����߼�����,��˵��ָ���߼�����
	// ��Ӧ�豸�ϲ�����.����ʧ��,����0,�˳�
	bh->b_dirt = 1;

	j += i * 8192 + sb->s_firstdatazone - 1;

	if ( j >= sb->s_nzones )
	{
		return 0;
	}

	// ��ȡ�豸�ϵĸ����߼�������( ��֤ ).���ʧ��������
	if ( !( bh = getblk( dev, j ) ) )
	{
		panic( "new_block: cannot get block" );
	}

	// �¿�����ü���ӦΪ1.��������
	if ( bh->b_count != 1 )
	{
		panic( "new block: count is != 1" );
	}

	// �������߼�������,����λ���±�־�����޸ı�־.Ȼ���ͷŶ�Ӧ������,�����߼����
	clear_block( bh->b_data );

	bh->b_uptodate	= 1;
	bh->b_dirt		= 1;

	brelse( bh );

	return j;
}

// �ͷ�ָ����i �ڵ�.
// ��λ��Ӧi �ڵ�λͼ����λ.
VOID free_inode( M_Inode * inode )
{
	Super_Block * sb;
	Buffer_Head * bh;

	// ��� i �ڵ�ָ�� =NULL ,���˳�.
	if ( !inode )
	{
		return;
	}

	// ���i �ڵ��ϵ��豸���ֶ�Ϊ0,˵���ýڵ�����,����0 ��ն�Ӧi �ڵ���ռ�ڴ���,������.
	if ( !inode->i_dev )
	{
		memset( inode, 0, sizeof( *inode ) );
		return;
	}

	// �����i �ڵ㻹��������������,�����ͷ�,˵���ں�������,����
	if ( inode->i_count > 1 ) 
	{
		printk( "trying to free inode with count=%d\n", inode->i_count );
		panic( "free_inode" );
	}

	// ����ļ�Ŀ¼����������Ϊ0,���ʾ���������ļ�Ŀ¼����ʹ�øýڵ�,
	// ��Ӧ�ͷ�,��Ӧ�÷Żص�.
	if ( inode->i_nlinks )
	{
		panic( "trying to free inode with links" );
	}

	// ȡi �ڵ������豸�ĳ�����,�����豸�Ƿ����.
	if ( !( sb = get_super( inode->i_dev ) ) )
	{
		panic( "trying to free inode on nonexistent device" );
	}

	// ���i �ڵ��=0 ����ڸ��豸��i �ڵ�����,�����( 0 ��i �ڵ㱣��û��ʹ�� )
	if ( inode->i_num < 1 || inode->i_num > sb->s_ninodes )
	{
		panic( "trying to free inode 0 or nonexistant inode" );
	}

	// �����i �ڵ��Ӧ�Ľڵ�λͼ������,�����.
	if ( !( bh = sb->s_imap[ inode->i_num >> 13 ] ) )
	{
		panic( "nonexistent imap in superblock" );
	}

	if ( clear_bit( inode->i_num & 8191, bh->b_data ) )
	{
		printk( "free_inode: bit already cleared.\n\r" );
	}

	// ��i �ڵ�λͼ���ڻ��������޸ı�־,����ո�i �ڵ�ṹ��ռ�ڴ���

	bh->b_dirt = 1;

	memset( inode, 0, sizeof( *inode ) );
}

// Ϊ�豸dev ����һ����i �ڵ�.���ظ���i �ڵ��ָ��.
// ���ڴ�i �ڵ���л�ȡһ������i �ڵ����,����i �ڵ�λͼ����һ������i �ڵ�.
M_Inode * new_inode( LONG dev )
{
	M_Inode			*	inode;
	Super_Block		*	sb;
	Buffer_Head		*	bh;
	LONG				i, j;

	// ���ڴ�i �ڵ��( inode_table )�л�ȡһ������i �ڵ���( inode ).
	if ( !( inode = get_empty_inode() ) )
	{
		return NULL;
	}

	// ��ȡָ���豸�ĳ�����ṹ.
	if ( !( sb = get_super( dev ) ) )
	{
		panic( "new_inode with unknown device" );
	}

	// ɨ��i �ڵ�λͼ,Ѱ���׸�0 ����λ,Ѱ�ҿ��нڵ�,��ȡ���ø�i �ڵ�Ľڵ��
	j = 8192;

	for ( i = 0; i < 8; i++ )
	{
		if ( bh = sb->s_imap[ i ] )
		{
			if ( ( j = find_first_zero( bh->b_data ) )<8192 )
			{
				break;
			}
		}
	}

	// ���ȫ��ɨ���껹û�ҵ�,����λͼ���ڵĻ������Ч( bh=NULL )�򷵻�0,�˳�( û�п���i �ڵ� ).

	if ( !bh || j >= 8192 || j + i * 8192 > sb->s_ninodes )
	{
		iput( inode );
		return NULL;
	}

	// ��λ��Ӧ��i �ڵ��i �ڵ�λͼ��Ӧ����λ,����Ѿ���λ,�����
	if ( set_bit( j, bh->b_data ) )
	{
		panic( "new_inode: bit already set" );
	}

	// ��i �ڵ�λͼ���ڻ��������޸ı�־
	bh->b_dirt = 1;

	// ��ʼ����i �ڵ�ṹ
	inode->i_count	= 1;						// ���ü���.
	inode->i_nlinks = 1;						// �ļ�Ŀ¼��������.
	inode->i_dev	= (USHORT)dev;				// i �ڵ����ڵ��豸��.
	inode->i_uid	= current->euid;			// i �ڵ������û�id.
	inode->i_gid	= ( UCHAR )current->egid;	// ��id 
	inode->i_dirt	= 1;						// ���޸ı�־��λ.
	inode->i_num	= (USHORT)(j + i * 8192);	// ��Ӧ�豸�е�i �ڵ��.
	inode->i_mtime	= inode->i_atime = inode->i_ctime = CURRENT_TIME;


	return inode;					// ���ظ�i �ڵ�ָ��
}
