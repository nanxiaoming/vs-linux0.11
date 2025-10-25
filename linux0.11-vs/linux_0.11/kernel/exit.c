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

// �ͷ�ָ������( ���� )
VOID release( Task_Struct * p )
{
	LONG i;

	if ( !p )
		return;
	for ( i = 1; i < NR_TASKS; i++ )
		if ( task[ i ] == p ) {
			task[ i ] = NULL;		// �ÿո�������ͷ�����ڴ�ҳ
			free_page( ( LONG )p );
			schedule();			// ���µ���
			return;
		}
	panic( "trying to release non-existent task" );
}

// ��ָ������( *p )�����ź�( sig ),Ȩ��Ϊpriv
static __inline LONG send_sig( LONG sig, Task_Struct * p, LONG priv )
{
	// ���źŲ���ȷ������ָ��Ϊ��������˳�
	if ( !p || sig<1 || sig>32 )
		return -EINVAL;

	// ����Ȩ�������Ч�û���ʶ��( euid )����ָ�����̵�euid �����ǳ����û�,���ڽ���λͼ�����
	// ���ź�,��������˳�.����suser()����Ϊ( current->euid==0 ),�����ж��Ƿ񳬼��û�.

	if ( priv || ( current->euid == p->euid ) || suser() )
		p->signal |= ( 1 << ( sig - 1 ) );
	else
		return -EPERM;
	return 0;
}

// ��ֹ�Ự( session )
static VOID kill_session()
{
	Task_Struct **p = NR_TASKS + task;	// ָ��*p ����ָ������������ĩ��

	// �������е�����( ������0 ���� ),�����Ự���ڵ�ǰ���̵ĻỰ���������͹ҶϽ����ź�.
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
// kill()ϵͳ���ÿ��������κν��̻�����鷢���κ��ź�.
// ���pid ֵ>0,���źű����͸�pid.
// ���pid=0,��ô�źžͻᱻ���͸���ǰ���̵Ľ������е����н���.
// ���pid=-1,���ź�sig �ͻᷢ�͸�����һ������������н���.
// ���pid < -1,���ź�sig �����͸�������-pid �����н���.
// ����ź�sig Ϊ0,�򲻷����ź�,���Ի���д�����.����ɹ��򷵻�0
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

// ֪ͨ������ -- �����pid �����ź�SIGCHLD:�ӽ��̽�ֹͣ����ֹ.
// ���û���ҵ�������,���Լ��ͷ�
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

// �����˳��������.��ϵͳ���õ��жϴ�������б����� code-������
LONG do_exit( LONG code )
{
	LONG i;

	// �ͷŵ�ǰ���̴���κ����ݶ���ռ���ڴ�ҳ

	free_page_tables( get_base( current->ldt[ 1 ] ), get_limit( 0x0f ) );
	free_page_tables( get_base( current->ldt[ 2 ] ), get_limit( 0x17 ) );

	// �����ǰ�������ӽ���,�ͽ��ӽ��̵�father ��Ϊ1( �丸���̸�Ϊ����1 ).������ӽ����Ѿ�
	// ���ڽ���( ZOMBIE )״̬,�������1 �����ӽ�����ֹ�ź�SIGCHLD
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
	// �رյ�ǰ���̴��ŵ������ļ�
	for ( i = 0; i < NR_OPEN; i++ )
		if ( current->filp[ i ] )
			sys_close( i );
	// �Ե�ǰ���̹���Ŀ¼pwd����Ŀ¼root �Լ����г����i �ڵ����ͬ������,���ֱ��ÿ�
	iput( current->pwd );
	current->pwd = NULL;
	iput( current->root );
	current->root = NULL;
	iput( current->executable );
	// �����ǰ��������ͷ( leader )���̲������п��Ƶ��ն�,���ͷŸ��ն�
	current->executable = NULL;
	if ( current->leader && current->tty >= 0 )
		tty_table[ current->tty ].pgrp = 0;
	// �����ǰ�����ϴ�ʹ�ù�Э������,��last_task_used_math �ÿ�
	if ( last_task_used_math == current )
		last_task_used_math = NULL;
	// �����ǰ������leader ����,����ֹ������ؽ���
	if ( current->leader )
		kill_session();
	// �ѵ�ǰ������Ϊ����״̬,�������˳���
	current->state = TASK_ZOMBIE;
	current->exit_code = code;
	// ֪ͨ������,Ҳ���򸸽��̷����ź�SIGCHLD -- �ӽ��̽�ֹͣ����ֹ
	tell_father( current->father );
	schedule();
	return ( -1 );	/* just to suppress warnings */
}

LONG sys_exit( LONG error_code )
{
	return do_exit( ( error_code & 0xff ) << 8 );
}

// ϵͳ����waitpid().����ǰ����,ֱ��pid ָ�����ӽ����˳�( ��ֹ )�����յ�Ҫ����ֹ
// �ý��̵��ź�,��������Ҫ����һ���źž��( �źŴ������ ).���pid ��ָ���ӽ�������
// �˳�( �ѳ���ν�Ľ������� ),�򱾵��ý����̷���.�ӽ���ʹ�õ�������Դ���ͷ�.
// ���pid > 0, ��ʾ�ȴ����̺ŵ���pid ���ӽ���.
// ���pid = 0, ��ʾ�ȴ�������ŵ��ڵ�ǰ���̵��κ��ӽ���.
// ���pid < -1, ��ʾ�ȴ�������ŵ���pid ����ֵ���κ��ӽ���.
// [ ���pid = -1, ��ʾ�ȴ��κ��ӽ���. ]
// ��options = WUNTRACED,��ʾ����ӽ�����ֹͣ��,Ҳ���Ϸ���.
// ��options = WNOHANG,��ʾ���û���ӽ����˳�����ֹ�����Ϸ���.
// ���stat_addr ��Ϊ��,��ͽ�״̬��Ϣ���浽����
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


