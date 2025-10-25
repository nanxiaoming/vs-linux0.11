/*
*  linux/kernel/exit.c
*
*  ( C ) 1991  Linus Torvalds
*/

#include <errno.h>
#include <signal.h>
#include <sys\wait.h>

#include <linux\sched.h>
#include <linux\kernel.h>
#include <linux\tty.h>
#include <asm\segment.h>

LONG sys_pause();
LONG sys_close( LONG fd );

// 释放指定进程( 任务 )
VOID release( Task_Struct * p )
{
	LONG i;

	if ( !p )
		return;
	for ( i = 1; i < NR_TASKS; i++ )
		if ( task[ i ] == p ) {
			task[ i ] = NULL;		// 置空该任务项并释放相关内存页
			free_page( ( LONG )p );
			schedule();			// 重新调度
			return;
		}
	panic( "trying to release non-existent task" );
}

// 向指定任务( *p )发送信号( sig ),权限为priv
static __inline LONG send_sig( LONG sig, Task_Struct * p, LONG priv )
{
	// 若信号不正确或任务指针为空则出错退出
	if ( !p || sig<1 || sig>32 )
		return -EINVAL;

	// 若有权或进程有效用户标识符( euid )就是指定进程的euid 或者是超级用户,则在进程位图中添加
	// 该信号,否则出错退出.其中suser()定义为( current->euid==0 ),用于判断是否超级用户.

	if ( priv || ( current->euid == p->euid ) || suser() )
		p->signal |= ( 1 << ( sig - 1 ) );
	else
		return -EPERM;
	return 0;
}

// 终止会话( session )
static VOID kill_session()
{
	Task_Struct **p = NR_TASKS + task;	// 指针*p 首先指向任务数组最末端

	// 对于所有的任务( 除任务0 以外 ),如果其会话等于当前进程的会话就向它发送挂断进程信号.
	while ( --p > &FIRST_TASK ) 
	{
		if ( *p && ( *p )->session == current->session )
			( *p )->signal |= 1 << ( SIGHUP - 1 );
	}
}

/*
 * XXX need to check permissions needed to send signals to process
 * groups, etc. etc.  kill() permissions semantics are tricky!
 */
// kill()系统调用可用于向任何进程或进程组发送任何信号.
// 如果pid 值>0,则信号被发送给pid.
// 如果pid=0,那么信号就会被发送给当前进程的进程组中的所有进程.
// 如果pid=-1,则信号sig 就会发送给除第一个进程外的所有进程.
// 如果pid < -1,则信号sig 将发送给进程组-pid 的所有进程.
// 如果信号sig 为0,则不发送信号,但仍会进行错误检查.如果成功则返回0
LONG sys_kill( LONG pid, LONG sig )
{
	Task_Struct **p = NR_TASKS + task;
	LONG err, retval = 0;

	if ( !pid ) while ( --p > &FIRST_TASK ) {
		if ( *p && ( *p )->pgrp == current->pid )
			if ( err = send_sig( sig, *p, 1 ) )
				retval = err;
	}
	else if ( pid > 0 ) while ( --p > &FIRST_TASK ) {
		if ( *p && ( *p )->pid == pid )
			if ( err = send_sig( sig, *p, 0 ) )
				retval = err;
	}
	else if ( pid == -1 ) while ( --p > &FIRST_TASK )
		if ( err = send_sig( sig, *p, 0 ) )
			retval = err;
		else while ( --p > &FIRST_TASK )
			if ( *p && ( *p )->pgrp == -pid )
				if ( err = send_sig( sig, *p, 0 ) )
					retval = err;
	return retval;
}

// 通知父进程 -- 向进程pid 发送信号SIGCHLD:子进程将停止或终止.
// 如果没有找到父进程,则自己释放
static VOID tell_father( LONG pid )
{
	LONG i;

	if ( pid )
		for ( i = 0; i < NR_TASKS; i++ ) {
			if ( !task[ i ] )
				continue;
			if ( task[ i ]->pid != pid )
				continue;
			task[ i ]->signal |= ( 1 << ( SIGCHLD - 1 ) );
			return;
		}
	/* if we don't find any fathers, we just release ourselves */
	/* This is not really OK. Must change it to make father 1 */
	printk( "BAD BAD - no father found\n\r" );
	release( current );
}

// 程序退出处理程序.在系统调用的中断处理程序中被调用 code-错误码
LONG do_exit( LONG code )
{
	LONG i;

	// 释放当前进程代码段和数据段所占的内存页

	free_page_tables( get_base( current->ldt[ 1 ] ), get_limit( 0x0f ) );
	free_page_tables( get_base( current->ldt[ 2 ] ), get_limit( 0x17 ) );

	// 如果当前进程有子进程,就将子进程的father 置为1( 其父进程改为进程1 ).如果该子进程已经
	// 处于僵死( ZOMBIE )状态,则向进程1 发送子进程终止信号SIGCHLD
	for ( i = 0; i < NR_TASKS; i++ )
	{
		if ( task[ i ] && task[ i ]->father == current->pid ) 
		{
			task[ i ]->father = 1;

			if ( task[ i ]->state == TASK_ZOMBIE )
			{
				/* assumption task[ 1 ] is always init */
				send_sig( SIGCHLD, task[ 1 ], 1 );
			}
		}
	}
	// 关闭当前进程打开着的所有文件
	for ( i = 0; i < NR_OPEN; i++ )
		if ( current->filp[ i ] )
			sys_close( i );
	// 对当前进程工作目录pwd、根目录root 以及运行程序的i 节点进行同步操作,并分别置空
	iput( current->pwd );
	current->pwd = NULL;
	iput( current->root );
	current->root = NULL;
	iput( current->executable );
	// 如果当前进程是领头( leader )进程并且其有控制的终端,则释放该终端
	current->executable = NULL;
	if ( current->leader && current->tty >= 0 )
		tty_table[ current->tty ].pgrp = 0;
	// 如果当前进程上次使用过协处理器,则将last_task_used_math 置空
	if ( last_task_used_math == current )
		last_task_used_math = NULL;
	// 如果当前进程是leader 进程,则终止所有相关进程
	if ( current->leader )
		kill_session();
	// 把当前进程置为僵死状态,并设置退出码
	current->state = TASK_ZOMBIE;
	current->exit_code = code;
	// 通知父进程,也即向父进程发送信号SIGCHLD -- 子进程将停止或终止
	tell_father( current->father );
	schedule();
	return ( -1 );	/* just to suppress warnings */
}

LONG sys_exit( LONG error_code )
{
	return do_exit( ( error_code & 0xff ) << 8 );
}

// 系统调用waitpid().挂起当前进程,直到pid 指定的子进程退出( 终止 )或者收到要求终止
// 该进程的信号,或者是需要调用一个信号句柄( 信号处理程序 ).如果pid 所指的子进程早已
// 退出( 已成所谓的僵死进程 ),则本调用将立刻返回.子进程使用的所有资源将释放.
// 如果pid > 0, 表示等待进程号等于pid 的子进程.
// 如果pid = 0, 表示等待进程组号等于当前进程的任何子进程.
// 如果pid < -1, 表示等待进程组号等于pid 绝对值的任何子进程.
// [ 如果pid = -1, 表示等待任何子进程. ]
// 若options = WUNTRACED,表示如果子进程是停止的,也马上返回.
// 若options = WNOHANG,表示如果没有子进程退出或终止就马上返回.
// 如果stat_addr 不为空,则就将状态信息保存到那里
LONG sys_waitpid( pid_t pid, ULONG * stat_addr, LONG options )
{
	LONG flag, code;
	Task_Struct ** p;

	verify_area( stat_addr, 4 );
repeat:
	flag = 0;
	for ( p = &LAST_TASK; p > &FIRST_TASK; --p ) {
		if ( !*p || *p == current )
			continue;
		if ( ( *p )->father != current->pid )
			continue;
		if ( pid > 0 ) {
			if ( ( *p )->pid != pid )
				continue;
		}
		else if ( !pid ) {
			if ( ( *p )->pgrp != current->pgrp )
				continue;
		}
		else if ( pid != -1 ) {
			if ( ( *p )->pgrp != -pid )
				continue;
		}
		switch ( ( *p )->state ) {
		case TASK_STOPPED:
			if ( !( options & WUNTRACED ) )
				continue;
			put_fs_long( 0x7f, stat_addr );
			return ( *p )->pid;
		case TASK_ZOMBIE:
			current->cutime += ( *p )->utime;
			current->cstime += ( *p )->stime;
			flag = ( *p )->pid;
			code = ( *p )->exit_code;
			release( *p );
			put_fs_long( code, stat_addr );
			return flag;
		default:
			flag = 1;
			continue;
		}
	}
	if ( flag ) {
		if ( options & WNOHANG )
			return 0;
		current->state = TASK_INTERRUPTIBLE;
		schedule();
		if ( !( current->signal &= ~( 1 << ( SIGCHLD - 1 ) ) ) )
			goto repeat;
		else
			return -EINTR;
	}
	return -ECHILD;
}


