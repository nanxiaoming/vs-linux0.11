/*
*  linux/kernel/sched.c
*
*  ( C ) 1991  Linus Torvalds
*/

/*
 * 'sched.c' is the main kernel file. It contains scheduling primitives
 * ( sleep_on, wakeup, schedule etc ) as well as a number of simple system
 * call functions ( type getpid(), which just extracts a field from
 * current-task
 */

/*
 * 'sched.c'����Ҫ���ں��ļ�.���а����йص��ȵĻ�������( sleep_on��wakeup��schedule �� )�Լ�
 * һЩ�򵥵�ϵͳ���ú���( ����getpid(),���ӵ�ǰ�����л�ȡһ���ֶ� ).
 */
#include <linux\sched.h>
#include <linux\kernel.h>
#include <linux\sys.h>
#include <linux\fdreg.h>
#include <asm\system.h>
#include <asm\io.h>
#include <asm\segment.h>

#include <signal.h>

#define _S( nr )	( 1<<( ( nr )-1 ) )						// ȡ�ź�nr ���ź�λͼ�ж�Ӧλ�Ķ�������ֵ.�źű��1-32,�����ź�5 ��λͼ��ֵ = 1<<( 5-1 ) = 16 = 00010000b
#define _BLOCKABLE	( ~( _S( SIGKILL ) | _S( SIGSTOP ) ) )	// ����SIGKILL ��SIGSTOP �ź�������������

typedef VOID( *timer_fn )(VOID);

// ��ʾ�����nr �Ľ��̺š�����״̬���ں˶�ջ�����ֽ���( ��Լ )
VOID show_task( LONG nr, Task_Struct * p )
{
	LONG i, j = 4096 - sizeof( Task_Struct );

	printk( "%d: pid=%d, state=%d, ", nr, p->pid, p->state );

	i = 0;

	while (i < j && !((CHAR*)(p + 1))[i])
	{
		i++;
	}

	printk( "%d ( of %d ) chars free in kernel stack\n\r", i, j );
}

// ��ʾ�������������š����̺š�����״̬���ں˶�ջ�����ֽ���
VOID show_stat()
{
	LONG i;

	for ( i = 0; i < NR_TASKS; i++ )
	{
		if ( task[ i ] )
		{
			show_task( i, task[ i ] );
		}
	}
}

// ����ÿ��ʱ��Ƭ�ĵδ���
#define LATCH ( 1193180/HZ )

extern VOID mem_use();

extern LONG timer_interrupt();	// ʱ���жϴ������
extern LONG system_call();		// ϵͳ�����жϴ������

// ������������( ����ṹ��Ա��stack �ַ���������Ա )
union task_union 
{
	Task_Struct task;				// Offset=0x0 Size=0x3bc
	CHAR stack[ PAGE_SIZE ];		// Offset=0x0 Size=0x1000
};

static union task_union init_task = { INIT_TASK, };

// �ӿ�����ʼ����ĵδ���ʱ��ֵ( 10ms/�δ� )
LONG volatile jiffies = 0;

LONG			startup_time		= 0;					// ����ʱ��.��1970:0:0:0 ��ʼ��ʱ������
Task_Struct *	current				= &( init_task.task );	// ��ǰ����ָ��( ��ʼ��Ϊ��ʼ���� )
Task_Struct *	last_task_used_math = NULL;					// ʹ�ù�Э�����������ָ��

Task_Struct * task[ NR_TASKS ] = { &( init_task.task ), };	// ��������ָ������

LONG user_stack[ PAGE_SIZE >> 2 ];							// ����ϵͳ��ջָ��,4K.ָ��ָ�����һ��

// �ýṹ�������ö�ջss:esp( ���ݶ�ѡ���,ָ�� )
struct
{
	LONG * a;
	short b;
} stack_start = { &user_stack[ PAGE_SIZE >> 2 ], 0x10 };

/*
 * 'math_state_restore()' saves the current math information in the
 * old math state array, and gets the new ones from the current task
 */

// �����񱻵��Ƚ������Ժ�,�ú������Ա���ԭ�����Э������״̬( ������ )���ָ��µ��Ƚ�����
// ��ǰ�����Э������ִ��״̬
VOID math_state_restore()
{
	I387_Struct *m;

	if ( last_task_used_math == current )	// �������û���򷵻�( ��һ��������ǵ�ǰ���� )
	{
		return;								// ������ָ��"��һ������"�Ǹձ�������ȥ������
	}
	
	__asm fwait								// �ڷ���Э����������֮ǰҪ�ȷ� WAIT ָ��

	if ( last_task_used_math ) 
	{
		// ����ϸ�����ʹ����Э������,�򱣴���״̬
		m = &last_task_used_math->tss.i387;
		__asm mov		eax, m
		__asm fnsave	TBYTE PTR[ eax ]
	}

	// ����,last_task_used_math ָ��ǰ����,
	// �Ա���ǰ���񱻽�����ȥʱʹ��
	last_task_used_math = current;

	if ( current->used_math ) 
	{
		// �����ǰ�����ù�Э������,��ָ���״̬
		__asm mov		eax, m
		__asm frstor	TBYTE PTR[ eax ]
	}
	else 
	{
		//��һ��ʹ��
		__asm fninit			// ���Ǿ���Э����������ʼ������
		current->used_math = 1;	// ������ʹ����Э��������־
	}
}

VOID schedule()
/*++

Routine Description:

	ԭʼע��:
	NOTE!!  Task 0 is the 'idle' task, which gets called when no other
	tasks can run. It can not be killed, and it cannot sleep. The 'state'
	information in task[ 0 ] is never used.

	�ú�����Ŀ�������°���������תִ��.

	ע�� ! ���� 0 �Ǹ�����( 'idle' )����,ֻ�е�û�����������������ʱ�ŵ�����.�����ܱ�ɱ��,
	Ҳ����˯��.���� 0 �е�״̬��Ϣ'state'�Ǵ������õ�.

Arguments:
	
	VOID

Return Value:

	VOID

--*/
{
	LONG			i, next, c;
	Task_Struct **	p;

	/* check alarm, wake up any interruptible tasks that have got a signal */

	// ��� alarm ( ���̵ı�����ʱֵ ),�����κ��ѵõ��źŵĿ��ж�����
	// ���������������һ������ʼ���alarm
	for ( p = &LAST_TASK; p > &FIRST_TASK; --p )
	{
		if ( *p ) 
		{
			// �������� alarm ʱ���Ѿ�����( alarm<jiffies ),�����ź�λͼ���� SIGALRM �ź�,Ȼ����alarm.
			// jiffies ��ϵͳ�ӿ�����ʼ����ĵδ���( 10ms/�δ� ).

			if ( ( *p )->alarm && ( *p )->alarm < jiffies ) 
			{
				( *p )->signal |= ( 1 << ( SIGALRM - 1 ) );
				( *p )->alarm = 0;
			}

			// ����ź�λͼ�г����������ź��⻹�������ź�,���������ڿ��ж�״̬,��������Ϊ����״̬.
			// ����'~( _BLOCKABLE & ( *p )->blocked )'���ں��Ա��������ź�,�� SIGKILL �� SIGSTOP ���ܱ�����.

			if ( ( ( *p )->signal & ~( _BLOCKABLE & ( *p )->blocked ) ) &&
				 ( ( *p )->state == TASK_INTERRUPTIBLE ) 
			   )
			{
				( *p )->state = TASK_RUNNING;	//��Ϊ����( ��ִ�� )״̬
			}	
		}
	}

	while ( 1 )
	{
		c	= -1;
		next= 0;
		i	= NR_TASKS;
		p	= &task[ NR_TASKS ];

		// ��δ���Ҳ�Ǵ�������������һ������ʼѭ������,��������������������.�Ƚ�ÿ������
		// ״̬�����counter( ��������ʱ��ĵݼ��δ���� )ֵ,��һ��ֵ��,����ʱ�仹����,next ��
		// ָ���ĸ��������
		while ( --i )
		{
			if ( !*--p )
			{
				continue;
			}
			if ( ( *p )->state == TASK_RUNNING && ( *p )->counter > c )
			{
				c = ( *p )->counter, next = i;
			}
		}
		// ����Ƚϵó��� counter ֵ���� 0 �Ľ��,���˳�ѭ��,ִ�������л�
		if ( c ) 
		{
			break;
		}

		// ����͸���ÿ�����������Ȩֵ,����ÿһ������� counter ֵ.
		// counter ֵ�ļ��㷽ʽΪcounter = counter /2 + priority.

		for ( p = &LAST_TASK; p > &FIRST_TASK; --p )
		{
			if ( *p )
			{
				( *p )->counter = ( ( *p )->counter >> 1 ) + ( *p )->priority;
			}
		}
	}
	switch_to( next );	// �л��������Ϊ next ������,������
}

LONG sys_pause()
/*++

Routine Description:

	pause()ϵͳ����.ת����ǰ�����״̬Ϊ���жϵĵȴ�״̬,�����µ���.
	��ϵͳ���ý����½��̽���˯��״̬,ֱ���յ�һ���ź�.���ź�������ֹ���̻���ʹ���̵���
	һ���źŲ�����.ֻ�е�������һ���ź�,�����źŲ�����������,pause()�Ż᷵��.
	
Arguments:
	
	VOID

Return Value:

	0

--*/
{
	current->state = TASK_INTERRUPTIBLE;

	schedule();

	return 0;
}

VOID sleep_on( Task_Struct **p )
/*++

Routine Description:

	�ѵ�ǰ������Ϊ�����жϵĵȴ�״̬,����˯�߶���ͷ��ָ��ָ��ǰ����.
	ֻ����ȷ�ػ���ʱ�Ż᷵��.�ú����ṩ�˽������жϴ������֮���ͬ������.
	�������� *p �Ƿ��õȴ�����Ķ���ͷָ��.( �μ��б���˵�� )

Arguments:

	p - �ȴ�������,Ϊ����Ĳ���

Return Value:

	VOID

--*/
{
	Task_Struct *tmp;

	if ( !p )
	{
		return;
	}

	if ( current == &( init_task.task ) )
	{
		panic( "task[ 0 ] trying to sleep" );
	}

	tmp				= *p;					// �� tmp ָ���Ѿ��ڵȴ������ϵ�����( ����еĻ� )
	*p				= current;				// ��˯�߶���ͷ�ĵȴ�ָ��ָ��ǰ����
	current->state	= TASK_UNINTERRUPTIBLE;

	schedule();

	// ֻ�е�����ȴ����񱻻���ʱ,���ȳ�����ַ��ص�����,���ʾ�����ѱ���ȷ�ػ���.
	// ��Ȼ��Ҷ��ڵȴ�ͬ������Դ,��ô����Դ����ʱ,���б�Ҫ�������еȴ�����Դ�Ľ���.�ú���
	// Ƕ�׵���,Ҳ��Ƕ�׻������еȴ�����Դ�Ľ���.Ȼ��ϵͳ�������Щ���̵���������,���µ���
	// Ӧ�����ĸ���������ʹ����Դ.Ҳ������Щ���̾����ϸ�
	if ( tmp )
	{
		tmp->state = TASK_RUNNING;
	}
}

// ����ǰ������Ϊ���жϵĵȴ�״̬,������*p ָ���ĵȴ�������.�μ��б���sleep_on()��˵��.

VOID interruptible_sleep_on( Task_Struct **p )
{
	Task_Struct *tmp;

	if ( !p )
	{
		return;
	}

	if ( current == &( init_task.task ) )
	{
		panic( "task[ 0 ] trying to sleep" );
	}

	tmp = *p;

	*p  = current;

repeat:

	current->state = TASK_INTERRUPTIBLE;

	schedule();

	// ����ȴ������л��еȴ�����,���Ҷ���ͷָ����ָ��������ǵ�ǰ����ʱ,�򽫸õȴ�������Ϊ
	// �����еľ���״̬,������ִ�е��ȳ���.��ָ�� *p ��ָ��Ĳ��ǵ�ǰ����ʱ,��ʾ�ڵ�ǰ���񱻷�
	// ����к�,�����µ����񱻲���ȴ�������,���,��Ȼ�������ǿ��жϵ�,��Ӧ������ִ������
	// �����ĵȴ�����
	if ( *p && *p != current ) 
	{
		( **p ).state = TASK_RUNNING;
		goto repeat;
	}

	// ����һ���������,Ӧ����*p = tmp,�ö���ͷָ��ָ������ȴ�����,�����ڵ�ǰ����֮ǰ����
	// �ȴ����е��������Ĩ����
	*p = NULL;

	if ( tmp )
	{
		tmp->state = TASK_RUNNING;
	}
}

// ����ָ������
VOID wake_up( Task_Struct **p )
{
	if ( p && *p ) 
	{
		( **p ).state = TASK_RUNNING;
		*p = NULL;
	}
}

/*
 * OK, here are some floppy things that shouldn't be in the kernel
 * proper. They are here because the floppy needs a timer, and this
 * was the easiest way of doing it.
 */
/*
 * �����￪ʼ��һЩ�й����̵��ӳ���,����Ӧ�÷����ں˵���Ҫ�����е�.�����Ƿ�������
 * ����Ϊ������Ҫһ��ʱ��,���������������İ취.
 */
static Task_Struct *	wait_motor[ 4 ] = {0};
static LONG				mon_timer [ 4 ] = {0};
static LONG				moff_timer[ 4 ] = {0};
	   UCHAR			current_DOR		= 0x0C;// ��������Ĵ���( ��ֵ:����DMA �������жϡ�����FDC )

// ָ�����̵�������ת״̬�����ӳٵδ���( ʱ�� ).
// nr -- ������( 0-3 ),����ֵΪ�δ���.
LONG ticks_to_floppy_on( ULONG nr )
{
	extern	UCHAR	selected;			// ��ǰѡ�е����̺�( kernel/blk_drv/floppy.c,122 ).
			UCHAR	mask = 0x10 << nr;	// ��ѡ������Ӧ��������Ĵ���������������λ.

	if ( nr > 3 )
	{
		panic( "floppy_on: nr>3" );		// ���4������
	}

	moff_timer[ nr ] = 10000;		/* 100 s = very big :- ) */

	cli();							/* use floppy_off to turn it off */

	mask |= current_DOR;

	// ������ǵ�ǰ����,�����ȸ�λ����������ѡ��λ,Ȼ���ö�Ӧ����ѡ��λ.
	if ( !selected ) 
	{
		mask &= 0xFC;
		mask |= nr;
	}

	// �����������Ĵ����ĵ�ǰֵ��Ҫ���ֵ��ͬ,����FDC ��������˿������ֵ( mask ).�������
	// Ҫ����������ﻹû������,������Ӧ���������������ʱ��ֵ( HZ/2 = 0.5 ���50 ���δ� ).
	// �˺���µ�ǰ��������Ĵ���ֵcurrent_DOR
	if ( mask != current_DOR ) 
	{
		outb( mask, FD_DOR );

		if ( ( mask ^ current_DOR ) & 0xf0 )
		{
			mon_timer[ nr ] = HZ / 2;
		}
		else if ( mon_timer[ nr ] < 2 )
		{
			mon_timer[ nr ] = 2;
		}
		current_DOR = mask;
	}

	sti();

	return mon_timer[ nr ];
}

// �ȴ�ָ�����������������ʱ��
VOID floppy_on( ULONG nr )
{
	cli();

	while ( ticks_to_floppy_on( nr ) )	// ������������ʱ��û��,��һֱ�ѵ�ǰ������
	{
		sleep_on( nr + wait_motor );	// Ϊ�����ж�˯��״̬������ȴ�������еĶ�����.
	}
	
	sti();
}

VOID floppy_off( ULONG nr )
{
	moff_timer[ nr ] = 3 * HZ;
}

// ���̶�ʱ�����ӳ���.�������������ʱֵ�����ر�ͣת��ʱֵ.���ӳ�������ʱ�Ӷ�ʱ
// �ж��б�����,���ÿһ���δ�( 10ms )������һ��,������￪����ͣת��ʱ����ֵ.���ĳ
// һ�����ͣת��ʱ��,����������Ĵ����������λ��λ
VOID do_floppy_timer()
{
	LONG	i;
	UCHAR	mask = 0x10;

	for ( i = 0; i < 4; i++, mask <<= 1 ) 
	{
		if ( !( mask & current_DOR ) )	// �������DOR ָ�������������
		{
			continue;
		}
		if ( mon_timer[ i ] ) 
		{
			if ( !--mon_timer[ i ] )	// ������������ʱ�����ѽ���
			{
				wake_up( i + wait_motor );
			}
		}
		else if ( !moff_timer[ i ] ) 
		{
			// ������ͣת��ʱ����
			current_DOR &= ~mask;		// ��λ��Ӧ�������λ
			outb( current_DOR, FD_DOR );// ������������Ĵ���
		}
		else
		{
			moff_timer[ i ]--;
		}
	}
}

#define TIME_REQUESTS 64 // ������64 ����ʱ������

// ��ʱ������ṹ�Ͷ�ʱ������
static struct timer_list
{
	LONG					jiffies;			// Offset=0x0 Size=0x4
	timer_fn				fn;					// Offset=0x4 Size=0x4
	struct timer_list *		next;				// Offset=0x8 Size=0x4
} timer_list[ TIME_REQUESTS ], *next_timer = NULL;


// ��Ӷ�ʱ��.�������Ϊָ���Ķ�ʱֵ( �δ��� )����Ӧ�Ĵ������ָ��.
// jiffies �C ��10 ����Ƶĵδ���;*fn()- ��ʱʱ�䵽ʱִ�еĺ���

VOID add_timer( LONG jiffies, timer_fn cb )
{
	struct timer_list * p;

	// �����ʱ�������ָ��Ϊ��,���˳�
	if ( !cb )
	{
		return;
	}

	cli();

	// �����ʱֵ<=0,�����̵����䴦�����.���Ҹö�ʱ��������������
	if ( jiffies <= 0 )
	{
		cb();
	}
	else 
	{
		// �Ӷ�ʱ��������,��һ��������
		for ( p = timer_list; p < timer_list + TIME_REQUESTS; p++ )
		{
			if ( !p->fn )
			{
				break;
			}
		}
		if ( p >= timer_list + TIME_REQUESTS )
		{
			panic( "No more time requests free" );
		}
		// ��ʱ�����ݽṹ������Ӧ��Ϣ.����������ͷ
		p->fn		= cb;
		p->jiffies	= jiffies;
		p->next		= next_timer;
		next_timer	= p;

		// �������ʱֵ��С��������.������ʱ��ȥ����ǰ����Ҫ�ĵδ���,�����ڴ���ʱ��ʱֻҪ
		// �鿴����ͷ�ĵ�һ��Ķ�ʱ�Ƿ��ڼ���.[ [ ?? ��γ������û�п�����ȫ.����²���Ķ�ʱ
		// ��ֵ < ԭ��ͷһ����ʱ��ֵʱ,ҲӦ�ý����к���Ķ�ʱֵ����ȥ�µĵ�1 ���Ķ�ʱֵ. ] ]

		while ( p->next && p->next->jiffies < p->jiffies )
		{
			p->jiffies 		   -= p->next->jiffies;
			cb					= p->fn;
			p->fn				= p->next->fn;
			p->next->fn			= cb;
			jiffies				= p->jiffies;
			p->jiffies			= p->next->jiffies;
			p->next->jiffies	= jiffies;
			p					= p->next;
		}
	}
	sti();
}

VOID do_timer( LONG cpl )
/*++

Routine Description:

	ʱ���жϺ����������

	����һ����������ִ��ʱ��Ƭ����ʱ,
	����������л�.��ִ��һ����ʱ���¹���

Arguments:

	cpl - 0 �ں�
		  3 �û�

Return Value:

	VOID

--*/
{
	extern LONG beepcount;
	extern VOID sysbeepstop();

	// �����������������,��رշ���.( ��0x61 �ڷ�������,��λλ0 ��1.λ0 ����8253������2 �Ĺ���,λ1 ���������� )
	if ( beepcount && !--beepcount )
	{
		sysbeepstop();
	}

	if ( cpl )
	{
		current->utime++;
	}
	else
	{
		current->stime++;
	}

	// ������û��Ķ�ʱ������,�������1����ʱ����ֵ��1.����ѵ���0,�������Ӧ�Ĵ������,
	// �����ô������ָ����Ϊ��.Ȼ��ȥ�����ʱ��
	if ( next_timer ) 
	{
		next_timer->jiffies--;

		while ( next_timer && next_timer->jiffies <= 0 ) 
		{
			timer_fn 
			fn				= next_timer->fn;
			next_timer->fn	= NULL;
			next_timer		= next_timer->next;

			fn();
		}
	}
	// �����ǰ���̿����� FDC ����������Ĵ������������λ����λ��,��ִ�����̶�ʱ����.
	if ( current_DOR & 0xf0 )
	{
		do_floppy_timer();
	}

	if ( --current->counter > 0 )
	{
		return;
	}

	current->counter = 0;

	if ( !cpl )
	{
		return;
	}

	//�����л�����
	schedule();
}

// ϵͳ���ù��� - ���ñ�����ʱʱ��ֵ( �� ).
// ����Ѿ����ù�alarm ֵ,�򷵻ؾ�ֵ,���򷵻�0
LONG sys_alarm( LONG seconds )
{
	LONG old = current->alarm;

	if ( old )
	{
		old = ( old - jiffies ) / HZ;
	}

	current->alarm = ( seconds > 0 ) ? ( jiffies + HZ*seconds ) : 0;

	return ( old );
}

// ȡ��ǰ���̺�pid
LONG sys_getpid()
{
	return current->pid;
}
// ȡ�����̺�ppid.
LONG sys_getppid()
{
	return current->father;
}

LONG sys_getuid()
{
	return current->uid;
}

LONG sys_geteuid()
{
	return current->euid;
}

LONG sys_getgid()
{
	return current->gid;
}

LONG sys_getegid()
{
	return current->egid;
}

// ϵͳ���ù��� -- ���Ͷ�CPU ��ʹ������Ȩ( ���˻�����?? ).
// Ӧ������increment ����0,����Ļ�,��ʹ����Ȩ����
LONG sys_nice( LONG increment )
{
	if ( current->priority - increment > 0 )
	{
		current->priority -= increment;
	}
	return 0;
}

// ���ȳ���ĳ�ʼ���ӳ���

VOID sched_init()
{
	LONG			i;
	Desc_Struct *	p;

	// ���ó�ʼ����( ����0 )������״̬���������;ֲ����ݱ�������.
	set_tss_desc( gdt + FIRST_TSS_ENTRY, &( init_task.task.tss ) );
	set_ldt_desc( gdt + FIRST_LDT_ENTRY, &( init_task.task.ldt ) );

	// ���������������������( ע��i=1 ��ʼ,���Գ�ʼ��������������� )
	p = gdt + 2 + FIRST_TSS_ENTRY;

	for ( i = 1; i < NR_TASKS; i++ )
	{
		task[ i ] = NULL;
		p->a = p->b = 0;
		p++;
		p->a = p->b = 0;
		p++;
	}

	/* Clear NT, so that we won't have troubles with that later on */
	/* �����־�Ĵ����е�λNT,�����Ժ�Ͳ������鷳 */
	// NT ��־���ڿ��Ƴ���ĵݹ����( Nested Task ).�� NT ��λʱ,��ô��ǰ�ж�����ִ��
	// iret ָ��ʱ�ͻ����������л� .NT ָ�� TSS �е� back_link �ֶ��Ƿ���Ч.

	__asm pushfd
	__asm and DWORD PTR[esp], 0xFFFFBFFF
	__asm popfd

	ltr ( 0 );						// ������ 0 �� TSS ���ص�����Ĵ���tr.
	lldt( 0 );						// ���ֲ�����������ص��ֲ���������Ĵ���

	// ע�⣡���ǽ� GDT ����Ӧ LDT ��������ѡ������ص� ldtr .ֻ��ȷ������һ��,�Ժ�������
	// LDT �ļ���,�� CPU ���� TSS �е� LDT ���Զ�����.
	// ����������ڳ�ʼ�� 8253 ��ʱ��

	outb_p( 0x36		, 0x43	);		/* binary, mode 3, LSB/MSB, ch 0 */
	outb_p( LATCH & 0xff, 0x40	);		/* LSB */// ��ʱֵ���ֽ�
	outb  ( LATCH >> 8	, 0x40	);		/* MSB */// ��ʱֵ���ֽ�

	// ����ʱ���жϴ��������( ����ʱ���ж��� )
	set_intr_gate( 0x20, &timer_interrupt );

	// �޸��жϿ�����������,����ʱ���ж�
	outb( inb_p( 0x21 )&~0x01, 0x21 );

	// ����ϵͳ�����ж���
	set_system_gate( 0x80, &system_call );
}
