/*
* This file has definitions for some important file table
* structures etc.
*/

#ifndef _FS_H
#define _FS_H

#include <sys\types.h>


/* devices are as follows: ( same as minix, so we can use the minix
* file system. These are major numbers. )
*
* 0 - unused ( nodev )
* 1 - /dev/mem
* 2 - /dev/fd
* 3 - /dev/hd
* 4 - /dev/ttyx
* 5 - /dev/tty
* 6 - /dev/lp
* 7 - unnamed pipes
*/

#define IS_SEEKABLE( x ) ( ( x )>=1 && ( x )<=3 )

#define READ 0
#define WRITE 1
#define READA 2		/* read-ahead - don't pause */
#define WRITEA 3	/* "write-ahead" - silly, but somewhat useful */

VOID buffer_init( LONG buffer_end );

#define MAJOR( a ) ( ( ( unsigned )( a ) )>>8 )
#define MINOR( a ) ( ( a )&0xff )

#define NAME_LEN 14
#define ROOT_INO 1

#define I_MAP_SLOTS 8
#define Z_MAP_SLOTS 8
#define SUPER_MAGIC 0x137F

#define NR_OPEN 20
#define NR_INODE 32
#define NR_FILE 64
#define NR_SUPER 8
#define NR_HASH 307
#define NR_BUFFERS nr_buffers
#define BLOCK_SIZE 1024
#define BLOCK_SIZE_BITS 10
#ifndef NULL
#define NULL ( ( VOID * ) 0 )
#endif

#define INODES_PER_BLOCK ( ( BLOCK_SIZE )/( sizeof ( D_Inode ) ) )
#define DIR_ENTRIES_PER_BLOCK ( ( BLOCK_SIZE )/( sizeof ( Dir_Entry ) ) )

#define PIPE_HEAD( inode ) ( ( inode ).i_zone[ 0 ] )
#define PIPE_TAIL( inode ) ( ( inode ).i_zone[ 1 ] )
#define PIPE_SIZE( inode ) ( ( PIPE_HEAD( inode )-PIPE_TAIL( inode ) )&( PAGE_SIZE-1 ) )
#define PIPE_EMPTY( inode ) ( PIPE_HEAD( inode )==PIPE_TAIL( inode ) )
#define PIPE_FULL( inode ) ( PIPE_SIZE( inode )==( PAGE_SIZE-1 ) )
#define INC_PIPE( head ) \
	__asm__( "incl %0\n\tandl $4095,%0"::"m" ( head ) )

typedef CHAR buffer_block[ BLOCK_SIZE ];

typedef struct buffer_head 
{
	CHAR				*	b_data;		/* pointer to data block ( 1024 bytes ) */	// Offset=0x0 Size=0x4
	ULONG					b_blocknr;	/* block number */							// Offset=0x4 Size=0x4
	USHORT					b_dev;		/* device ( 0 = free ) */					// Offset=0x8 Size=0x2
	UCHAR					b_uptodate;												// Offset=0xa Size=0x1
	UCHAR					b_dirt;		/* 0-clean,1-dirty */						// Offset=0xb Size=0x1
	UCHAR					b_count;	/* users using this block */				// Offset=0xc Size=0x1
	UCHAR					b_lock;		/* 0 - ok, 1 -locked */						// Offset=0xd Size=0x1
	struct task_struct	*	b_wait;													// Offset=0x10 Size=0x4
	struct buffer_head	*	b_prev;													// Offset=0x14 Size=0x4
	struct buffer_head	*	b_next;													// Offset=0x18 Size=0x4
	struct buffer_head	*	b_prev_free;											// Offset=0x1c Size=0x4
	struct buffer_head	*	b_next_free;											// Offset=0x20 Size=0x4
}Buffer_Head;

typedef struct d_inode 
{
	USHORT i_mode;
	USHORT i_uid;
	ULONG i_size;
	ULONG i_time;
	UCHAR i_gid;
	UCHAR i_nlinks;
	USHORT i_zone[ 9 ];
}D_Inode;

/*
 * i_mode 的属性
 *           15              7               0
 *           +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *           | | | | | | | |R|W|X|R|W|X|R|W|X|
 *           +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *			  ---+--- --+-- ----- ----- -----
 *				 |	    |	 宿主    组员  其他
 *				 |	    |
 *				 |	    |	01 - 执行时设置用户ID(set-user-ID)
 *				 |	    +-->02 - 执行时设置组ID
 * 				 |	        04 - 对于目录，受限删除标记
 *				 |
 *				 |		    01 - FIFO文件(八进制)
 * 			     |          02 - 字符设备文件
 * 				 +--------->04 - 目录文件
 *							06 - 块设备文件
 *							10 - 常规文件
 */


typedef struct m_inode 
{
	USHORT		i_mode;					// Offset=0x0 Size=0x2	文件的类型和属性
	USHORT		i_uid;					// Offset=0x2 Size=0x2	文件宿主的用户id 
	ULONG		i_size;					// Offset=0x4 Size=0x4	文件长度（字节）
	ULONG		i_mtime;				// Offset=0x8 Size=0x4	修改时间（从1970.1.1:0时算起，秒）
	UCHAR		i_gid;					// Offset=0xc Size=0x1	文件宿主的组id 
	UCHAR		i_nlinks;				// Offset=0xd Size=0x1	链接数（有多少个文件目录项指向该i节点）
	USHORT		i_zone[ 9 ];			// Offset=0xe Size=0x12	文件所占用的盘上逻辑块号数组。其中： 
										//						zone[0]-zone[6]是直接块号； 
										//						zone[7]是一次间接块号； 
										//						zone[8]是二次（双重）间接块号。 
										//						注：zone是区的意思，可译成区块或逻辑块。
										//						对于设备特殊文件名的i节点，其zone[0]中
										//						存放的是该文件名所指设备的设备号。 
	/* these are in memory also */
	struct task_struct * i_wait;		// Offset=0x20 Size=0x4	等待该i节点的进程
	ULONG		i_atime;				// Offset=0x24 Size=0x4 最后访问时间
	ULONG		i_ctime;				// Offset=0x28 Size=0x4	i节点自身被修改时间
	USHORT		i_dev;					// Offset=0x2c Size=0x2	i节点所在的设备号
	USHORT		i_num;					// Offset=0x2e Size=0x2	i节点号
	USHORT		i_count;				// Offset=0x30 Size=0x2	i节点被引用的次数，0表示空闲
	UCHAR		i_lock;					// Offset=0x32 Size=0x1	i节点被锁定标志
	UCHAR		i_dirt;					// Offset=0x33 Size=0x1	i节点已被修改(脏)标志
	UCHAR		i_pipe;					// Offset=0x34 Size=0x1	i节点用作管道标志
	UCHAR		i_mount;				// Offset=0x35 Size=0x1	i节点安装了其他文件系统标志
	UCHAR		i_seek;					// Offset=0x36 Size=0x1	搜索标志(lseek操作时)
	UCHAR		i_update;				// Offset=0x37 Size=0x1 i节点已更新标志
}M_Inode;


/*
 * i_zone 的结构,每个文件表达最大长度为 7+512+512*512 = 262663Kb空间
 *
 *				+-----------+		    一次间接块
 * 			+->	| i_zone[0]	|		    +-------+
 * 			|	+-----------+		    |       |  
 * 			|	| i_zone[1]	|		    |       |       二次间接块
 * 			|	+-----------+	+---->  |       |		+-------+
 * 			|	| i_zone[2]	|	|	    |       |  		|       |
 * 			|	+-----------+   |       +-------+		|       |
 * 	 	直接 |	| i_zone[3]	|   |						|       |
 * 	 	块号 |	+-----------+   |						|       |
 * 			|	| i_zone[4]	|   |				   +-->	|       |
 * 			|	+-----------+   |				   |	|       |
 * 			|	| i_zone[5]	|	|		+-------+  |    +-------+
 * 			|	+-----------+	|		|       |  |
 * 			+->	| i_zone[6]	|	|	+->	|       |--+
 * 				+-----------+	|	|	|       |
 *    1级间接块号 | i_zone[7]	|---+	|	|       |
 * 				+-----------+		|	+-------+
 * 	  2级间接块号 | i_zone[8]	|-------+
 * 				+-----------+ 
 * 
 */

typedef struct file		// Size=0x10
{	
	USHORT		f_mode;				// Offset=0x0 Size=0x2
	USHORT		f_flags;			// Offset=0x2 Size=0x2
	USHORT		f_count;			// Offset=0x4 Size=0x2
	M_Inode *	f_inode;			// Offset=0x8 Size=0x4
	off_t		f_pos;				// Offset=0xc Size=0x4
}File;

typedef struct super_block 
{
	USHORT					s_ninodes;				// Offset=0x0 Size=0x2		i节点数
	USHORT					s_nzones;				// Offset=0x2 Size=0x2		逻辑块数
	USHORT					s_imap_blocks;			// Offset=0x4 Size=0x2		i节点位图所占块数
	USHORT					s_zmap_blocks;			// Offset=0x6 Size=0x2		逻辑块位图所占块数
	USHORT					s_firstdatazone;		// Offset=0x8 Size=0x2		数据区第一个逻辑块块号
	USHORT					s_log_zone_size;		// Offset=0xa Size=0x2		Log2磁盘块数
	ULONG					s_max_size;				// Offset=0xc Size=0x4		最大文件长度
	USHORT					s_magic;				// Offset=0x10 Size=0x2		0x137f
	/* These are only in memory */
	Buffer_Head *			s_imap[ 8 ];			// Offset=0x14 Size=0x20	i节点位图在高速缓存块指针数组
	Buffer_Head *			s_zmap[ 8 ];			// Offset=0x34 Size=0x20	逻辑块位图在高速缓存块指针数组
	USHORT					s_dev;					// Offset=0x54 Size=0x2		超级块所在设备号
	M_Inode		*			s_isup;					// Offset=0x58 Size=0x4		被安装文件系统根目录i节点
	M_Inode		*			s_imount;				// Offset=0x5c Size=0x4		该文件系统被安装到的i节点
	ULONG					s_time;					// Offset=0x60 Size=0x4		
	struct task_struct *	s_wait;					// Offset=0x64 Size=0x4		等待本超级块的进程
	UCHAR					s_lock;					// Offset=0x68 Size=0x1		锁定标记
	UCHAR					s_rd_only;				// Offset=0x69 Size=0x1		自读标记
	UCHAR					s_dirt;					// Offset=0x6a Size=0x1		被修改(脏)标志
}Super_Block;

typedef struct d_super_block
{
	USHORT	s_ninodes;				// Offset=0x0 Size=0x2
	USHORT	s_nzones;				// Offset=0x2 Size=0x2
	USHORT	s_imap_blocks;			// Offset=0x4 Size=0x2
	USHORT	s_zmap_blocks;			// Offset=0x6 Size=0x2
	USHORT	s_firstdatazone;		// Offset=0x8 Size=0x2
	USHORT	s_log_zone_size;		// Offset=0xa Size=0x2
	ULONG	s_max_size;				// Offset=0xc Size=0x4
	USHORT	s_magic;				// Offset=0x10 Size=0x2
}D_Super_Block;

typedef struct dir_entry 
{
	USHORT	inode;					// Offset=0x0 Size=0x2
	CHAR	name[ NAME_LEN ];			// Offset=0x2 Size=0xe
}Dir_Entry;

extern M_Inode			inode_table[ NR_INODE ];
extern File				file_table [ NR_FILE  ];
extern Super_Block		super_block[ NR_SUPER ];
extern Buffer_Head	*	start_buffer;
extern LONG				nr_buffers;

extern VOID check_disk_change( LONG dev );
extern LONG floppy_change( ULONG nr );
extern LONG ticks_to_floppy_on( ULONG dev );
extern VOID floppy_on( ULONG dev );
extern VOID floppy_off( ULONG dev );
extern VOID truncate( M_Inode * inode );
extern VOID sync_inodes();
extern VOID wait_on( M_Inode * inode );
extern LONG bmap( M_Inode * inode, LONG block );
extern LONG create_block( M_Inode * inode, LONG block );
extern M_Inode *namei( const CHAR * pathname );
extern LONG open_namei( const CHAR * pathname, LONG flag, LONG mode,M_Inode **res_inode );
extern VOID iput( M_Inode * inode );
extern M_Inode *iget( LONG dev, LONG nr );
extern M_Inode *get_empty_inode();
extern M_Inode *get_pipe_inode();
extern Buffer_Head *get_hash_table( LONG dev, LONG block );
extern Buffer_Head *getblk( LONG dev, LONG block );
extern VOID ll_rw_block( LONG rw, Buffer_Head * bh );
extern VOID brelse( Buffer_Head * buf );
extern Buffer_Head *bread( LONG dev, LONG block );
extern VOID bread_page( ULONG addr, LONG dev, LONG b[ 4 ] );
extern Buffer_Head *breada( LONG dev, LONG block, ... );
extern LONG new_block( LONG dev );
extern VOID free_block( LONG dev, LONG block );
extern M_Inode *new_inode( LONG dev );
extern VOID free_inode( M_Inode * inode );
extern LONG sync_dev( LONG dev );
extern Super_Block *get_super( LONG dev );
extern LONG ROOT_DEV;
extern VOID mount_root();

#endif
