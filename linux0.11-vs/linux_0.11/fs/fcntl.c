/*
 *  linux/fs/fcntl.c
 *
 *  ( C ) 1991  Linus Torvalds
 */

#include <string.h>
#include <errno.h>
#include <linux\sched.h>
#include <linux\kernel.h>
#include <asm\segment.h>

#include <fcntl.h>
#include <sys\stat.h>

extern LONG sys_close( LONG fd );

// �����ļ����( ������ ).
// ����fd �������Ƶ��ļ����,arg ָ�����ļ��������С��ֵ.
// �������ļ�����������.
static LONG dupfd( ULONG fd, ULONG arg )
{
	// ����ļ����ֵ����һ�����������ļ���NR_OPEN,���߸þ�����ļ��ṹ������,�����,
	// ���س����벢�˳�.

	if ( fd >= NR_OPEN || !current->filp[ fd ] )
	{
		return -EBADF;
	}
	// ���ָ�����¾��ֵarg ���������ļ���,�����,���س����벢�˳�
	if ( arg >= NR_OPEN )
	{
		return -EINVAL;
	}
	// �ڵ�ǰ���̵��ļ��ṹָ��������Ѱ�������Ŵ��ڵ���arg ����û��ʹ�õ���
	while ( arg < NR_OPEN )
	{
		if ( current->filp[ arg ] )
		{
			arg++;
		}
		else
		{
			break;
		}
	}

	// ����ҵ����¾��ֵarg ���������ļ���,�����,���س����벢�˳�
	if ( arg >= NR_OPEN )
	{
		return -EMFILE;
	}

	// ��ִ��ʱ�رձ�־λͼ�и�λ�þ��λ.Ҳ��������exec()�ຯ��ʱ���رոþ��

	current->close_on_exec &= ~( 1 << arg );

	// ����ļ��ṹָ�����ԭ���fd ��ָ��,�����ļ����ü�����1
	( current->filp[ arg ] = current->filp[ fd ] )->f_count++;

	return arg;
}

// �����ļ����ϵͳ���ú���.
// ����ָ���ļ����oldfd,�¾��ֵ����newfd.���newfd �Ѿ���,�����ȹر�֮
LONG sys_dup2( ULONG oldfd, ULONG newfd )
{
	sys_close( newfd );				// �����newfd �Ѿ���,�����ȹر�֮.

	return dupfd( oldfd, newfd );	// ���Ʋ������¾��.
}

// �����ļ����ϵͳ���ú���.
// ����ָ���ļ����oldfd,�¾����ֵ�ǵ�ǰ��С��δ�þ��
LONG sys_dup( ULONG fildes )
{
	return dupfd( fildes, 0 );
}

// �ļ�����ϵͳ���ú���.
// ����fd ���ļ����,cmd �ǲ�������( �μ�include/fcntl.h,23-30 �� )
LONG sys_fcntl( ULONG fd, ULONG cmd, ULONG arg )
{
	File * filp;

	// ����ļ����ֵ����һ�����������ļ���NR_OPEN,���߸þ�����ļ��ṹָ��Ϊ��,�����,
	// ���س����벢�˳�
	if ( fd >= NR_OPEN || !( filp = current->filp[ fd ] ) )
	{
		return -EBADF;
	}

	switch ( cmd ) 
	{
	case F_DUPFD:	// �����ļ����
	{
		return dupfd( fd, arg );
	}
	case F_GETFD:	// ȡ�ļ������ִ��ʱ�رձ�־
	{
		return ( current->close_on_exec >> fd ) & 1;
	}
	case F_SETFD:	// ���þ��ִ��ʱ�رձ�־.arg λ0 ��λ������,����ر�
	{
		if ( arg & 1 )
		{
			current->close_on_exec |= ( 1 << fd );
		}
		else
		{
			current->close_on_exec &= ~( 1 << fd );
		}	
		return 0;
	}
	case F_GETFL:	// ȡ�ļ�״̬��־�ͷ���ģʽ
	{
		return filp->f_flags;
	}
	case F_SETFL:	// �����ļ�״̬�ͷ���ģʽ( ����arg ������ӡ���������־ )
	{
		filp->f_flags &= ~( O_APPEND | O_NONBLOCK );
		filp->f_flags |= arg & ( O_APPEND | O_NONBLOCK );
		return 0;
	}
	case F_GETLK:	
	case F_SETLK:	
	case F_SETLKW:
	{
		return -1;
	}
	default:
	{
		return -1;
	}
	}
}
