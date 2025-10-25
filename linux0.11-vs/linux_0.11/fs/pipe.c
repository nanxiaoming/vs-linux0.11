/*
 *  linux/fs/pipe.c
 *
 *  ( C ) 1991  Linus Torvalds
 */

#include <signal.h>

#include <linux\sched.h>
#include <linux\mm.h>	/* for get_free_page */
#include <asm\segment.h>

 // 管道读操作函数.
 // 参数inode 是管道对应的i 节点,buf 是数据缓冲区指针,count 是读取的字节数.
LONG read_pipe( M_Inode * inode, CHAR * buf, LONG count )
{
	LONG chars, size, read = 0;

	// 若欲读取的字节计数值count 大于0,则循环执行以下操作
	while ( count > 0 ) {
		// 若当前管道中没有数据( size=0 ),则唤醒等待该节点的进程,如果已没有写管道者,则返回已读
		// 字节数,退出.否则在该i 节点上睡眠,等待信息
		while ( !( size = PIPE_SIZE( *inode ) ) ) {
			wake_up( &inode->i_wait );
			if ( inode->i_count != 2 ) /* are there any writers? */
				return read;
			sleep_on( &inode->i_wait );
		}
		// 取管道尾到缓冲区末端的字节数chars.如果其大于还需要读取的字节数count,则令其等于count.
		// 如果chars 大于当前管道中含有数据的长度size,则令其等于size
		chars = PAGE_SIZE - PIPE_TAIL( *inode );
		if ( chars > count )
			chars = count;
		if ( chars > size )
			chars = size;
		// 读字节计数减去此次可读的字节数chars,并累加已读字节数
		count -= chars;
		read += chars;
		// 令size 指向管道尾部,调整当前管道尾指针( 前移chars 字节 )
		size = PIPE_TAIL( *inode );
		PIPE_TAIL( *inode ) += (USHORT)chars;
		PIPE_TAIL( *inode ) &= ( PAGE_SIZE - 1 );
		// 将管道中的数据复制到用户缓冲区中.对于管道i 节点,其i_size 字段中是管道缓冲块指针
		while ( chars-- > 0 )
			put_fs_byte( ( ( CHAR * )inode->i_size )[ size++ ], buf++ );
	}
	// 唤醒等待该管道i 节点的进程,并返回读取的字节数
	wake_up( &inode->i_wait );
	return read;
}

// 管道写操作函数.
// 参数inode 是管道对应的i 节点,buf 是数据缓冲区指针,count 是将写入管道的字节数

LONG write_pipe( M_Inode * inode, CHAR * buf, LONG count )
{
	LONG chars, size, written = 0;

	while ( count > 0 ) {
		// 若当前管道中没有已经满了( size=0 ),则唤醒等待该节点的进程,如果已没有读管道者,则向进程
		// 发送SIGPIPE 信号,并返回已写入的字节数并退出.若写入0 字节,则返回-1.否则在该i 节点上
		// 睡眠,等待管道腾出空间
		while ( !( size = ( PAGE_SIZE - 1 ) - PIPE_SIZE( *inode ) ) ) {
			wake_up( &inode->i_wait );
			if ( inode->i_count != 2 ) { /* no readers */
				current->signal |= ( 1 << ( SIGPIPE - 1 ) );
				return written ? written : -1;
			}
			sleep_on( &inode->i_wait );
		}
		// 取管道头部到缓冲区末端空间字节数chars.如果其大于还需要写入的字节数count,则令其等于
		// count.如果chars 大于当前管道中空闲空间长度size,则令其等于size
		chars = PAGE_SIZE - PIPE_HEAD( *inode );
		if ( chars > count )
			chars = count;
		if ( chars > size )
			chars = size;
		// 写入字节计数减去此次可写入的字节数chars,并累加已写字节数到written
		count -= chars;
		written += chars;
		// 令size 指向管道数据头部,调整当前管道数据头部指针( 前移chars 字节 )
		size = PIPE_HEAD( *inode );
		PIPE_HEAD( *inode ) += (USHORT)chars;
		PIPE_HEAD( *inode ) &= ( PAGE_SIZE - 1 );
		// 从用户缓冲区复制chars 个字节到管道中.对于管道i 节点,其i_size 字段中是管道缓冲块指针.
		while ( chars-- > 0 )
			( ( CHAR * )inode->i_size )[ size++ ] = get_fs_byte( buf++ );
	}
	// 唤醒等待该i 节点的进程,返回已写入的字节数,退出
	wake_up( &inode->i_wait );
	return written;
}

// 创建管道系统调用函数.
// 在fildes 所指的数组中创建一对文件句柄( 描述符 ).这对文件句柄指向一管道i 节点.fildes[ 0 ]
// 用于读管道中数据,fildes[ 1 ]用于向管道中写入数据.
// 成功时返回0,出错时返回-1
LONG sys_pipe( ULONG * fildes )
{
	M_Inode * inode;
	File * f[ 2 ];
	LONG fd[ 2 ];
	LONG i, j;

	j = 0;
	for ( i = 0; j < 2 && i < NR_FILE; i++ )
		if ( !file_table[ i ].f_count )
			( f[ j++ ] = i + file_table )->f_count++;
	if ( j == 1 )
		f[ 0 ]->f_count = 0;
	if ( j < 2 )
		return -1;
	j = 0;
	for ( i = 0; j < 2 && i < NR_OPEN; i++ )
		if ( !current->filp[ i ] ) {
			current->filp[ fd[ j ] = i ] = f[ j ];
			j++;
		}
	if ( j == 1 )
		current->filp[ fd[ 0 ] ] = NULL;
	if ( j < 2 ) {
		f[ 0 ]->f_count = f[ 1 ]->f_count = 0;
		return -1;
	}
	if ( !( inode = get_pipe_inode() ) ) {
		current->filp[ fd[ 0 ] ] =
			current->filp[ fd[ 1 ] ] = NULL;
		f[ 0 ]->f_count = f[ 1 ]->f_count = 0;
		return -1;
	}
	f[ 0 ]->f_inode = f[ 1 ]->f_inode = inode;
	f[ 0 ]->f_pos = f[ 1 ]->f_pos = 0;
	f[ 0 ]->f_mode = 1;		/* read */
	f[ 1 ]->f_mode = 2;		/* write */
	put_fs_long( fd[ 0 ], 0 + fildes );
	put_fs_long( fd[ 1 ], 1 + fildes );
	return 0;
}
