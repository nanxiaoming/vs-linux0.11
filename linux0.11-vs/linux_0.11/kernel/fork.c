/*
*  linux/kernel/fork.c
*
*  ( C ) 1991  Linus Torvalds
*/

/*
 *  'fork.c' contains the help-routines for the 'fork' system call
 * ( see also system_call.s ), and some misc functions ( 'verify_area' ).
 * Fork is rather simple, once you get the hang of it, but the memory
 * management can be a bitch. See 'mm/mm.c': 'copy_page_tables()'
 */
/*
 * 'fork.c'中含有系统调用'fork'的辅助子程序( 参见system_call.s ),以及一些其它函数
 * ( 'verify_area' ).一旦你了解了fork,就会发现它是非常简单的,但内存管理却有些难度.
 * 参见'mm/mm.c'中的'copy_page_tables()'.
 */
#include <errno.h>

#include <linux\sched.h>
#include <linux\kernel.h>
#include <asm\segment.h>
#include <asm\system.h>

extern VOID write_verify( ULONG address );

LONG last_pid = 0;

// 进程空间区域写前验证函数.
// 对当前进程的地址addr 到addr+size 这一段进程空间以页为单位执行写操作前的检测操作.
// 若页面是只读的,则执行共享检验和复制页面操作( 写时复制 )
VOID verify_area( VOID * addr, LONG size )
{
	ULONG start;

	start = ( ULONG )addr;

	// 将起始地址start 调整为其所在页的左边界开始位置,同时相应地调整验证区域大小.
	// 此时start 是当前进程空间中的线性地址.

	size  += start & 0xfff;
	start &= 0xfffff000;
	start += get_base( current->ldt[ 2 ] );	// 此时start 变成系统整个线性空间中的地址位置

	while ( size > 0 ) 
	{
		size -= 4096;

		// 写页面验证.若页面不可写,则复制页面
		write_verify( start );
		start += 4096;
	}
}

// 设置新任务的代码和数据段基址、限长并复制页表.
// nr 为新任务号;p 是新任务数据结构的指针
LONG copy_mem( LONG nr, Task_Struct * p )
{
	ULONG old_data_base, new_data_base, data_limit;
	ULONG old_code_base, new_code_base, code_limit;

	code_limit = get_limit( 0x0f );					// 取局部描述符表中代码段描述符项中段限长.
	data_limit = get_limit( 0x17 );					// 取局部描述符表中数据段描述符项中段限长.
	
	old_code_base = get_base( current->ldt[ 1 ] );	// 取原代码段基址.
	old_data_base = get_base( current->ldt[ 2 ] );	// 取原数据段基址.
	
	if ( old_data_base != old_code_base )			// 0.11 版不支持代码和数据段分立的情况.
	{
		panic( "We don't support separate I&D" );
	}

	if ( data_limit < code_limit )					// 如果数据段长度 < 代码段长度也不对
	{
		panic( "Bad data_limit" );
	}

	new_data_base = new_code_base = nr * 0x4000000;	// 新基址=任务号*64Mb( 任务大小 )
	p->start_code = new_code_base;
	
	set_base( p->ldt[ 1 ], new_code_base );			// 设置代码段描述符中基址域
	set_base( p->ldt[ 2 ], new_data_base );			// 设置数据段描述符中基址域
	
	if ( copy_page_tables( old_data_base, new_data_base, data_limit ) ) 
	{
		free_page_tables( new_data_base, data_limit );
		return -ENOMEM;
	}
	return 0;
}

/*
 *  Ok, this is the main fork-routine. It copies the system process
 * information ( task[ nr ] ) and sets up the necessary registers. It
 * also copies the data segment in it's entirety.
 */
/*
 * OK,下面是主要的fork 子程序.它复制系统进程信息( task[ n ] )并且设置必要的寄存器.
 * 它还整个地复制数据段.
 */
 // 复制进程 nr - 任务号
LONG 
copy_process( 
	LONG	nr, 
	LONG	ebp, 
	LONG	edi, 
	LONG	esi, 
	LONG	gs, 
	LONG	none,
	LONG	ebx, 
	LONG	ecx, 
	LONG	edx,
	LONG	fs, 
	LONG	es, 
	LONG	ds,
	LONG	eip, 
	LONG	cs, 
	LONG	eflags, 
	LONG	esp, 
	LONG	ss )
{
	Task_Struct		*p;
	I387_Struct		*m;
	LONG			i;
	File			*f;

	p = ( Task_Struct * ) get_free_page();
	if ( !p )
		return -EAGAIN;
	task[ nr ] = p;
	__asm	cld
	*p = *current;	/* NOTE! this doesn't copy the supervisor stack */

	p->state			= TASK_UNINTERRUPTIBLE;
	p->pid				= last_pid;
	p->father			= current->pid;
	p->counter			= p->priority;
	p->signal			= 0;
	p->alarm			= 0;
	p->leader			= 0;		/* process leadership doesn't inherit */
	p->utime			= p->stime = 0;
	p->cutime			= p->cstime = 0;
	p->start_time		= jiffies;
	p->tss.back_link	= 0;
	p->tss.esp0			= PAGE_SIZE + ( LONG )p;
	p->tss.ss0			= 0x10;
	p->tss.eip			= eip;
	p->tss.eflags		= eflags;
	p->tss.eax			= 0;
	p->tss.ecx			= ecx;
	p->tss.edx			= edx;
	p->tss.ebx			= ebx;
	p->tss.esp			= esp;
	p->tss.ebp			= ebp;
	p->tss.esi			= esi;
	p->tss.edi			= edi;
	p->tss.es			= es & 0xffff;
	p->tss.cs			= cs & 0xffff;
	p->tss.ss			= ss & 0xffff;
	p->tss.ds			= ds & 0xffff;
	p->tss.fs			= fs & 0xffff;
	p->tss.gs			= gs & 0xffff;
	p->tss.ldt			= _LDT( nr );

	p->tss.trace_bitmap = 0x80000000;

	m = &p->tss.i387;

	if ( last_task_used_math == current )
	{
		__asm mov		eax, m
		__asm clts
		__asm fnsave	TBYTE PTR[ eax ]
	}
	if ( copy_mem( nr, p ) ) 
	{
		task[ nr ] = NULL;
		free_page( ( LONG )p );
		return -EAGAIN;
	}

	for ( i = 0; i < NR_OPEN; i++ )
	{
		if ( f = p->filp[ i ] )
		{
			f->f_count++;
		}
	}

	if ( current->pwd )
	{
		current->pwd->i_count++;
	}
	if ( current->root )
	{
		current->root->i_count++;
	}
	if ( current->executable )
	{
		current->executable->i_count++;
	}

	set_tss_desc( gdt + ( nr << 1 ) + FIRST_TSS_ENTRY, &( p->tss ) );
	set_ldt_desc( gdt + ( nr << 1 ) + FIRST_LDT_ENTRY, &( p->ldt ) );

	p->state = TASK_RUNNING;	/* do this last, just in case */

	return last_pid;
}

// 为新进程取得不重复的进程号last_pid,并返回在任务数组中的任务号( 数组index ).
LONG find_empty_process()
{
	LONG i;

repeat:
	if ( ( ++last_pid ) < 0 ) last_pid = 1;
	for ( i = 0; i < NR_TASKS; i++ )
		if ( task[ i ] && task[ i ]->pid == last_pid ) goto repeat;
	for ( i = 1; i < NR_TASKS; i++ )
		if ( !task[ i ] )
			return i;
	return -EAGAIN;
}
