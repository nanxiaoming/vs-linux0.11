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

// 超级块结构数组( 共8项 )
Super_Block super_block[ NR_SUPER ];
/* this is initialized in init/main.c */
LONG ROOT_DEV = 0;

// 锁定指定的超级块
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

// 对指定超级块解锁.( 如果使用ulock_super 这个名称则更妥帖 )
static VOID free_super( Super_Block * sb )
{
	cli();

	sb->s_lock = 0;

	wake_up( &( sb->s_wait ) );

	sti();
}

// 睡眠等待超级块解锁
static VOID wait_on_super( Super_Block * sb )
{
	cli();

	while ( sb->s_lock )
	{
		sleep_on( &( sb->s_wait ) );
	}

	sti();
}

// 取指定设备的超级块.返回该超级块结构指针
Super_Block *get_super( LONG dev )
{
	Super_Block * s;

	if ( !dev )
	{
		return NULL;
	}

	s = 0 + super_block;

	// 如果当前搜索项是指定设备的超级块,则首先等待该超级块解锁( 若已经被其它进程上锁的话 ).
	// 在等待期间,该超级块有可能被其它设备使用,因此此时需再判断一次是否是指定设备的超级块,
	// 如果是则返回该超级块的指针.否则就重新对超级块数组再搜索一遍,因此s 重又指向超级块数组
	// 开始处
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

// 释放指定设备的超级块.
// 释放设备所使用的超级块数组项( 置s_dev=0 ),并释放该设备i 节点位图和逻辑块位图所占用
// 的高速缓冲块.如果超级块对应的文件系统是根文件系统,或者其i 节点上已经安装有其它的文件
// 系统,则不能释放该超级块
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

	// 首先检查该设备是否可更换过盘片( 也即是否是软盘设备 ),如果更换过盘,则高速缓冲区有关该
	// 设备的所有缓冲块均失效,需要进行失效处理( 释放原来加载的文件系统 )
	check_disk_change( dev );

	// 如果该设备的超级块已经在高速缓冲中,则直接返回该超级块的指针
	if ( s = get_super( dev ) )
	{
		return s;
	}

	// 否则,首先在超级块数组中找出一个空项( 也即其 s_dev = 0 的项 ).如果数组已经占满则返回空指针.
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

	// 找到超级块空项后,就将该超级块用于指定设备,对该超级块的内存项进行部分初始化
	s->s_dev		= (USHORT)dev;
	s->s_isup		= NULL;
	s->s_imount		= NULL;
	s->s_time		= 0;
	s->s_rd_only	= 0;
	s->s_dirt		= 0;

	// 然后锁定该超级块,并从设备上读取超级块信息到bh 指向的缓冲区中.如果读超级块操作失败,
	// 则释放上面选定的超级块数组中的项,并解锁该项,返回空指针退出
	lock_super( s );

	if ( !( bh = bread( dev, 1 ) ) ) 
	{
		s->s_dev = 0;
		free_super( s );
		return NULL;
	}

	// 将设备上读取的超级块信息复制到超级块数组相应项结构中.并释放存放读取信息的高速缓冲块.
	*( ( D_Super_Block* )s ) = *( ( D_Super_Block* )bh->b_data );
		
	brelse( bh );

	// 如果读取的超级块的文件系统魔数字段内容不对,说明设备上不是正确的文件系统,因此同上面
	// 一样,释放上面选定的超级块数组中的项,并解锁该项,返回空指针退出.
	// 对于该版linux 内核,只支持minix 文件系统版本1.0,其魔数是0x137f
	if ( s->s_magic != SUPER_MAGIC ) 
	{
		s->s_dev = 0;
		free_super( s );
		return NULL;
	}
	// 下面开始读取设备上i 节点位图和逻辑块位图数据.首先初始化内存超级块结构中位图空间
	for ( i = 0; i < I_MAP_SLOTS; i++ )
	{
		s->s_imap[ i ] = NULL;
	}

	for ( i = 0; i < Z_MAP_SLOTS; i++ )
	{
		s->s_zmap[ i ] = NULL;
	}

	// 然后从设备上读取i 节点位图和逻辑块位图信息,并存放在超级块对应字段中
	block = 2;

	// 如果读出的位图逻辑块数不等于位图应该占有的逻辑块数,说明文件系统位图信息有问题,超级块
	// 初始化失败.因此只能释放前面申请的所有资源,返回空指针并退出
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
		// 释放i 节点位图和逻辑块位图占用的高速缓冲区
		for ( i = 0; i < I_MAP_SLOTS; i++ )
		{
			brelse( s->s_imap[ i ] );
		}
		for ( i = 0; i < Z_MAP_SLOTS; i++ )
		{
			brelse( s->s_zmap[ i ] );
		}
		//释放上面选定的超级块数组中的项,并解锁该超级块项,返回空指针退出
		s->s_dev = 0;
		free_super( s );
		return NULL;
	}

	// 否则一切成功.对于申请空闲 i 节点的函数来讲,如果设备上所有的i 节点已经全被使用,则查找
	// 函数会返回0 值.因此0 号i 节点是不能用的,所以这里将位图中的最低位设置为1,以防止文件
	// 系统分配0 号i 节点.同样的道理,也将逻辑块位图的最低位设置为1

	s->s_imap[ 0 ]->b_data[ 0 ] |= 1;
	s->s_zmap[ 0 ]->b_data[ 0 ] |= 1;

	free_super( s );

	return s;
}

// 卸载文件系统的系统调用函数.
// 参数dev_name 是设备文件名
LONG sys_umount( CHAR * dev_name )
{
	M_Inode			*	inode;
	Super_Block		*	sb;
	LONG				dev;

	// 首先根据设备文件名找到对应的 i 节点,并取其中的设备号
	if ( !( inode = namei( dev_name ) ) )
	{
		return -ENOENT;
	}

	dev = inode->i_zone[ 0 ];

	// 如果不是块设备文件,则释放刚申请的i 节点dev_i,返回出错码
	if ( !S_ISBLK( inode->i_mode ) ) 
	{
		iput( inode );
		return -ENOTBLK;
	}

	// 释放设备文件名的i 节点
	iput( inode );

	// 如果设备是根文件系统,则不能被卸载,返回出错号
	if ( dev == ROOT_DEV )
	{
		return -EBUSY;
	}
	// 如果取设备的超级块失败,或者该设备文件系统没有安装过,则返回出错码
	if ( !( sb = get_super( dev ) ) || !( sb->s_imount ) )
	{
		return -ENOENT;
	}
	// 如果超级块所指明的被安装到的i 节点没有置位其安装标志,则显示警告信息
	if ( !sb->s_imount->i_mount )
	{
		printk( "Mounted inode has i_mount=0\n" );
	}

	// 查找i 节点表,看是否有进程在使用该设备上的文件,如果有则返回忙出错码
	for ( inode = inode_table + 0; inode < inode_table + NR_INODE; inode++ )
	{
		if ( inode->i_dev == dev && inode->i_count )
		{
			return -EBUSY;
		}
	}

	// 复位被安装到的i 节点的安装标志,释放该i 节点
	sb->s_imount->i_mount = 0;

	iput( sb->s_imount );

	// 置超级块中被安装i 节点字段为空,并释放设备文件系统的根i 节点,置超级块中被安装系统
	// 根 i 节点指针为空
	sb->s_imount = NULL;

	iput( sb->s_isup );
	sb->s_isup = NULL;

	put_super( dev );
	sync_dev ( dev );

	return 0;
}

// 安装文件系统调用函数.
// 参数dev_name 是设备文件名,dir_name 是安装到的目录名,rw_flag 被安装文件的读写标志.
// 将被加载的地方必须是一个目录名,并且对应的i 节点没有被其它程序占用

LONG sys_mount( CHAR * dev_name, CHAR * dir_name, LONG rw_flag )
{
	M_Inode			*	dev_i, *dir_i;
	Super_Block		*	sb;
	LONG				dev;

	// 首先根据设备文件名找到对应的i 节点,并取其中的设备号.
	// 对于块特殊设备文件,设备号在i 节点的i_zone[ 0 ]中
	if ( !( dev_i = namei( dev_name ) ) )
	{
		return -ENOENT;
	}

	dev = dev_i->i_zone[ 0 ];

	// 如果不是块设备文件,则释放刚取得的 i 节点 dev_i ,返回出错码.
	if ( !S_ISBLK( dev_i->i_mode ) ) 
	{
		iput( dev_i );

		return -EPERM;
	}

	// 释放该设备文件的i 节点dev_i
	iput( dev_i );

	// 根据给定的目录文件名找到对应的i 节点dir_i
	if ( !( dir_i = namei( dir_name ) ) )
	{
		return -ENOENT;
	}

	// 如果该i 节点的引用计数不为1( 仅在这里引用 ),或者该i 节点的节点号是根文件系统的节点
	// 号1,则释放该i 节点,返回出错码

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

	// 读取将安装文件系统的超级块,如果失败则也释放该i 节点,返回出错码
	if ( !( sb = read_super( dev ) ) )
	{
		iput( dir_i );
		return -EBUSY;
	}

	// 如果将要被安装的文件系统已经安装在其它地方,则释放该i 节点,返回出错码
	if ( sb->s_imount ) 
	{
		iput( dir_i );
		return -EBUSY;
	}

	// 如果将要安装到的i 节点已经安装了文件系统( 安装标志已经置位 ),则释放该i 节点,返回出错码.
	if ( dir_i->i_mount ) 
	{
		iput( dir_i );
		return -EPERM;
	}

	// 被安装文件系统超级块的"被安装到i 节点"字段指向安装到的目录名的i 节点
	sb->s_imount	= dir_i;
	// 设置安装位置i 节点的安装标志和节点已修改标志
	dir_i->i_mount	= 1;
	dir_i->i_dirt	= 1;		/* NOTE! we don't iput( dir_i ) */
	return 0;			/* we do that in umount */
}

VOID mount_root()
/*++

Routine Description:

	挂载根文件系统.被sys_setup调用.MINIX 1.0 文件系统格式如下:

	+--------+--------+--------+--------+--------+--------+--------+--------+--------+-----------
	|		 |		  |		   |		|		 |		  |		   |		|		 |
	|  BOOT  |	Super |	inode  | Block	| inode  |	inode | ...    | Block	| Block	 | ...
	|        |	Block |	BitMap | BitMap	|	0	 |	  1	  |		   |   0	|   1	 |
	|		 |		  |		   |		|		 |		  |		   |		|		 |
	+--------+--------+--------+--------+--------+--------+--------+--------+--------+-----------

	SuperBlock  s_ninodes			i节点数
				s_nzones			逻辑块数
				s_imap_blocks		i节点位图所占块数
				s_zmap_blocks		逻辑块位图所占块数
				s_firstdatazone  	数据区第一个逻辑块块号


	inode       i_mode				文件的类型和属性
		        i_uid				文件宿主的用户id 
		        i_size				文件长度（字节）
		        i_mtime				修改时间（从1970.1.1:0时算起，秒）
		        i_gid				文件宿主的组id 
		        i_nlinks			链接数（有多少个文件目录项指向该i节点）
                i_zone[ 9 ]		    文件所占用的盘上逻辑块号数组


	源代码中 8191 为 0x1FFFF ,由于每个块是2扇区 1K 大小,因此一个块有 8192 个Bit

	set_bit( i & 8191, p->s_zmap[ i >> 13 ]->b_data ) 就是设置对应的块的bit位

	mi=iget(0,1); 这个inode作为进程1的工作目录

Arguments:
	-
Return Value:
	-
--*/
{
	LONG				i, free;
	Super_Block		*	p;
	M_Inode			*	mi;

	// 初始化文件表数组( 共64 项,也即系统同时只能打开64 个文件 ),将所有文件结构中的引用计数
	// 设置为0.[ ??为什么放在这里初始化? ]

	for ( i = 0; i < NR_FILE; i++ )
	{
		file_table[ i ].f_count = 0;
	}

	// 如果根文件系统所在设备是软盘的话,就提示"插入根文件系统盘,并按回车键",并等待按键.

	if ( MAJOR( ROOT_DEV ) == 2 ) 
	{
		printk( "Insert root floppy and press ENTER" );
		wait_for_keypress();
	}

	// 初始化超级块数组( 共 8 项 )
	for ( p = &super_block[ 0 ]; p < &super_block[ NR_SUPER ]; p++ )
	{
		p->s_dev	= 0;
		p->s_lock	= 0;
		p->s_wait	= NULL;
	}

	// 如果读根设备上超级块失败,则显示信息,并死机
	if ( !( p = read_super( ROOT_DEV ) ) )
	{
		panic( "Unable to mount root" );
	}

	//从设备上读取文件系统的根 i 节点( 1 ),如果失败则显示出错信息,死机
	if ( !( mi = iget( ROOT_DEV, ROOT_INO ) ) )
	{
		panic( "Unable to read root i-node" );
	}

	// 该i 节点引用次数递增3 次.因为下面266-268 行上也引用了该i 节点
	/* 注意！从逻辑上讲,它已被引用了4 次,而不是 1 次 */
	// 置该超级块的被安装文件系统i 节点和被安装到的i 节点为该 i 节点.

	mi->i_count	   += 3;	/* NOTE! it is logically used 4 times, not 1 */
	p->s_isup		= p->s_imount = mi;
	current->pwd	= mi;
	current->root	= mi;
	free			= 0;
	i				= p->s_nzones;

	// 然后根据逻辑块位图中相应比特位的占用情况统计出空闲块数.这里宏函数set_bit()只是在测试
	// 比特位,而非设置比特位."i&8191"用于取得i 节点号在当前块中的偏移值."i>>13"是将i 除以
	// 8192,也即除一个磁盘块包含的比特位数 8191<->0x1FFFF
	while ( --i >= 0 )
	{
		if ( !set_bit( i & 8191, p->s_zmap[ i >> 13 ]->b_data ) )
		{
			free++;
		}
	}

	// 显示设备上空闲逻辑块数/逻辑块总数
	printk( "%d/%d free blocks\n\r", free, p->s_nzones );

	// 统计设备上空闲 i 节点数.首先令 i 等于超级块中表明的设备上 i 节点总数+1.加 1 是将 0 节点
	// 也统计进去

	free = 0;
	i	 = p->s_ninodes + 1;

	// 然后根据i 节点位图中相应比特位的占用情况计算出空闲i 节点数
	while ( --i >= 0 )
	{
		if ( !set_bit( i & 8191, p->s_imap[ i >> 13 ]->b_data ) )
		{
			free++;
		}
	}
	printk( "%d/%d free inodes\n\r", free, p->s_ninodes );
}
