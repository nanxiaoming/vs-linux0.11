/*
 *  linux/fs/pipe.c
 *
 *  ( C ) 1991  Linus Torvalds
 */

#include <signal.h>

#include <linux\sched.h>
#include <linux\mm.h>	/* for get_free_page */
#include <asm\segment.h>

 // �ܵ�����������.
 // ����inode �ǹܵ���Ӧ��i �ڵ�,buf �����ݻ�����ָ��,count �Ƕ�ȡ���ֽ���.
LONG read_pipe( M_Inode * inode, CHAR * buf, LONG count )
{
	LONG chars, size, read = 0;

	// ������ȡ���ֽڼ���ֵcount ����0,��ѭ��ִ�����²���
	while ( count > 0 ) {
		// ����ǰ�ܵ���û������( size=0 ),���ѵȴ��ýڵ�Ľ���,�����û��д�ܵ���,�򷵻��Ѷ�
		// �ֽ���,�˳�.�����ڸ�i �ڵ���˯��,�ȴ���Ϣ
		while ( !( size = PIPE_SIZE( *inode ) ) ) {
			wake_up( &inode->i_wait );
			if ( inode->i_count != 2 ) /* are there any writers? */
				return read;
			sleep_on( &inode->i_wait );
		}
		// ȡ�ܵ�β��������ĩ�˵��ֽ���chars.�������ڻ���Ҫ��ȡ���ֽ���count,���������count.
		// ���chars ���ڵ�ǰ�ܵ��к������ݵĳ���size,���������size
		chars = PAGE_SIZE - PIPE_TAIL( *inode );
		if ( chars > count )
			chars = count;
		if ( chars > size )
			chars = size;
		// ���ֽڼ�����ȥ�˴οɶ����ֽ���chars,���ۼ��Ѷ��ֽ���
		count -= chars;
		read += chars;
		// ��size ָ��ܵ�β��,������ǰ�ܵ�βָ��( ǰ��chars �ֽ� )
		size = PIPE_TAIL( *inode );
		PIPE_TAIL( *inode ) += (USHORT)chars;
		PIPE_TAIL( *inode ) &= ( PAGE_SIZE - 1 );
		// ���ܵ��е����ݸ��Ƶ��û���������.���ڹܵ�i �ڵ�,��i_size �ֶ����ǹܵ������ָ��
		while ( chars-- > 0 )
			put_fs_byte( ( ( CHAR * )inode->i_size )[ size++ ], buf++ );
	}
	// ���ѵȴ��ùܵ�i �ڵ�Ľ���,�����ض�ȡ���ֽ���
	wake_up( &inode->i_wait );
	return read;
}

// �ܵ�д��������.
// ����inode �ǹܵ���Ӧ��i �ڵ�,buf �����ݻ�����ָ��,count �ǽ�д��ܵ����ֽ���

LONG write_pipe( M_Inode * inode, CHAR * buf, LONG count )
{
	LONG chars, size, written = 0;

	while ( count > 0 ) {
		// ����ǰ�ܵ���û���Ѿ�����( size=0 ),���ѵȴ��ýڵ�Ľ���,�����û�ж��ܵ���,�������
		// ����SIGPIPE �ź�,��������д����ֽ������˳�.��д��0 �ֽ�,�򷵻�-1.�����ڸ�i �ڵ���
		// ˯��,�ȴ��ܵ��ڳ��ռ�
		while ( !( size = ( PAGE_SIZE - 1 ) - PIPE_SIZE( *inode ) ) ) {
			wake_up( &inode->i_wait );
			if ( inode->i_count != 2 ) { /* no readers */
				current->signal |= ( 1 << ( SIGPIPE - 1 ) );
				return written ? written : -1;
			}
			sleep_on( &inode->i_wait );
		}
		// ȡ�ܵ�ͷ����������ĩ�˿ռ��ֽ���chars.�������ڻ���Ҫд����ֽ���count,���������
		// count.���chars ���ڵ�ǰ�ܵ��п��пռ䳤��size,���������size
		chars = PAGE_SIZE - PIPE_HEAD( *inode );
		if ( chars > count )
			chars = count;
		if ( chars > size )
			chars = size;
		// д���ֽڼ�����ȥ�˴ο�д����ֽ���chars,���ۼ���д�ֽ�����written
		count -= chars;
		written += chars;
		// ��size ָ��ܵ�����ͷ��,������ǰ�ܵ�����ͷ��ָ��( ǰ��chars �ֽ� )
		size = PIPE_HEAD( *inode );
		PIPE_HEAD( *inode ) += (USHORT)chars;
		PIPE_HEAD( *inode ) &= ( PAGE_SIZE - 1 );
		// ���û�����������chars ���ֽڵ��ܵ���.���ڹܵ�i �ڵ�,��i_size �ֶ����ǹܵ������ָ��.
		while ( chars-- > 0 )
			( ( CHAR * )inode->i_size )[ size++ ] = get_fs_byte( buf++ );
	}
	// ���ѵȴ���i �ڵ�Ľ���,������д����ֽ���,�˳�
	wake_up( &inode->i_wait );
	return written;
}

// �����ܵ�ϵͳ���ú���.
// ��fildes ��ָ�������д���һ���ļ����( ������ ).����ļ����ָ��һ�ܵ�i �ڵ�.fildes[ 0 ]
// ���ڶ��ܵ�������,fildes[ 1 ]������ܵ���д������.
// �ɹ�ʱ����0,����ʱ����-1
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
