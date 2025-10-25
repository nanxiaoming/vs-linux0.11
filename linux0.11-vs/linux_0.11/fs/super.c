/*
*  linux/fs/super.c
*
*  ( C ) 1991  Linus Torvalds
*/

/*
* super.c contains code to handle the super-block tables.
*/
#include <linux\config.h>
#include <linux\sched.h>
#include <linux\kernel.h>
#include <asm\system.h>

#include <errno.h>
#include <sys\stat.h>

LONG sync_dev( LONG dev );
VOID wait_for_keypress();


/* set_bit uses setb, as gas doesn't recognize setc */
static __inline LONG set_bit( LONG bitnr, VOID *addr )
{
	register LONG __res;

	__asm	xor		eax, eax
	__asm	mov		edi, addr
	__asm	mov		edx, bitnr
	__asm	bt		DWORD PTR[ edi ], edx
	__asm	setb	al
	__asm	mov		__res, eax

	return __res;
}

// ������ṹ����( ��8�� )
Super_Block super_block[ NR_SUPER ];
/* this is initialized in init/main.c */
LONG ROOT_DEV = 0;

// ����ָ���ĳ�����
static VOID lock_super( Super_Block * sb )
{
	cli();

	while ( sb->s_lock )
	{
		sleep_on( &( sb->s_wait ) );
	}
	sb->s_lock = 1;

	sti();
}

// ��ָ�����������.( ���ʹ��ulock_super �������������� )
static VOID free_super( Super_Block * sb )
{
	cli();

	sb->s_lock = 0;

	wake_up( &( sb->s_wait ) );

	sti();
}

// ˯�ߵȴ����������
static VOID wait_on_super( Super_Block * sb )
{
	cli();

	while ( sb->s_lock )
	{
		sleep_on( &( sb->s_wait ) );
	}

	sti();
}

// ȡָ���豸�ĳ�����.���ظó�����ṹָ��
Super_Block *get_super( LONG dev )
{
	Super_Block * s;

	if ( !dev )
	{
		return NULL;
	}

	s = 0 + super_block;

	// �����ǰ��������ָ���豸�ĳ�����,�����ȵȴ��ó��������( ���Ѿ����������������Ļ� ).
	// �ڵȴ��ڼ�,�ó������п��ܱ������豸ʹ��,��˴�ʱ�����ж�һ���Ƿ���ָ���豸�ĳ�����,
	// ������򷵻ظó������ָ��.��������¶Գ���������������һ��,���s ����ָ�򳬼�������
	// ��ʼ��
	while ( s < NR_SUPER + super_block )
	{
		if ( s->s_dev == dev ) 
		{
			wait_on_super( s );

			if ( s->s_dev == dev )
			{
				return s;
			}
			s = 0 + super_block;
		}
		else
		{
			s++;
		}
	}
	return NULL;
}

// �ͷ�ָ���豸�ĳ�����.
// �ͷ��豸��ʹ�õĳ�����������( ��s_dev=0 ),���ͷŸ��豸i �ڵ�λͼ���߼���λͼ��ռ��
// �ĸ��ٻ����.����������Ӧ���ļ�ϵͳ�Ǹ��ļ�ϵͳ,������i �ڵ����Ѿ���װ���������ļ�
// ϵͳ,�����ͷŸó�����
VOID put_super( LONG dev )
{
	Super_Block *	sb;
	LONG			i;

	if ( dev == ROOT_DEV )
	{
		printk( "root diskette changed: prepare for armageddon\n\r" );
		return;
	}

	if ( !( sb = get_super( dev ) ) )
	{
		return;
	}

	if ( sb->s_imount ) 
	{
		printk( "Mounted disk changed - tssk, tssk\n\r" );
		return;
	}

	lock_super( sb );
	sb->s_dev = 0;

	for ( i = 0; i < I_MAP_SLOTS; i++ )
	{
		brelse( sb->s_imap[ i ] );
	}
	for ( i = 0; i < Z_MAP_SLOTS; i++ )
	{
		brelse( sb->s_zmap[ i ] );
	}

	free_super( sb );
	return;
}

static Super_Block *read_super( LONG dev )
{
	Super_Block		*	s	;
	Buffer_Head		*	bh	;
	LONG				i, block;

	if ( !dev )
	{
		return NULL;
	}

	// ���ȼ����豸�Ƿ�ɸ�������Ƭ( Ҳ���Ƿ��������豸 ),�����������,����ٻ������йظ�
	// �豸�����л�����ʧЧ,��Ҫ����ʧЧ����( �ͷ�ԭ�����ص��ļ�ϵͳ )
	check_disk_change( dev );

	// ������豸�ĳ������Ѿ��ڸ��ٻ�����,��ֱ�ӷ��ظó������ָ��
	if ( s = get_super( dev ) )
	{
		return s;
	}

	// ����,�����ڳ������������ҳ�һ������( Ҳ���� s_dev = 0 ���� ).��������Ѿ�ռ���򷵻ؿ�ָ��.
	for ( s = 0 + super_block ;; s++ ) 
	{
		if ( s >= NR_SUPER + super_block )
		{
			return NULL;
		}
		if ( !s->s_dev )
		{
			break;
		}
	}

	// �ҵ�����������,�ͽ��ó���������ָ���豸,�Ըó�������ڴ�����в��ֳ�ʼ��
	s->s_dev		= (USHORT)dev;
	s->s_isup		= NULL;
	s->s_imount		= NULL;
	s->s_time		= 0;
	s->s_rd_only	= 0;
	s->s_dirt		= 0;

	// Ȼ�������ó�����,�����豸�϶�ȡ��������Ϣ��bh ָ��Ļ�������.��������������ʧ��,
	// ���ͷ�����ѡ���ĳ����������е���,����������,���ؿ�ָ���˳�
	lock_super( s );

	if ( !( bh = bread( dev, 1 ) ) ) 
	{
		s->s_dev = 0;
		free_super( s );
		return NULL;
	}

	// ���豸�϶�ȡ�ĳ�������Ϣ���Ƶ�������������Ӧ��ṹ��.���ͷŴ�Ŷ�ȡ��Ϣ�ĸ��ٻ����.
	*( ( D_Super_Block* )s ) = *( ( D_Super_Block* )bh->b_data );
		
	brelse( bh );

	// �����ȡ�ĳ�������ļ�ϵͳħ���ֶ����ݲ���,˵���豸�ϲ�����ȷ���ļ�ϵͳ,���ͬ����
	// һ��,�ͷ�����ѡ���ĳ����������е���,����������,���ؿ�ָ���˳�.
	// ���ڸð�linux �ں�,ֻ֧��minix �ļ�ϵͳ�汾1.0,��ħ����0x137f
	if ( s->s_magic != SUPER_MAGIC ) 
	{
		s->s_dev = 0;
		free_super( s );
		return NULL;
	}
	// ���濪ʼ��ȡ�豸��i �ڵ�λͼ���߼���λͼ����.���ȳ�ʼ���ڴ泬����ṹ��λͼ�ռ�
	for ( i = 0; i < I_MAP_SLOTS; i++ )
	{
		s->s_imap[ i ] = NULL;
	}

	for ( i = 0; i < Z_MAP_SLOTS; i++ )
	{
		s->s_zmap[ i ] = NULL;
	}

	// Ȼ����豸�϶�ȡi �ڵ�λͼ���߼���λͼ��Ϣ,������ڳ������Ӧ�ֶ���
	block = 2;

	// ���������λͼ�߼�����������λͼӦ��ռ�е��߼�����,˵���ļ�ϵͳλͼ��Ϣ������,������
	// ��ʼ��ʧ��.���ֻ���ͷ�ǰ�������������Դ,���ؿ�ָ�벢�˳�
	for ( i = 0; i < s->s_imap_blocks; i++ )
	{
		if ( s->s_imap[ i ] = bread( dev, block ) )
		{
			block++;
		}
		else
		{
			break;
		}
	}
	for ( i = 0; i < s->s_zmap_blocks; i++ )
	{
		if ( s->s_zmap[ i ] = bread( dev, block ) )
		{
			block++;
		}
		else
		{
			break;
		}
	}

	if ( block != 2 + s->s_imap_blocks + s->s_zmap_blocks ) 
	{
		// �ͷ�i �ڵ�λͼ���߼���λͼռ�õĸ��ٻ�����
		for ( i = 0; i < I_MAP_SLOTS; i++ )
		{
			brelse( s->s_imap[ i ] );
		}
		for ( i = 0; i < Z_MAP_SLOTS; i++ )
		{
			brelse( s->s_zmap[ i ] );
		}
		//�ͷ�����ѡ���ĳ����������е���,�������ó�������,���ؿ�ָ���˳�
		s->s_dev = 0;
		free_super( s );
		return NULL;
	}

	// ����һ�гɹ�.����������� i �ڵ�ĺ�������,����豸�����е�i �ڵ��Ѿ�ȫ��ʹ��,�����
	// �����᷵��0 ֵ.���0 ��i �ڵ��ǲ����õ�,�������ｫλͼ�е����λ����Ϊ1,�Է�ֹ�ļ�
	// ϵͳ����0 ��i �ڵ�.ͬ���ĵ���,Ҳ���߼���λͼ�����λ����Ϊ1

	s->s_imap[ 0 ]->b_data[ 0 ] |= 1;
	s->s_zmap[ 0 ]->b_data[ 0 ] |= 1;

	free_super( s );

	return s;
}

// ж���ļ�ϵͳ��ϵͳ���ú���.
// ����dev_name ���豸�ļ���
LONG sys_umount( CHAR * dev_name )
{
	M_Inode			*	inode;
	Super_Block		*	sb;
	LONG				dev;

	// ���ȸ����豸�ļ����ҵ���Ӧ�� i �ڵ�,��ȡ���е��豸��
	if ( !( inode = namei( dev_name ) ) )
	{
		return -ENOENT;
	}

	dev = inode->i_zone[ 0 ];

	// ������ǿ��豸�ļ�,���ͷŸ������i �ڵ�dev_i,���س�����
	if ( !S_ISBLK( inode->i_mode ) ) 
	{
		iput( inode );
		return -ENOTBLK;
	}

	// �ͷ��豸�ļ�����i �ڵ�
	iput( inode );

	// ����豸�Ǹ��ļ�ϵͳ,���ܱ�ж��,���س����
	if ( dev == ROOT_DEV )
	{
		return -EBUSY;
	}
	// ���ȡ�豸�ĳ�����ʧ��,���߸��豸�ļ�ϵͳû�а�װ��,�򷵻س�����
	if ( !( sb = get_super( dev ) ) || !( sb->s_imount ) )
	{
		return -ENOENT;
	}
	// �����������ָ���ı���װ����i �ڵ�û����λ�䰲װ��־,����ʾ������Ϣ
	if ( !sb->s_imount->i_mount )
	{
		printk( "Mounted inode has i_mount=0\n" );
	}

	// ����i �ڵ��,���Ƿ��н�����ʹ�ø��豸�ϵ��ļ�,������򷵻�æ������
	for ( inode = inode_table + 0; inode < inode_table + NR_INODE; inode++ )
	{
		if ( inode->i_dev == dev && inode->i_count )
		{
			return -EBUSY;
		}
	}

	// ��λ����װ����i �ڵ�İ�װ��־,�ͷŸ�i �ڵ�
	sb->s_imount->i_mount = 0;

	iput( sb->s_imount );

	// �ó������б���װi �ڵ��ֶ�Ϊ��,���ͷ��豸�ļ�ϵͳ�ĸ�i �ڵ�,�ó������б���װϵͳ
	// �� i �ڵ�ָ��Ϊ��
	sb->s_imount = NULL;

	iput( sb->s_isup );
	sb->s_isup = NULL;

	put_super( dev );
	sync_dev ( dev );

	return 0;
}

// ��װ�ļ�ϵͳ���ú���.
// ����dev_name ���豸�ļ���,dir_name �ǰ�װ����Ŀ¼��,rw_flag ����װ�ļ��Ķ�д��־.
// �������صĵط�������һ��Ŀ¼��,���Ҷ�Ӧ��i �ڵ�û�б���������ռ��

LONG sys_mount( CHAR * dev_name, CHAR * dir_name, LONG rw_flag )
{
	M_Inode			*	dev_i, *dir_i;
	Super_Block		*	sb;
	LONG				dev;

	// ���ȸ����豸�ļ����ҵ���Ӧ��i �ڵ�,��ȡ���е��豸��.
	// ���ڿ������豸�ļ�,�豸����i �ڵ��i_zone[ 0 ]��
	if ( !( dev_i = namei( dev_name ) ) )
	{
		return -ENOENT;
	}

	dev = dev_i->i_zone[ 0 ];

	// ������ǿ��豸�ļ�,���ͷŸ�ȡ�õ� i �ڵ� dev_i ,���س�����.
	if ( !S_ISBLK( dev_i->i_mode ) ) 
	{
		iput( dev_i );

		return -EPERM;
	}

	// �ͷŸ��豸�ļ���i �ڵ�dev_i
	iput( dev_i );

	// ���ݸ�����Ŀ¼�ļ����ҵ���Ӧ��i �ڵ�dir_i
	if ( !( dir_i = namei( dir_name ) ) )
	{
		return -ENOENT;
	}

	// �����i �ڵ�����ü�����Ϊ1( ������������ ),���߸�i �ڵ�Ľڵ���Ǹ��ļ�ϵͳ�Ľڵ�
	// ��1,���ͷŸ�i �ڵ�,���س�����

	if ( dir_i->i_count != 1 || dir_i->i_num == ROOT_INO ) 
	{
		iput( dir_i );
		return -EBUSY;
	}

	if ( !S_ISDIR( dir_i->i_mode ) ) 
	{
		iput( dir_i );
		return -EPERM;
	}

	// ��ȡ����װ�ļ�ϵͳ�ĳ�����,���ʧ����Ҳ�ͷŸ�i �ڵ�,���س�����
	if ( !( sb = read_super( dev ) ) )
	{
		iput( dir_i );
		return -EBUSY;
	}

	// �����Ҫ����װ���ļ�ϵͳ�Ѿ���װ�������ط�,���ͷŸ�i �ڵ�,���س�����
	if ( sb->s_imount ) 
	{
		iput( dir_i );
		return -EBUSY;
	}

	// �����Ҫ��װ����i �ڵ��Ѿ���װ���ļ�ϵͳ( ��װ��־�Ѿ���λ ),���ͷŸ�i �ڵ�,���س�����.
	if ( dir_i->i_mount ) 
	{
		iput( dir_i );
		return -EPERM;
	}

	// ����װ�ļ�ϵͳ�������"����װ��i �ڵ�"�ֶ�ָ��װ����Ŀ¼����i �ڵ�
	sb->s_imount	= dir_i;
	// ���ð�װλ��i �ڵ�İ�װ��־�ͽڵ����޸ı�־
	dir_i->i_mount	= 1;
	dir_i->i_dirt	= 1;		/* NOTE! we don't iput( dir_i ) */
	return 0;			/* we do that in umount */
}

VOID mount_root()
/*++

Routine Description:

	���ظ��ļ�ϵͳ.��sys_setup����.MINIX 1.0 �ļ�ϵͳ��ʽ����:

	+--------+--------+--------+--------+--------+--------+--------+--------+--------+-----------
	|		 |		  |		   |		|		 |		  |		   |		|		 |
	|  BOOT  |	Super |	inode  | Block	| inode  |	inode | ...    | Block	| Block	 | ...
	|        |	Block |	BitMap | BitMap	|	0	 |	  1	  |		   |   0	|   1	 |
	|		 |		  |		   |		|		 |		  |		   |		|		 |
	+--------+--------+--------+--------+--------+--------+--------+--------+--------+-----------

	SuperBlock  s_ninodes			i�ڵ���
				s_nzones			�߼�����
				s_imap_blocks		i�ڵ�λͼ��ռ����
				s_zmap_blocks		�߼���λͼ��ռ����
				s_firstdatazone  	��������һ���߼�����


	inode       i_mode				�ļ������ͺ�����
		        i_uid				�ļ��������û�id 
		        i_size				�ļ����ȣ��ֽڣ�
		        i_mtime				�޸�ʱ�䣨��1970.1.1:0ʱ�����룩
		        i_gid				�ļ���������id 
		        i_nlinks			���������ж��ٸ��ļ�Ŀ¼��ָ���i�ڵ㣩
                i_zone[ 9 ]		    �ļ���ռ�õ������߼��������


	Դ������ 8191 Ϊ 0x1FFFF ,����ÿ������2���� 1K ��С,���һ������ 8192 ��Bit

	set_bit( i & 8191, p->s_zmap[ i >> 13 ]->b_data ) �������ö�Ӧ�Ŀ��bitλ

	mi=iget(0,1); ���inode��Ϊ����1�Ĺ���Ŀ¼

Arguments:
	-
Return Value:
	-
--*/
{
	LONG				i, free;
	Super_Block		*	p;
	M_Inode			*	mi;

	// ��ʼ���ļ�������( ��64 ��,Ҳ��ϵͳͬʱֻ�ܴ�64 ���ļ� ),�������ļ��ṹ�е����ü���
	// ����Ϊ0.[ ??Ϊʲô���������ʼ��? ]

	for ( i = 0; i < NR_FILE; i++ )
	{
		file_table[ i ].f_count = 0;
	}

	// ������ļ�ϵͳ�����豸�����̵Ļ�,����ʾ"������ļ�ϵͳ��,�����س���",���ȴ�����.

	if ( MAJOR( ROOT_DEV ) == 2 ) 
	{
		printk( "Insert root floppy and press ENTER" );
		wait_for_keypress();
	}

	// ��ʼ������������( �� 8 �� )
	for ( p = &super_block[ 0 ]; p < &super_block[ NR_SUPER ]; p++ )
	{
		p->s_dev	= 0;
		p->s_lock	= 0;
		p->s_wait	= NULL;
	}

	// ��������豸�ϳ�����ʧ��,����ʾ��Ϣ,������
	if ( !( p = read_super( ROOT_DEV ) ) )
	{
		panic( "Unable to mount root" );
	}

	//���豸�϶�ȡ�ļ�ϵͳ�ĸ� i �ڵ�( 1 ),���ʧ������ʾ������Ϣ,����
	if ( !( mi = iget( ROOT_DEV, ROOT_INO ) ) )
	{
		panic( "Unable to read root i-node" );
	}

	// ��i �ڵ����ô�������3 ��.��Ϊ����266-268 ����Ҳ�����˸�i �ڵ�
	/* ע�⣡���߼��Ͻ�,���ѱ�������4 ��,������ 1 �� */
	// �øó�����ı���װ�ļ�ϵͳi �ڵ�ͱ���װ����i �ڵ�Ϊ�� i �ڵ�.

	mi->i_count	   += 3;	/* NOTE! it is logically used 4 times, not 1 */
	p->s_isup		= p->s_imount = mi;
	current->pwd	= mi;
	current->root	= mi;
	free			= 0;
	i				= p->s_nzones;

	// Ȼ������߼���λͼ����Ӧ����λ��ռ�����ͳ�Ƴ����п���.����꺯��set_bit()ֻ���ڲ���
	// ����λ,�������ñ���λ."i&8191"����ȡ��i �ڵ���ڵ�ǰ���е�ƫ��ֵ."i>>13"�ǽ�i ����
	// 8192,Ҳ����һ�����̿�����ı���λ�� 8191<->0x1FFFF
	while ( --i >= 0 )
	{
		if ( !set_bit( i & 8191, p->s_zmap[ i >> 13 ]->b_data ) )
		{
			free++;
		}
	}

	// ��ʾ�豸�Ͽ����߼�����/�߼�������
	printk( "%d/%d free blocks\n\r", free, p->s_nzones );

	// ͳ���豸�Ͽ��� i �ڵ���.������ i ���ڳ������б������豸�� i �ڵ�����+1.�� 1 �ǽ� 0 �ڵ�
	// Ҳͳ�ƽ�ȥ

	free = 0;
	i	 = p->s_ninodes + 1;

	// Ȼ�����i �ڵ�λͼ����Ӧ����λ��ռ��������������i �ڵ���
	while ( --i >= 0 )
	{
		if ( !set_bit( i & 8191, p->s_imap[ i >> 13 ]->b_data ) )
		{
			free++;
		}
	}
	printk( "%d/%d free inodes\n\r", free, p->s_ninodes );
}
