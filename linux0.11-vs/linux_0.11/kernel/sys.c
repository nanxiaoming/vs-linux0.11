/*
 *  linux/kernel/sys.c
 *
 *  ( C ) 1991  Linus Torvalds
 */

#include <errno.h>

#include <linux\sched.h>
#include <linux\tty.h>
#include <linux\kernel.h>
#include <asm\segment.h>
#include <sys\times.h>
#include <sys\utsname.h>

LONG sys_ftime()
{
	return -ENOSYS;
}

LONG sys_break()
{
	return -ENOSYS;
}

LONG sys_ptrace()
{
	return -ENOSYS;
}

LONG sys_stty()
{
	return -ENOSYS;
}

LONG sys_gtty()
{
	return -ENOSYS;
}

LONG sys_rename()
{
	return -ENOSYS;
}

LONG sys_prof()
{
	return -ENOSYS;
}

LONG sys_setregid( LONG rgid, LONG egid )
{
	if ( rgid > 0 ) 
	{
		if ( ( current->gid == rgid ) || suser() )
		{
			current->gid = (USHORT)rgid;
		}
		else
		{
			return( -EPERM );
		}
	}
	if ( egid > 0 ) 
	{
		if ( ( current->gid  == egid ) ||
			 ( current->egid == egid ) ||
			 ( current->sgid == egid ) ||
			 suser()
		   )
		{
			current->egid = (USHORT)egid;
		}
		else
		{
			return( -EPERM );
		}
			
	}
	return 0;
}

LONG sys_setgid( LONG gid )
{
	return( sys_setregid( gid, gid ) );
}

LONG sys_acct()
{
	return -ENOSYS;
}

LONG sys_phys()
{
	return -ENOSYS;
}

LONG sys_lock()
{
	return -ENOSYS;
}

LONG sys_mpx()
{
	return -ENOSYS;
}

LONG sys_ulimit()
{
	return -ENOSYS;
}

LONG sys_time( LONG * tloc )
{
	LONG i;

	i = CURRENT_TIME;

	if ( tloc ) 
	{
		verify_area( tloc, 4 );
		put_fs_long( i, ( ULONG * )tloc );
	}

	return i;
}

/*
 * Unprivileged users may change the real user id to the effective uid
 * or vice versa.
 */
 /*
 * ����Ȩ���û����Լ�ʵ���û���ʶ��( real uid )�ĳ���Ч�û���ʶ��( effective uid ),��֮ҲȻ.
 */
 // ���������ʵ���Լ�/������Ч�û�ID( uid ).�������û�г����û���Ȩ,��ôֻ�ܻ�����
 // ʵ���û�ID ����Ч�û�ID.���������г����û���Ȩ,��������������Ч�ĺ�ʵ�ʵ��û�ID.
 // ������uid( saved uid )�����ó�����Чuid ֵͬ
LONG sys_setreuid( LONG ruid, LONG euid )
{
	LONG old_ruid = current->uid;

	if ( ruid > 0 ) {
		if ( ( current->euid == ruid ) ||
			( old_ruid == ruid ) ||
			suser() )
			current->uid = (USHORT)ruid;
		else
			return( -EPERM );
	}
	if ( euid > 0 ) {
		if ( ( old_ruid == euid ) ||
			( current->euid == euid ) ||
			suser() )
			current->euid = (USHORT)euid;
		else {
			current->uid = (USHORT)old_ruid;
			return( -EPERM );
		}
	}
	return 0;
}

// ���������û���( uid ).�������û�г����û���Ȩ,������ʹ��setuid()������Чuid
// ( effective uid )���ó��䱣��uid( saved uid )����ʵ��uid( real uid ).���������
// �����û���Ȩ,��ʵ��uid����Чuid �ͱ���uid �������óɲ���ָ����uid
LONG sys_setuid( LONG uid )
{
	return( sys_setreuid( uid, uid ) );
}

// ����ϵͳʱ�������.����tptr �Ǵ�1970 ��1 ��1 ��00:00:00 GMT ��ʼ��ʱ��ʱ��ֵ( �� ).
// ���ý��̱�����г����û�Ȩ��
LONG sys_stime( LONG * tptr )
{
	if ( !suser() )		// ������ǳ����û��������
		return -EPERM;
	startup_time = get_fs_long( ( ULONG * )tptr ) - jiffies / HZ;
	return 0;
}

// ��ȡ��ǰ����ʱ��.tms �ṹ�а����û�ʱ�䡢ϵͳʱ�䡢�ӽ����û�ʱ�䡢�ӽ���ϵͳʱ��.

LONG sys_times( struct tms * tbuf )
{
	if ( tbuf )
	{
		verify_area( tbuf, sizeof *tbuf );
		put_fs_long( current->utime, ( ULONG * )&tbuf->tms_utime );
		put_fs_long( current->stime, ( ULONG * )&tbuf->tms_stime );
		put_fs_long( current->cutime, ( ULONG * )&tbuf->tms_cutime );
		put_fs_long( current->cstime, ( ULONG * )&tbuf->tms_cstime );
	}
	return jiffies;
}

// ������end_data_seg ��ֵ����,����ϵͳȷʵ���㹻���ڴ�,���ҽ���û�г�Խ��������ݶδ�С
// ʱ,�ú����������ݶ�ĩβΪend_data_seg ָ����ֵ.��ֵ������ڴ����β����ҪС�ڶ�ջ
// ��β16KB.����ֵ�����ݶε��½�βֵ( �������ֵ��Ҫ��ֵ��ͬ,������д��� ).
// �ú����������û�ֱ�ӵ���,����libc �⺯�����а�װ,���ҷ���ֵҲ��һ��.
LONG sys_brk( ULONG end_data_seg )
{
	if ( end_data_seg >= current->end_code &&			// �������>�����β,����
		 end_data_seg  < current->start_stack - 16384 )	// С�ڶ�ջ-16KB,
	{
		current->brk = end_data_seg;					// �����������ݶν�βֵ.
	}
	return current->brk;								// ���ؽ��̵�ǰ�����ݶν�βֵ.
}

/*
 * This needs some heave checking ...
 * I just haven't get the stomach for it. I also don't fully
 * understand sessions/pgrp etc. Let somebody who does explain it.
 */
/*
 * ���������ҪĳЩ�ϸ�ļ�顭
 * ��ֻ��û��θ��������Щ.��Ҳ����ȫ����sessions/pgrp ��.�������˽����ǵ���������.
 */
 // ������pid ָ�����̵Ľ�����ID ���ó�pgid.�������pid=0,��ʹ�õ�ǰ���̺�.���
 // pgid Ϊ0,��ʹ�ò���pid ָ���Ľ��̵���ID ��Ϊpgid.����ú������ڽ����̴�һ��
 // �������Ƶ���һ��������,���������������������ͬһ���Ự( session ).�����������,
 // ����pgid ָ����Ҫ��������н�����ID,��ʱ����ĻỰID �����뽫Ҫ������̵���ͬ( 193 �� ).

LONG sys_setpgid( LONG pid, LONG pgid )
{
	LONG i;

	if ( !pid )
	{
		pid = current->pid;
	}	
	if ( !pgid )
	{
		pgid = current->pid;
	}
	for ( i = 0; i < NR_TASKS; i++ )
	{	
		if ( task[ i ] && task[ i ]->pid == pid ) 
		{
			if ( task[ i ]->leader )
			{
				return -EPERM;
			}

			if ( task[ i ]->session != current->session )
			{
				return -EPERM;
			}

			task[ i ]->pgrp = pgid;
			return 0;
		}
	}
	return -ESRCH;
}

// ���ص�ǰ���̵����.��getpgid( 0 )��ͬ

LONG sys_getpgrp()
{
	return current->pgrp;
}

// ����һ���Ự( session )( ��������leader=1 ),����������Ự=�����=����̺�.

LONG sys_setsid()
{
	if ( current->leader && !suser() )
	{
		return -EPERM;
	}

	current->leader		= 1;
	current->session	= current->pgrp = current->pid;
	current->tty		= -1;

	return current->pgrp;
}

// ��ȡϵͳ��Ϣ.����utsname �ṹ����5 ���ֶ�,�ֱ���:���汾����ϵͳ�����ơ�����ڵ����ơ�
// ��ǰ���м��𡢰汾�����Ӳ����������
LONG sys_uname( struct utsname * name )
{
	static struct utsname thisname = 
	{
		"linux .0", "nodename", "release ", "version ", "machine "
	};

	LONG i;

	if ( !name ) 
	{
		return -ERROR;
	}

	verify_area( name, sizeof *name );		// ��֤��������С�Ƿ���( �����ѷ�����ڴ�� ).
	
	for ( i = 0; i < sizeof *name; i++ )		// ��utsname �е���Ϣ���ֽڸ��Ƶ��û���������.
	{
		put_fs_byte( ( ( CHAR * )&thisname )[ i ], i + ( CHAR * )name );
	}
	return 0;
}

// ���õ�ǰ���̴����ļ�����������Ϊmask & 0777.������ԭ������.
LONG sys_umask( LONG mask )
{
	LONG old = current->umask;

	current->umask = mask & 0777;
	return ( old );
}
