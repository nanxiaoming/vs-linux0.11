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

 // �����ļ�״̬��Ϣ.
 // ����inode ���ļ���Ӧ��i �ڵ�,statbuf ��stat �ļ�״̬�ṹָ��,���ڴ��ȡ�õ�״̬��Ϣ.

static VOID cp_stat( M_Inode * inode, struct stat * statbuf )
{
	struct stat tmp;
	LONG i;

	// ������֤( ����� )������ݵ��ڴ�ռ�
	verify_area( statbuf, sizeof ( *statbuf ) );
	// Ȼ����ʱ������Ӧ�ڵ��ϵ���Ϣ
	tmp.st_dev = inode->i_dev;			// �ļ����ڵ��豸��.
	tmp.st_ino = inode->i_num;			// �ļ�i �ڵ��.
	tmp.st_mode = inode->i_mode;		// �ļ�����.
	tmp.st_nlink = inode->i_nlinks;		// �ļ���������.
	tmp.st_uid = inode->i_uid;			// �ļ����û�id.
	tmp.st_gid = inode->i_gid;			// �ļ�����id.
	tmp.st_rdev = inode->i_zone[ 0 ];		// �豸��( ����ļ���������ַ��ļ�����ļ� ).
	tmp.st_size = inode->i_size;		// �ļ���С( �ֽ��� )( ����ļ��ǳ����ļ� ).
	tmp.st_atime = inode->i_atime;		// ������ʱ��.
	tmp.st_mtime = inode->i_mtime;		// ����޸�ʱ��.
	tmp.st_ctime = inode->i_ctime;		// ���ڵ��޸�ʱ��.

	// �����Щ״̬��Ϣ���Ƶ��û���������
	for ( i = 0; i < sizeof ( tmp ); i++ )
		put_fs_byte( ( ( CHAR * )&tmp )[ i ], &( ( CHAR * )statbuf )[ i ] );
}

// �ļ�״̬ϵͳ���ú��� - �����ļ�����ȡ�ļ�״̬��Ϣ.
// ����filename ��ָ�����ļ���,statbuf �Ǵ��״̬��Ϣ�Ļ�����ָ��.
// ����0,�������򷵻س�����

LONG sys_stat( CHAR * filename, struct stat * statbuf )
{
	M_Inode * inode;

	// ���ȸ����ļ����ҳ���Ӧ��i �ڵ�,�������򷵻ش�����
	if ( !( inode = namei( filename ) ) )
		return -ENOENT;
	// ��i �ڵ��ϵ��ļ�״̬��Ϣ���Ƶ��û���������,���ͷŸ�i �ڵ�.
	cp_stat( inode, statbuf );
	iput( inode );
	return 0;
}

// �ļ�״̬ϵͳ���� - �����ļ������ȡ�ļ�״̬��Ϣ.
// ����fd ��ָ���ļ��ľ��( ������ ),statbuf �Ǵ��״̬��Ϣ�Ļ�����ָ��.
// ����0,�������򷵻س�����
LONG sys_fstat( ULONG fd, struct stat * statbuf )
{
	File * f;
	M_Inode * inode;

	if ( fd >= NR_OPEN || !( f = current->filp[ fd ] ) || !( inode = f->f_inode ) )
		return -EBADF;
	cp_stat( inode, statbuf );
	return 0;
}
