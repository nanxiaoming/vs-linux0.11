/*
 *  linux/fs/open.c
 *
 *  ( C ) 1991  Linus Torvalds
 */

#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys\types.h>
#include <utime.h>
#include <sys\stat.h>

#include <linux\sched.h>
#include <linux\tty.h>
#include <linux\kernel.h>
#include <asm\segment.h>

LONG sys_ustat( LONG dev, struct ustat * ubuf )
{
	return -ENOSYS;
}

// �����ļ����ʺ��޸�ʱ��.
// ����filename ���ļ���,times �Ƿ��ʺ��޸�ʱ��ṹָ��.
// ���times ָ�벻ΪNULL,��ȡutimbuf �ṹ�е�ʱ����Ϣ�������ļ��ķ��ʺ��޸�ʱ��.���
// times ָ����NULL,��ȡϵͳ��ǰʱ��������ָ���ļ��ķ��ʺ��޸�ʱ����
LONG sys_utime( CHAR * filename, struct utimbuf * times )
{
	M_Inode		*	inode;
	LONG			actime, modtime;

	// �����ļ���Ѱ�Ҷ�Ӧ��i �ڵ�,���û���ҵ�,�򷵻س�����
	if ( !( inode = namei( filename ) ) )
	{
		return -ENOENT;
	}

	// ������ʺ��޸�ʱ�����ݽṹָ�벻ΪNULL,��ӽṹ�ж�ȡ�û����õ�ʱ��ֵ
	if ( times ) 
	{
		actime	= get_fs_long( ( ULONG * )&times->actime	);
		modtime = get_fs_long( ( ULONG * )&times->modtime	);
		// ���򽫷��ʺ��޸�ʱ����Ϊ��ǰʱ��
	}
	else
	{
		actime = modtime = CURRENT_TIME;
	}

	// �޸�i �ڵ��еķ���ʱ���ֶκ��޸�ʱ���ֶ�
	inode->i_atime = actime;
	inode->i_mtime = modtime;
	// ��i �ڵ����޸ı�־,�ͷŸýڵ�,������0
	inode->i_dirt = 1;

	iput( inode );

	return 0;
}

/*
 * XXX should we use the real or effective uid?  BSD uses the real uid,
 * so as to make this call useful to setuid programs.
 */
/*
 * �ļ�����XXX,���Ǹ�����ʵ�û�id ������Ч�û�id?BSD ϵͳʹ������ʵ�û�id,
 * ��ʹ�õ��ÿ��Թ�setuid ����ʹ��.( ע:POSIX ��׼����ʹ����ʵ�û�ID )
 */
// �����ļ��ķ���Ȩ��.
// ����filename ���ļ���,mode ��������,��R_OK( 4 )��W_OK( 2 )��X_OK( 1 )��F_OK( 0 )���.
// ��������������Ļ�,�򷵻�0,���򷵻س�����
LONG sys_access( const CHAR * filename, LONG mode )
{
	M_Inode *	inode;
	LONG		res, i_mode;

	// �������ɵ�3 λ���,���������и߱���λ
	mode &= 0007;

	// ����ļ�����Ӧ��i �ڵ㲻����,�򷵻س�����
	if ( !( inode = namei( filename ) ) )
	{
		return -EACCES;
	}

	// ȡ�ļ���������,���ͷŸ�i �ڵ�
	i_mode = res = inode->i_mode & 0777;

	iput( inode );

	// �����ǰ�����Ǹ��ļ�������,��ȡ�ļ���������
	if ( current->uid == inode->i_uid )
		res >>= 6;
	// ���������ǰ����������ļ�ͬ��һ��,��ȡ�ļ�������
	else if ( current->gid == inode->i_gid )
		res >>= 6;

	// ����ļ����Ծ��в�ѯ������λ,��������,����0
	if ( ( res & 0007 & mode ) == mode )
	{
		return 0;
	}
	/*
	 * XXX we are doing this test last because we really should be
	 * swapping the effective with the real user id ( temporarily ),
	 * and then calling suser() routine.  If we do call the
	 * suser() routine, it needs to be called last.
	 */
	/*
	 * XXX ��������������Ĳ���,��Ϊ����ʵ������Ҫ������Ч�û�id ��
	 * ��ʵ�û�id( ��ʱ�� ),Ȼ��ŵ���suser()����.�������ȷʵҪ����
	 * suser()����,����Ҫ���ű�����. 
	 */
	// �����ǰ�û�id Ϊ0( �����û� )����������ִ��λ��0 ���ļ����Ա��κ��˷���,�򷵻�0.

	if ( ( !current->uid ) && ( !( mode & 1 ) || ( i_mode & 0111 ) ) )
	{
		return 0;
	}
	// ���򷵻س�����
	return -EACCES;
}


// �ı䵱ǰ����Ŀ¼ϵͳ���ú���.
// ����filename ��Ŀ¼��.
// �����ɹ��򷵻�0,���򷵻س�����.
LONG sys_chdir( const CHAR * filename )
{
	M_Inode * inode;

	if ( !( inode = namei( filename ) ) )
		return -ENOENT;

	if ( !S_ISDIR( inode->i_mode ) ) 
	{
		iput( inode );
		return -ENOTDIR;
	}

	// �ͷŵ�ǰ����ԭ����Ŀ¼i �ڵ�,��ָ������õĹ���Ŀ¼i �ڵ�.����0
	iput( current->pwd );

	current->pwd = inode;

	return ( 0 );
}

// �ı��Ŀ¼ϵͳ���ú���.
// ��ָ����·������Ϊ��Ŀ¼'/'.
// ��������ɹ��򷵻�0,���򷵻س�����.

LONG sys_chroot( const CHAR * filename )
{
	M_Inode * inode;

	if ( !( inode = namei( filename ) ) )
		return -ENOENT;

	if ( !S_ISDIR( inode->i_mode ) ) 
	{
		iput( inode );
		return -ENOTDIR;
	}

	iput( current->root );
	current->root = inode;

	return ( 0 );
}

// �޸��ļ�����ϵͳ���ú���.
// ����filename ���ļ���,mode ���µ��ļ�����.
// �������ɹ��򷵻�0,���򷵻س�����

LONG sys_chmod( const CHAR * filename, LONG mode )
{
	M_Inode * inode;

	// ����ļ�����Ӧ��i �ڵ㲻����,�򷵻س�����
	if ( !( inode = namei( filename ) ) )
	{
		return -ENOENT;
	}

	// �����ǰ���̵���Ч�û�id �������ļ�i �ڵ���û�id,���ҵ�ǰ���̲��ǳ����û�,���ͷŸ�
	// �ļ�i �ڵ�,���س�����
	if ( ( current->euid != inode->i_uid ) && !suser() ) 
	{
		iput( inode );
		return -EACCES;
	}
	// ��������i �ڵ���ļ�����,���ø�i �ڵ����޸ı�־.�ͷŸ�i �ڵ�,����0
	inode->i_mode = ( mode & 07777 ) | ( inode->i_mode & ~07777 );
	inode->i_dirt = 1;
	iput( inode );
	return 0;
}

// �޸��ļ�����ϵͳ���ú���.
// ����filename ���ļ���,uid ���û���ʶ��( �û�id ),gid ����id.
// �������ɹ��򷵻�0,���򷵻س�����
LONG sys_chown( const CHAR * filename, LONG uid, LONG gid )
{
	M_Inode * inode;

	// ����ļ�����Ӧ��i �ڵ㲻����,�򷵻س�����
	if ( !( inode = namei( filename ) ) )
		return -ENOENT;

	// ����ǰ���̲��ǳ����û�,���ͷŸ�i �ڵ�,���س�����
	if ( !suser() ) 
	{
		iput( inode );
		return -EACCES;
	}
	// �����ļ���Ӧi �ڵ���û�id ����id,����i �ڵ��Ѿ��޸ı�־,�ͷŸ�i �ڵ�,����0
	inode->i_uid = (USHORT)uid;
	inode->i_gid = (UCHAR)gid;
	inode->i_dirt = 1;

	iput( inode );
	return 0;
}

// ��( �򴴽� )�ļ�ϵͳ���ú���.
// ����filename ���ļ���,flag �Ǵ��ļ���־:ֻ��O_RDONLY��ֻдO_WRONLY ���дO_RDWR,
// �Լ�O_CREAT��O_EXCL��O_APPEND ������һЩ��־�����,��������������һ�����ļ�,��mode
// ����ָ��ʹ���ļ����������,��Щ������S_IRWXU( �ļ��������ж���д��ִ��Ȩ�� )��S_IRUSR
// ( �û����ж��ļ�Ȩ�� )��S_IRWXG( ���Ա���ж���д��ִ��Ȩ�� )�ȵ�.�����´������ļ�,��Щ
// ����ֻӦ���ڽ������ļ��ķ���,������ֻ���ļ��Ĵ򿪵���Ҳ������һ���ɶ�д���ļ����.
// �������ɹ��򷵻��ļ����( �ļ������� ),���򷵻س�����.( �μ�sys/stat.h, fcntl.h )

LONG sys_open( const CHAR * filename, LONG flag, LONG mode )
{
	M_Inode		*	inode;
	File		*	f;
	LONG			i, fd;

	// ���û����õ�ģʽ����̵�ģʽ����������,������ɵ��ļ�ģʽ
	mode &= 0777 & ~current->umask;

	// �������̽ṹ���ļ��ṹָ������,����һ��������,���Ѿ�û�п�����,�򷵻س�����.
	for ( fd = 0; fd < NR_OPEN; fd++ )
	{
		if ( !current->filp[ fd ] )
			break;
	}

	if ( fd >= NR_OPEN )
	{
		return -EINVAL;
	}

	// ����ִ��ʱ�ر��ļ����λͼ,��λ��Ӧ����λ
	current->close_on_exec &= ~( 1 << fd );

	// ��f ָ���ļ������鿪ʼ��.���������ļ��ṹ��( ������ü���Ϊ0 ���� ),���Ѿ�û�п���
	// �ļ���ṹ��,�򷵻س�����

	f = 0 + file_table;

	for ( i = 0; i < NR_FILE; i++, f++ )
		if ( !f->f_count ) 
			break;

	if ( i >= NR_FILE )
		return -EINVAL;

	// �ý��̵Ķ�Ӧ�ļ�������ļ��ṹָ��ָ�����������ļ��ṹ,���������ü�������1.

	( current->filp[ fd ] = f )->f_count++;

	// ���ú���ִ�д򿪲���,������ֵС��0,��˵������,�ͷŸ����뵽���ļ��ṹ,���س�����.
	if ( ( i = open_namei( filename, flag, mode, &inode ) ) < 0 ) 
	{
		current->filp[ fd ] = NULL;
		f->f_count = 0;
		return i;
	}

	/* ttys are somewhat special ( ttyxx major==4, tty major==5 ) */
	/* ttys ��Щ����( ttyxx ����==4,tty ����==5 )*/
	// ������ַ��豸�ļ�,��ô����豸����4 �Ļ�,�����õ�ǰ���̵�tty ��Ϊ��i �ڵ�����豸��.
	// �����õ�ǰ����tty ��Ӧ��tty ����ĸ�������ŵ��ڽ��̵ĸ��������

	if ( S_ISCHR( inode->i_mode ) )
	{
		if ( MAJOR( inode->i_zone[ 0 ] ) == 4 ) 
		{
			if ( current->leader && current->tty < 0 ) 
			{
				current->tty = MINOR( inode->i_zone[ 0 ] );
				tty_table[ current->tty ].pgrp = current->pgrp;
			}
			// ����������ַ��ļ��豸����5 �Ļ�,����ǰ����û��tty,��˵������,�ͷ�i �ڵ�����뵽��
			// �ļ��ṹ,���س�����
		}
		else if ( MAJOR( inode->i_zone[ 0 ] ) == 5 )
		{
			if ( current->tty < 0 )
			{
				iput( inode );
				current->filp[ fd ] = NULL;
				f->f_count = 0;
				return -EPERM;
			}
		}
	}

	/* Likewise with block-devices: check for floppy_change */
	/* ͬ�����ڿ��豸�ļ�:��Ҫ�����Ƭ�Ƿ񱻸��� */
	// ����򿪵��ǿ��豸�ļ�,������Ƭ�Ƿ����,����������Ҫ�Ǹ��ٻ����ж�Ӧ���豸������
	// �����ʧЧ
	if ( S_ISBLK( inode->i_mode ) )
	{
		check_disk_change( inode->i_zone[ 0 ] );
	}

	// ��ʼ���ļ��ṹ.���ļ��ṹ���Ժͱ�־,�þ�����ü���Ϊ1,����i �ڵ��ֶ�,�ļ���дָ��
	// ��ʼ��Ϊ0.�����ļ����

	f->f_mode	= inode->i_mode;
	f->f_flags	= (USHORT)flag;
	f->f_count	= 1;
	f->f_inode	= inode;
	f->f_pos	= 0;

	return ( fd );
}

// �����ļ�ϵͳ���ú���.
// ����pathname ��·����,mode �������sys_open()������ͬ.
// �ɹ��򷵻��ļ����,���򷵻س�����
LONG sys_creat( const CHAR * pathname, LONG mode )
{
	return sys_open( pathname, O_CREAT | O_TRUNC, mode );
}

// �ر��ļ�ϵͳ���ú���.
// ����fd ���ļ����.
// �ɹ��򷵻�0,���򷵻س�����
LONG sys_close( ULONG fd )
{
	File * filp;

	if ( fd >= NR_OPEN )
		return -EINVAL;

	// ��λ���̵�ִ��ʱ�ر��ļ����λͼ��Ӧλ
	current->close_on_exec &= ~( 1 << fd );

	// �����ļ������Ӧ���ļ��ṹָ����NULL,�򷵻س�����
	if ( !( filp = current->filp[ fd ] ) )
		return -EINVAL;

	// �ø��ļ�������ļ��ṹָ��ΪNULL
	current->filp[ fd ] = NULL;

	// ���ڹر��ļ�֮ǰ,��Ӧ�ļ��ṹ�еľ�����ü����Ѿ�Ϊ0,��˵���ں˳���,����
	if ( filp->f_count == 0 )
	{
		panic( "Close: file count is 0" );
	}

	// ���򽫶�Ӧ�ļ��ṹ�ľ�����ü�����1,�������Ϊ0,�򷵻�0( �ɹ� ).���ѵ���0,˵����
	// �ļ��Ѿ�û�о������,���ͷŸ��ļ�i �ڵ�,����0
	if ( --filp->f_count )
	{
		return ( 0 );
	}

	iput( filp->f_inode );

	return ( 0 );
}
