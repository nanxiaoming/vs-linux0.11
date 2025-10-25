/*
*  linux/fs/read_write.c
*
*  ( C ) 1991  Linus Torvalds
*/

#include <sys\stat.h>
#include <errno.h>
#include <sys\types.h>

#include <linux\kernel.h>
#include <linux\sched.h>
#include <asm\segment.h>

extern LONG rw_char( LONG rw, LONG dev, CHAR * buf, LONG count, off_t * pos );
extern LONG read_pipe( M_Inode * inode, CHAR * buf, LONG count );
extern LONG write_pipe( M_Inode * inode, CHAR * buf, LONG count );
extern LONG block_read( LONG dev, off_t * pos, CHAR * buf, LONG count );
extern LONG block_write( LONG dev, off_t * pos, CHAR * buf, LONG count );
extern LONG file_read( M_Inode * inode, File * filp,
	CHAR * buf, LONG count );
extern LONG file_write( M_Inode * inode, File * filp,
	CHAR * buf, LONG count );

// 重定位文件读写指针系统调用函数.
// 参数fd 是文件句柄,offset 是新的文件读写指针偏移值,origin 是偏移的起始位置,是SEEK_SET
// ( 0,从文件开始处 )、SEEK_CUR( 1,从当前读写位置 )、SEEK_END( 2,从文件尾处 )三者之一.

LONG sys_lseek( ULONG fd, off_t offset, LONG origin )
{
	File * file;
	LONG tmp;

	// 如果文件句柄值大于程序最多打开文件数NR_OPEN( 20 ),或者该句柄的文件结构指针为空,或者
	// 对应文件结构的i 节点字段为空,或者指定设备文件指针是不可定位的,则返回出错码并退出.

	if ( fd >= NR_OPEN || !( file = current->filp[ fd ] ) || !( file->f_inode )
		|| !IS_SEEKABLE( MAJOR( file->f_inode->i_dev ) ) )
		return -EBADF;

	// 如果文件对应的i 节点是管道节点,则返回出错码,退出.管道头尾指针不可随意移动！
	if ( file->f_inode->i_pipe )
		return -ESPIPE;
	// 根据设置的定位标志,分别重新定位文件读写指针
	switch ( origin ) {
	// origin = SEEK_SET,要求以文件起始处作为原点设置文件读写指针.若偏移值小于零,则出错返
	// 回错误码.否则设置文件读写指针等于offset
	case 0:
		if ( offset < 0 ) return -EINVAL;
		file->f_pos = offset;
		break;
	// origin = SEEK_CUR,要求以文件当前读写指针处作为原点重定位读写指针.如果文件当前指针加
	// 上偏移值小于0,则返回出错码退出.否则在当前读写指针上加上偏移值
	case 1:
		if ( file->f_pos + offset < 0 ) return -EINVAL;
		file->f_pos += offset;
		break;
	// origin = SEEK_END,要求以文件末尾作为原点重定位读写指针.此时若文件大小加上偏移值小于零
	// 则返回出错码退出.否则重定位读写指针为文件长度加上偏移值
	case 2:
		if ( ( tmp = file->f_inode->i_size + offset ) < 0 )
			return -EINVAL;
		file->f_pos = tmp;
		break;
	// origin 设置出错,返回出错码退出
	default:
		return -EINVAL;
	}
	return file->f_pos;
}

// 读文件系统调用函数.
// 参数fd 是文件句柄,buf 是缓冲区,count 是欲读字节数
LONG sys_read( ULONG fd, CHAR * buf, LONG count )
{
	File * file;
	M_Inode * inode;

	// 如果文件句柄值大于程序最多打开文件数NR_OPEN,或者需要读取的字节计数值小于0,或者该句柄
	// 的文件结构指针为空,则返回出错码并退出
	if ( fd >= NR_OPEN || count < 0 || !( file = current->filp[ fd ] ) )
		return -EINVAL;
	if ( !count )
		return 0;
	verify_area( buf, count );
	inode = file->f_inode;
	if ( inode->i_pipe )
		return ( file->f_mode & 1 ) ? read_pipe( inode, buf, count ) : -EIO;
	// 如果是字符型文件,则进行读字符设备操作,返回读取的字符数
	if ( S_ISCHR( inode->i_mode ) )
		return rw_char( READ, inode->i_zone[ 0 ], buf, count, &file->f_pos );
	// 如果是块设备文件,则执行块设备读操作,并返回读取的字节数
	if ( S_ISBLK( inode->i_mode ) )
		return block_read( inode->i_zone[ 0 ], &file->f_pos, buf, count );
	// 如果是目录文件或者是常规文件,则首先验证读取数count 的有效性并进行调整( 若读取字节数加上
	// 文件当前读写指针值大于文件大小,则重新设置读取字节数为文件长度-当前读写指针值,若读取数
	// 等于0,则返回0 退出 ),然后执行文件读操作,返回读取的字节数并退出
	if ( S_ISDIR( inode->i_mode ) || S_ISREG( inode->i_mode ) ) {
		if ( count + file->f_pos > inode->i_size )
			count = inode->i_size - file->f_pos;
		if ( count <= 0 )
			return 0;
		return file_read( inode, file, buf, count );
	}
	printk( "( Read )inode->i_mode=%06o\n\r", inode->i_mode );
	return -EINVAL;
}

LONG sys_write( ULONG fd, CHAR * buf, LONG count )
{
	File * file;
	M_Inode * inode;

	if ( fd >= NR_OPEN || count < 0 || !( file = current->filp[ fd ] ) )
		return -EINVAL;
	if ( !count )
		return 0;
	inode = file->f_inode;
	if ( inode->i_pipe )
		return ( file->f_mode & 2 ) ? write_pipe( inode, buf, count ) : -EIO;
	
	// 如果是字符型文件 则进行写字符设备操作
	if ( S_ISCHR( inode->i_mode ) )
		return rw_char( WRITE, inode->i_zone[ 0 ], buf, count, &file->f_pos );
	// 块设备
	if ( S_ISBLK( inode->i_mode ) )
		return block_write( inode->i_zone[ 0 ], &file->f_pos, buf, count );
	// 常规文件
	if ( S_ISREG( inode->i_mode ) )
		return file_write( inode, file, buf, count );
	printk( "( Write )inode->i_mode=%06o\n\r", inode->i_mode );
	return -EINVAL;
}
