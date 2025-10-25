/*
 *  linux/fs/stat.c
 *
 *  ( C ) 1991  Linus Torvalds
 */

#include <errno.h>
#include <sys\stat.h>

#include <linux\fs.h>
#include <linux\sched.h>
#include <linux\kernel.h>
#include <asm\segment.h>

 // 复制文件状态信息.
 // 参数inode 是文件对应的i 节点,statbuf 是stat 文件状态结构指针,用于存放取得的状态信息.

static VOID cp_stat( M_Inode * inode, struct stat * statbuf )
{
	struct stat tmp;
	LONG i;

	// 首先验证( 或分配 )存放数据的内存空间
	verify_area( statbuf, sizeof ( *statbuf ) );
	// 然后临时复制相应节点上的信息
	tmp.st_dev = inode->i_dev;			// 文件所在的设备号.
	tmp.st_ino = inode->i_num;			// 文件i 节点号.
	tmp.st_mode = inode->i_mode;		// 文件属性.
	tmp.st_nlink = inode->i_nlinks;		// 文件的连接数.
	tmp.st_uid = inode->i_uid;			// 文件的用户id.
	tmp.st_gid = inode->i_gid;			// 文件的组id.
	tmp.st_rdev = inode->i_zone[ 0 ];		// 设备号( 如果文件是特殊的字符文件或块文件 ).
	tmp.st_size = inode->i_size;		// 文件大小( 字节数 )( 如果文件是常规文件 ).
	tmp.st_atime = inode->i_atime;		// 最后访问时间.
	tmp.st_mtime = inode->i_mtime;		// 最后修改时间.
	tmp.st_ctime = inode->i_ctime;		// 最后节点修改时间.

	// 最后将这些状态信息复制到用户缓冲区中
	for ( i = 0; i < sizeof ( tmp ); i++ )
		put_fs_byte( ( ( CHAR * )&tmp )[ i ], &( ( CHAR * )statbuf )[ i ] );
}

// 文件状态系统调用函数 - 根据文件名获取文件状态信息.
// 参数filename 是指定的文件名,statbuf 是存放状态信息的缓冲区指针.
// 返回0,若出错则返回出错码

LONG sys_stat( CHAR * filename, struct stat * statbuf )
{
	M_Inode * inode;

	// 首先根据文件名找出对应的i 节点,若出错则返回错误码
	if ( !( inode = namei( filename ) ) )
		return -ENOENT;
	// 将i 节点上的文件状态信息复制到用户缓冲区中,并释放该i 节点.
	cp_stat( inode, statbuf );
	iput( inode );
	return 0;
}

// 文件状态系统调用 - 根据文件句柄获取文件状态信息.
// 参数fd 是指定文件的句柄( 描述符 ),statbuf 是存放状态信息的缓冲区指针.
// 返回0,若出错则返回出错码
LONG sys_fstat( ULONG fd, struct stat * statbuf )
{
	File * f;
	M_Inode * inode;

	if ( fd >= NR_OPEN || !( f = current->filp[ fd ] ) || !( inode = f->f_inode ) )
		return -EBADF;
	cp_stat( inode, statbuf );
	return 0;
}
