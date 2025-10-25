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
 * i_mode ������
 *           15              7               0
 *           +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *           | | | | | | | |R|W|X|R|W|X|R|W|X|
 *           +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *			  ---+--- --+-- ----- ----- -----
 *				 |	    |	 ����    ��Ա  ����
 *				 |	    |
 *				 |	    |	01 - ִ��ʱ�����û�ID(set-user-ID)
 *				 |	    +-->02 - ִ��ʱ������ID
 * 				 |	        04 - ����Ŀ¼������ɾ�����
 *				 |
 *				 |		    01 - FIFO�ļ�(�˽���)
 * 			     |          02 - �ַ��豸�ļ�
 * 				 +--------->04 - Ŀ¼�ļ�
 *							06 - ���豸�ļ�
 *							10 - �����ļ�
 */


typedef struct m_inode 
{
	USHORT		i_mode;					// Offset=0x0 Size=0x2	�ļ������ͺ�����
	USHORT		i_uid;					// Offset=0x2 Size=0x2	�ļ��������û�id 
	ULONG		i_size;					// Offset=0x4 Size=0x4	�ļ����ȣ��ֽڣ�
	ULONG		i_mtime;				// Offset=0x8 Size=0x4	�޸�ʱ�䣨��1970.1.1:0ʱ�����룩
	UCHAR		i_gid;					// Offset=0xc Size=0x1	�ļ���������id 
	UCHAR		i_nlinks;				// Offset=0xd Size=0x1	���������ж��ٸ��ļ�Ŀ¼��ָ���i�ڵ㣩
	USHORT		i_zone[ 9 ];			// Offset=0xe Size=0x12	�ļ���ռ�õ������߼�������顣���У� 
										//						zone[0]-zone[6]��ֱ�ӿ�ţ� 
										//						zone[7]��һ�μ�ӿ�ţ� 
										//						zone[8]�Ƕ��Σ�˫�أ���ӿ�š� 
										//						ע��zone��������˼�������������߼��顣
										//						�����豸�����ļ�����i�ڵ㣬��zone[0]��
										//						��ŵ��Ǹ��ļ�����ָ�豸���豸�š� 
	/* these are in memory also */
	struct task_struct * i_wait;		// Offset=0x20 Size=0x4	�ȴ���i�ڵ�Ľ���
	ULONG		i_atime;				// Offset=0x24 Size=0x4 ������ʱ��
	ULONG		i_ctime;				// Offset=0x28 Size=0x4	i�ڵ������޸�ʱ��
	USHORT		i_dev;					// Offset=0x2c Size=0x2	i�ڵ����ڵ��豸��
	USHORT		i_num;					// Offset=0x2e Size=0x2	i�ڵ��
	USHORT		i_count;				// Offset=0x30 Size=0x2	i�ڵ㱻���õĴ�����0��ʾ����
	UCHAR		i_lock;					// Offset=0x32 Size=0x1	i�ڵ㱻������־
	UCHAR		i_dirt;					// Offset=0x33 Size=0x1	i�ڵ��ѱ��޸�(��)��־
	UCHAR		i_pipe;					// Offset=0x34 Size=0x1	i�ڵ������ܵ���־
	UCHAR		i_mount;				// Offset=0x35 Size=0x1	i�ڵ㰲װ�������ļ�ϵͳ��־
	UCHAR		i_seek;					// Offset=0x36 Size=0x1	������־(lseek����ʱ)
	UCHAR		i_update;				// Offset=0x37 Size=0x1 i�ڵ��Ѹ��±�־
}M_Inode;


/*
 * i_zone �Ľṹ,ÿ���ļ������󳤶�Ϊ 7+512+512*512 = 262663Kb�ռ�
 *
 *				+-----------+		    һ�μ�ӿ�
 * 			+->	| i_zone[0]	|		    +-------+
 * 			|	+-----------+		    |       |  
 * 			|	| i_zone[1]	|		    |       |       ���μ�ӿ�
 * 			|	+-----------+	+---->  |       |		+-------+
 * 			|	| i_zone[2]	|	|	    |       |  		|       |
 * 			|	+-----------+   |       +-------+		|       |
 * 	 	ֱ�� |	| i_zone[3]	|   |						|       |
 * 	 	��� |	+-----------+   |						|       |
 * 			|	| i_zone[4]	|   |				   +-->	|       |
 * 			|	+-----------+   |				   |	|       |
 * 			|	| i_zone[5]	|	|		+-------+  |    +-------+
 * 			|	+-----------+	|		|       |  |
 * 			+->	| i_zone[6]	|	|	+->	|       |--+
 * 				+-----------+	|	|	|       |
 *    1����ӿ�� | i_zone[7]	|---+	|	|       |
 * 				+-----------+		|	+-------+
 * 	  2����ӿ�� | i_zone[8]	|-------+
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
	USHORT					s_ninodes;				// Offset=0x0 Size=0x2		i�ڵ���
	USHORT					s_nzones;				// Offset=0x2 Size=0x2		�߼�����
	USHORT					s_imap_blocks;			// Offset=0x4 Size=0x2		i�ڵ�λͼ��ռ����
	USHORT					s_zmap_blocks;			// Offset=0x6 Size=0x2		�߼���λͼ��ռ����
	USHORT					s_firstdatazone;		// Offset=0x8 Size=0x2		��������һ���߼�����
	USHORT					s_log_zone_size;		// Offset=0xa Size=0x2		Log2���̿���
	ULONG					s_max_size;				// Offset=0xc Size=0x4		����ļ�����
	USHORT					s_magic;				// Offset=0x10 Size=0x2		0x137f
	/* These are only in memory */
	Buffer_Head *			s_imap[ 8 ];			// Offset=0x14 Size=0x20	i�ڵ�λͼ�ڸ��ٻ����ָ������
	Buffer_Head *			s_zmap[ 8 ];			// Offset=0x34 Size=0x20	�߼���λͼ�ڸ��ٻ����ָ������
	USHORT					s_dev;					// Offset=0x54 Size=0x2		�����������豸��
	M_Inode		*			s_isup;					// Offset=0x58 Size=0x4		����װ�ļ�ϵͳ��Ŀ¼i�ڵ�
	M_Inode		*			s_imount;				// Offset=0x5c Size=0x4		���ļ�ϵͳ����װ����i�ڵ�
	ULONG					s_time;					// Offset=0x60 Size=0x4		
	struct task_struct *	s_wait;					// Offset=0x64 Size=0x4		�ȴ���������Ľ���
	UCHAR					s_lock;					// Offset=0x68 Size=0x1		�������
	UCHAR					s_rd_only;				// Offset=0x69 Size=0x1		�Զ����
	UCHAR					s_dirt;					// Offset=0x6a Size=0x1		���޸�(��)��־
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
