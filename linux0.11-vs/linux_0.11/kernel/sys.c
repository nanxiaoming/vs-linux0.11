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
 * 无特权的用户可以见实际用户标识符( real uid )改成有效用户标识符( effective uid ),反之也然.
 */
 // 设置任务的实际以及/或者有效用户ID( uid ).如果任务没有超级用户特权,那么只能互换其
 // 实际用户ID 和有效用户ID.如果任务具有超级用户特权,就能任意设置有效的和实际的用户ID.
 // 保留的uid( saved uid )被设置成与有效uid 同值
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

// 设置任务用户号( uid ).如果任务没有超级用户特权,它可以使用setuid()将其有效uid
// ( effective uid )设置成其保留uid( saved uid )或其实际uid( real uid ).如果任务有
// 超级用户特权,则实际uid、有效uid 和保留uid 都被设置成参数指定的uid
LONG sys_setuid( LONG uid )
{
	return( sys_setreuid( uid, uid ) );
}

// 设置系统时间和日期.参数tptr 是从1970 年1 月1 日00:00:00 GMT 开始计时的时间值( 秒 ).
// 调用进程必须具有超级用户权限
LONG sys_stime( LONG * tptr )
{
	if ( !suser() )		// 如果不是超级用户则出错返回
		return -EPERM;
	startup_time = get_fs_long( ( ULONG * )tptr ) - jiffies / HZ;
	return 0;
}

// 获取当前任务时间.tms 结构中包括用户时间、系统时间、子进程用户时间、子进程系统时间.

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

// 当参数end_data_seg 数值合理,并且系统确实有足够的内存,而且进程没有超越其最大数据段大小
// 时,该函数设置数据段末尾为end_data_seg 指定的值.该值必须大于代码结尾并且要小于堆栈
// 结尾16KB.返回值是数据段的新结尾值( 如果返回值与要求值不同,则表明有错发生 ).
// 该函数并不被用户直接调用,而由libc 库函数进行包装,并且返回值也不一样.
LONG sys_brk( ULONG end_data_seg )
{
	if ( end_data_seg >= current->end_code &&			// 如果参数>代码结尾,并且
		 end_data_seg  < current->start_stack - 16384 )	// 小于堆栈-16KB,
	{
		current->brk = end_data_seg;					// 则设置新数据段结尾值.
	}
	return current->brk;								// 返回进程当前的数据段结尾值.
}

/*
 * This needs some heave checking ...
 * I just haven't get the stomach for it. I also don't fully
 * understand sessions/pgrp etc. Let somebody who does explain it.
 */
/*
 * 下面代码需要某些严格的检查…
 * 我只是没有胃口来做这些.我也不完全明白sessions/pgrp 等.还是让了解它们的人来做吧.
 */
 // 将参数pid 指定进程的进程组ID 设置成pgid.如果参数pid=0,则使用当前进程号.如果
 // pgid 为0,则使用参数pid 指定的进程的组ID 作为pgid.如果该函数用于将进程从一个
 // 进程组移到另一个进程组,则这两个进程组必须属于同一个会话( session ).在这种情况下,
 // 参数pgid 指定了要加入的现有进程组ID,此时该组的会话ID 必须与将要加入进程的相同( 193 行 ).

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

// 返回当前进程的组号.与getpgid( 0 )等同

LONG sys_getpgrp()
{
	return current->pgrp;
}

// 创建一个会话( session )( 即设置其leader=1 ),并且设置其会话=其组号=其进程号.

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

// 获取系统信息.其中utsname 结构包含5 个字段,分别是:本版本操作系统的名称、网络节点名称、
// 当前发行级别、版本级别和硬件类型名称
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

	verify_area( name, sizeof *name );		// 验证缓冲区大小是否超限( 超出已分配的内存等 ).
	
	for ( i = 0; i < sizeof *name; i++ )		// 将utsname 中的信息逐字节复制到用户缓冲区中.
	{
		put_fs_byte( ( ( CHAR * )&thisname )[ i ], i + ( CHAR * )name );
	}
	return 0;
}

// 设置当前进程创建文件属性屏蔽码为mask & 0777.并返回原屏蔽码.
LONG sys_umask( LONG mask )
{
	LONG old = current->umask;

	current->umask = mask & 0777;
	return ( old );
}
