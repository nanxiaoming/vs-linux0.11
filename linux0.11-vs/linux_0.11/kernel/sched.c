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
 * 'sched.c'是主要的内核文件.其中包括有关调度的基本函数( sleep_on、wakeup、schedule 等 )以及
 * 一些简单的系统调用函数( 比如getpid(),仅从当前任务中获取一个字段 ).
 */
#include <linux\sched.h>
#include <linux\kernel.h>
#include <linux\sys.h>
#include <linux\fdreg.h>
#include <asm\system.h>
#include <asm\io.h>
#include <asm\segment.h>

#include <signal.h>

#define _S( nr )	( 1<<( ( nr )-1 ) )						// 取信号nr 在信号位图中对应位的二进制数值.信号编号1-32,比如信号5 的位图数值 = 1<<( 5-1 ) = 16 = 00010000b
#define _BLOCKABLE	( ~( _S( SIGKILL ) | _S( SIGSTOP ) ) )	// 除了SIGKILL 和SIGSTOP 信号以外其它都是

typedef VOID( *timer_fn )(VOID);

// 显示任务号nr 的进程号、进程状态和内核堆栈空闲字节数( 大约 )
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

// 显示所有任务的任务号、进程号、进程状态和内核堆栈空闲字节数
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

// 定义每个时间片的滴答数
#define LATCH ( 1193180/HZ )

extern VOID mem_use();

extern LONG timer_interrupt();	// 时钟中断处理程序
extern LONG system_call();		// 系统调用中断处理程序

// 定义任务联合( 任务结构成员和stack 字符数组程序成员 )
union task_union 
{
	Task_Struct task;				// Offset=0x0 Size=0x3bc
	CHAR stack[ PAGE_SIZE ];		// Offset=0x0 Size=0x1000
};

static union task_union init_task = { INIT_TASK, };

// 从开机开始算起的滴答数时间值( 10ms/滴答 )
LONG volatile jiffies = 0;

LONG			startup_time		= 0;					// 开机时间.从1970:0:0:0 开始计时的秒数
Task_Struct *	current				= &( init_task.task );	// 当前任务指针( 初始化为初始任务 )
Task_Struct *	last_task_used_math = NULL;					// 使用过协处理器任务的指针

Task_Struct * task[ NR_TASKS ] = { &( init_task.task ), };	// 定义任务指针数组

LONG user_stack[ PAGE_SIZE >> 2 ];							// 定义系统堆栈指针,4K.指针指在最后一项

// 该结构用于设置堆栈ss:esp( 数据段选择符,指针 )
struct
{
	LONG * a;
	short b;
} stack_start = { &user_stack[ PAGE_SIZE >> 2 ], 0x10 };

/*
 * 'math_state_restore()' saves the current math information in the
 * old math state array, and gets the new ones from the current task
 */

// 当任务被调度交换过以后,该函数用以保存原任务的协处理器状态( 上下文 )并恢复新调度进来的
// 当前任务的协处理器执行状态
VOID math_state_restore()
{
	I387_Struct *m;

	if ( last_task_used_math == current )	// 如果任务没变则返回( 上一个任务就是当前任务 )
	{
		return;								// 这里所指的"上一个任务"是刚被交换出去的任务
	}
	
	__asm fwait								// 在发送协处理器命令之前要先发 WAIT 指令

	if ( last_task_used_math ) 
	{
		// 如果上个任务使用了协处理器,则保存其状态
		m = &last_task_used_math->tss.i387;
		__asm mov		eax, m
		__asm fnsave	TBYTE PTR[ eax ]
	}

	// 现在,last_task_used_math 指向当前任务,
	// 以备当前任务被交换出去时使用
	last_task_used_math = current;

	if ( current->used_math ) 
	{
		// 如果当前任务用过协处理器,则恢复其状态
		__asm mov		eax, m
		__asm frstor	TBYTE PTR[ eax ]
	}
	else 
	{
		//第一次使用
		__asm fninit			// 于是就向协处理器发初始化命令
		current->used_math = 1;	// 并设置使用了协处理器标志
	}
}

VOID schedule()
/*++

Routine Description:

	原始注释:
	NOTE!!  Task 0 is the 'idle' task, which gets called when no other
	tasks can run. It can not be killed, and it cannot sleep. The 'state'
	information in task[ 0 ] is never used.

	该函数的目的是重新安排任务并跳转执行.

	注意 ! 任务 0 是个闲置( 'idle' )任务,只有当没有其它任务可以运行时才调用它.它不能被杀死,
	也不能睡眠.任务 0 中的状态信息'state'是从来不用的.

Arguments:
	
	VOID

Return Value:

	VOID

--*/
{
	LONG			i, next, c;
	Task_Struct **	p;

	/* check alarm, wake up any interruptible tasks that have got a signal */

	// 检测 alarm ( 进程的报警定时值 ),唤醒任何已得到信号的可中断任务
	// 从任务数组中最后一个任务开始检测alarm
	for ( p = &LAST_TASK; p > &FIRST_TASK; --p )
	{
		if ( *p ) 
		{
			// 如果任务的 alarm 时间已经过期( alarm<jiffies ),则在信号位图中置 SIGALRM 信号,然后清alarm.
			// jiffies 是系统从开机开始算起的滴答数( 10ms/滴答 ).

			if ( ( *p )->alarm && ( *p )->alarm < jiffies ) 
			{
				( *p )->signal |= ( 1 << ( SIGALRM - 1 ) );
				( *p )->alarm = 0;
			}

			// 如果信号位图中除被阻塞的信号外还有其它信号,并且任务处于可中断状态,则置任务为就绪状态.
			// 其中'~( _BLOCKABLE & ( *p )->blocked )'用于忽略被阻塞的信号,但 SIGKILL 和 SIGSTOP 不能被阻塞.

			if ( ( ( *p )->signal & ~( _BLOCKABLE & ( *p )->blocked ) ) &&
				 ( ( *p )->state == TASK_INTERRUPTIBLE ) 
			   )
			{
				( *p )->state = TASK_RUNNING;	//置为就绪( 可执行 )状态
			}	
		}
	}

	while ( 1 )
	{
		c	= -1;
		next= 0;
		i	= NR_TASKS;
		p	= &task[ NR_TASKS ];

		// 这段代码也是从任务数组的最后一个任务开始循环处理,并跳过不含任务的数组槽.比较每个就绪
		// 状态任务的counter( 任务运行时间的递减滴答计数 )值,哪一个值大,运行时间还不长,next 就
		// 指向哪个的任务号
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
		// 如果比较得出有 counter 值大于 0 的结果,则退出循环,执行任务切换
		if ( c ) 
		{
			break;
		}

		// 否则就根据每个任务的优先权值,更新每一个任务的 counter 值.
		// counter 值的计算方式为counter = counter /2 + priority.

		for ( p = &LAST_TASK; p > &FIRST_TASK; --p )
		{
			if ( *p )
			{
				( *p )->counter = ( ( *p )->counter >> 1 ) + ( *p )->priority;
			}
		}
	}
	switch_to( next );	// 切换到任务号为 next 的任务,并运行
}

LONG sys_pause()
/*++

Routine Description:

	pause()系统调用.转换当前任务的状态为可中断的等待状态,并重新调度.
	该系统调用将导致进程进入睡眠状态,直到收到一个信号.该信号用于终止进程或者使进程调用
	一个信号捕获函数.只有当捕获了一个信号,并且信号捕获处理函数返回,pause()才会返回.
	
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

	把当前任务置为不可中断的等待状态,并让睡眠队列头的指针指向当前任务.
	只有明确地唤醒时才会返回.该函数提供了进程与中断处理程序之间的同步机制.
	函数参数 *p 是放置等待任务的队列头指针.( 参见列表后的说明 )

Arguments:

	p - 等待的任务,为输出的参数

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

	tmp				= *p;					// 让 tmp 指向已经在等待队列上的任务( 如果有的话 )
	*p				= current;				// 将睡眠队列头的等待指针指向当前任务
	current->state	= TASK_UNINTERRUPTIBLE;

	schedule();

	// 只有当这个等待任务被唤醒时,调度程序才又返回到这里,则表示进程已被明确地唤醒.
	// 既然大家都在等待同样的资源,那么在资源可用时,就有必要唤醒所有等待该资源的进程.该函数
	// 嵌套调用,也会嵌套唤醒所有等待该资源的进程.然后系统会根据这些进程的优先条件,重新调度
	// 应该由哪个进程首先使用资源.也即让这些进程竞争上岗
	if ( tmp )
	{
		tmp->state = TASK_RUNNING;
	}
}

// 将当前任务置为可中断的等待状态,并放入*p 指定的等待队列中.参见列表后对sleep_on()的说明.

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

	// 如果等待队列中还有等待任务,并且队列头指针所指向的任务不是当前任务时,则将该等待任务置为
	// 可运行的就绪状态,并重新执行调度程序.当指针 *p 所指向的不是当前任务时,表示在当前任务被放
	// 入队列后,又有新的任务被插入等待队列中,因此,既然本任务是可中断的,就应该首先执行所有
	// 其它的等待任务
	if ( *p && *p != current ) 
	{
		( **p ).state = TASK_RUNNING;
		goto repeat;
	}

	// 下面一句代码有误,应该是*p = tmp,让队列头指针指向其余等待任务,否则在当前任务之前插入
	// 等待队列的任务均被抹掉了
	*p = NULL;

	if ( tmp )
	{
		tmp->state = TASK_RUNNING;
	}
}

// 唤醒指定任务
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
 * 从这里开始是一些有关软盘的子程序,本不应该放在内核的主要部分中的.将它们放在这里
 * 是因为软驱需要一个时钟,而放在这里是最方便的办法.
 */
static Task_Struct *	wait_motor[ 4 ] = {0};
static LONG				mon_timer [ 4 ] = {0};
static LONG				moff_timer[ 4 ] = {0};
	   UCHAR			current_DOR		= 0x0C;// 数字输出寄存器( 初值:允许DMA 和请求中断、启动FDC )

// 指定软盘到正常运转状态所需延迟滴答数( 时间 ).
// nr -- 软驱号( 0-3 ),返回值为滴答数.
LONG ticks_to_floppy_on( ULONG nr )
{
	extern	UCHAR	selected;			// 当前选中的软盘号( kernel/blk_drv/floppy.c,122 ).
			UCHAR	mask = 0x10 << nr;	// 所选软驱对应数字输出寄存器中启动马达比特位.

	if ( nr > 3 )
	{
		panic( "floppy_on: nr>3" );		// 最多4个软驱
	}

	moff_timer[ nr ] = 10000;		/* 100 s = very big :- ) */

	cli();							/* use floppy_off to turn it off */

	mask |= current_DOR;

	// 如果不是当前软驱,则首先复位其它软驱的选择位,然后置对应软驱选择位.
	if ( !selected ) 
	{
		mask &= 0xFC;
		mask |= nr;
	}

	// 如果数字输出寄存器的当前值与要求的值不同,则向FDC 数字输出端口输出新值( mask ).并且如果
	// 要求启动的马达还没有启动,则置相应软驱的马达启动定时器值( HZ/2 = 0.5 秒或50 个滴答 ).
	// 此后更新当前数字输出寄存器值current_DOR
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

// 等待指定软驱马达启动所需时间
VOID floppy_on( ULONG nr )
{
	cli();

	while ( ticks_to_floppy_on( nr ) )	// 如果马达启动定时还没到,就一直把当前进程置
	{
		sleep_on( nr + wait_motor );	// 为不可中断睡眠状态并放入等待马达运行的队列中.
	}
	
	sti();
}

VOID floppy_off( ULONG nr )
{
	moff_timer[ nr ] = 3 * HZ;
}

// 软盘定时处理子程序.更新马达启动定时值和马达关闭停转计时值.该子程序是在时钟定时
// 中断中被调用,因此每一个滴答( 10ms )被调用一次,更新马达开启或停转定时器的值.如果某
// 一个马达停转定时到,则将数字输出寄存器马达启动位复位
VOID do_floppy_timer()
{
	LONG	i;
	UCHAR	mask = 0x10;

	for ( i = 0; i < 4; i++, mask <<= 1 ) 
	{
		if ( !( mask & current_DOR ) )	// 如果不是DOR 指定的马达则跳过
		{
			continue;
		}
		if ( mon_timer[ i ] ) 
		{
			if ( !--mon_timer[ i ] )	// 如果马达启动定时到则唤醒进程
			{
				wake_up( i + wait_motor );
			}
		}
		else if ( !moff_timer[ i ] ) 
		{
			// 如果马达停转定时到则
			current_DOR &= ~mask;		// 复位相应马达启动位
			outb( current_DOR, FD_DOR );// 更新数字输出寄存器
		}
		else
		{
			moff_timer[ i ]--;
		}
	}
}

#define TIME_REQUESTS 64 // 最多可有64 个定时器链表

// 定时器链表结构和定时器数组
static struct timer_list
{
	LONG					jiffies;			// Offset=0x0 Size=0x4
	timer_fn				fn;					// Offset=0x4 Size=0x4
	struct timer_list *		next;				// Offset=0x8 Size=0x4
} timer_list[ TIME_REQUESTS ], *next_timer = NULL;


// 添加定时器.输入参数为指定的定时值( 滴答数 )和相应的处理程序指针.
// jiffies – 以10 毫秒计的滴答数;*fn()- 定时时间到时执行的函数

VOID add_timer( LONG jiffies, timer_fn cb )
{
	struct timer_list * p;

	// 如果定时处理程序指针为空,则退出
	if ( !cb )
	{
		return;
	}

	cli();

	// 如果定时值<=0,则立刻调用其处理程序.并且该定时器不加入链表中
	if ( jiffies <= 0 )
	{
		cb();
	}
	else 
	{
		// 从定时器数组中,找一个空闲项
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
		// 向定时器数据结构填入相应信息.并链入链表头
		p->fn		= cb;
		p->jiffies	= jiffies;
		p->next		= next_timer;
		next_timer	= p;

		// 链表项按定时值从小到大排序.在排序时减去排在前面需要的滴答数,这样在处理定时器时只要
		// 查看链表头的第一项的定时是否到期即可.[ [ ?? 这段程序好象没有考虑周全.如果新插入的定时
		// 器值 < 原来头一个定时器值时,也应该将所有后面的定时值均减去新的第1 个的定时值. ] ]

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

	时钟中断函数处理程序

	对于一个进程由于执行时间片用完时,
	则进行任务切换.并执行一个计时更新工作

Arguments:

	cpl - 0 内核
		  3 用户

Return Value:

	VOID

--*/
{
	extern LONG beepcount;
	extern VOID sysbeepstop();

	// 如果发声计数次数到,则关闭发声.( 向0x61 口发送命令,复位位0 和1.位0 控制8253计数器2 的工作,位1 控制扬声器 )
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

	// 如果有用户的定时器存在,则将链表第1个定时器的值减1.如果已等于0,则调用相应的处理程序,
	// 并将该处理程序指针置为空.然后去掉该项定时器
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
	// 如果当前软盘控制器 FDC 的数字输出寄存器中马达启动位有置位的,则执行软盘定时程序.
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

	//尝试切换进程
	schedule();
}

// 系统调用功能 - 设置报警定时时间值( 秒 ).
// 如果已经设置过alarm 值,则返回旧值,否则返回0
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

// 取当前进程号pid
LONG sys_getpid()
{
	return current->pid;
}
// 取父进程号ppid.
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

// 系统调用功能 -- 降低对CPU 的使用优先权( 有人会用吗?? ).
// 应该限制increment 大于0,否则的话,可使优先权增大
LONG sys_nice( LONG increment )
{
	if ( current->priority - increment > 0 )
	{
		current->priority -= increment;
	}
	return 0;
}

// 调度程序的初始化子程序

VOID sched_init()
{
	LONG			i;
	Desc_Struct *	p;

	// 设置初始任务( 任务0 )的任务状态段描述符和局部数据表描述符.
	set_tss_desc( gdt + FIRST_TSS_ENTRY, &( init_task.task.tss ) );
	set_ldt_desc( gdt + FIRST_LDT_ENTRY, &( init_task.task.ldt ) );

	// 清任务数组和描述符表项( 注意i=1 开始,所以初始任务的描述符还在 )
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
	/* 清除标志寄存器中的位NT,这样以后就不会有麻烦 */
	// NT 标志用于控制程序的递归调用( Nested Task ).当 NT 置位时,那么当前中断任务执行
	// iret 指令时就会引起任务切换 .NT 指出 TSS 中的 back_link 字段是否有效.

	__asm pushfd
	__asm and DWORD PTR[esp], 0xFFFFBFFF
	__asm popfd

	ltr ( 0 );						// 将任务 0 的 TSS 加载到任务寄存器tr.
	lldt( 0 );						// 将局部描述符表加载到局部描述符表寄存器

	// 注意！！是将 GDT 中相应 LDT 描述符的选择符加载到 ldtr .只明确加载这一次,以后新任务
	// LDT 的加载,是 CPU 根据 TSS 中的 LDT 项自动加载.
	// 下面代码用于初始化 8253 定时器

	outb_p( 0x36		, 0x43	);		/* binary, mode 3, LSB/MSB, ch 0 */
	outb_p( LATCH & 0xff, 0x40	);		/* LSB */// 定时值低字节
	outb  ( LATCH >> 8	, 0x40	);		/* MSB */// 定时值高字节

	// 设置时钟中断处理程序句柄( 设置时钟中断门 )
	set_intr_gate( 0x20, &timer_interrupt );

	// 修改中断控制器屏蔽码,允许时钟中断
	outb( inb_p( 0x21 )&~0x01, 0x21 );

	// 设置系统调用中断门
	set_system_gate( 0x80, &system_call );
}
