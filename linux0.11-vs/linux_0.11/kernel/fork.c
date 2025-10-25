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
 * 'fork.c'�к���ϵͳ����'fork'�ĸ����ӳ���( �μ�system_call.s ),�Լ�һЩ��������
 * ( 'verify_area' ).һ�����˽���fork,�ͻᷢ�����Ƿǳ��򵥵�,���ڴ����ȴ��Щ�Ѷ�.
 * �μ�'mm/mm.c'�е�'copy_page_tables()'.
 */
#include <errno.h>

#include <linux\sched.h>
#include <linux\kernel.h>
#include <asm\segment.h>
#include <asm\system.h>

extern VOID write_verify( ULONG address );

LONG last_pid = 0;

// ���̿ռ�����дǰ��֤����.
// �Ե�ǰ���̵ĵ�ַaddr ��addr+size ��һ�ν��̿ռ���ҳΪ��λִ��д����ǰ�ļ�����.
// ��ҳ����ֻ����,��ִ�й������͸���ҳ�����( дʱ���� )
VOID verify_area( VOID * addr, LONG size )
{
	ULONG start;

	start = ( ULONG )addr;

	// ����ʼ��ַstart ����Ϊ������ҳ����߽翪ʼλ��,ͬʱ��Ӧ�ص�����֤�����С.
	// ��ʱstart �ǵ�ǰ���̿ռ��е����Ե�ַ.

	size  += start & 0xfff;
	start &= 0xfffff000;
	start += get_base( current->ldt[ 2 ] );	// ��ʱstart ���ϵͳ�������Կռ��еĵ�ַλ��

	while ( size > 0 ) 
	{
		size -= 4096;

		// дҳ����֤.��ҳ�治��д,����ҳ��
		write_verify( start );
		start += 4096;
	}
}

// ����������Ĵ�������ݶλ�ַ���޳�������ҳ��.
// nr Ϊ�������;p �����������ݽṹ��ָ��
LONG copy_mem( LONG nr, Task_Struct * p )
{
	ULONG old_data_base, new_data_base, data_limit;
	ULONG old_code_base, new_code_base, code_limit;

	code_limit = get_limit( 0x0f );					// ȡ�ֲ����������д�������������ж��޳�.
	data_limit = get_limit( 0x17 );					// ȡ�ֲ��������������ݶ����������ж��޳�.
	
	old_code_base = get_base( current->ldt[ 1 ] );	// ȡԭ����λ�ַ.
	old_data_base = get_base( current->ldt[ 2 ] );	// ȡԭ���ݶλ�ַ.
	
	if ( old_data_base != old_code_base )			// 0.11 �治֧�ִ�������ݶη��������.
	{
		panic( "We don't support separate I&D" );
	}

	if ( data_limit < code_limit )					// ������ݶγ��� < ����γ���Ҳ����
	{
		panic( "Bad data_limit" );
	}

	new_data_base = new_code_base = nr * 0x4000000;	// �»�ַ=�����*64Mb( �����С )
	p->start_code = new_code_base;
	
	set_base( p->ldt[ 1 ], new_code_base );			// ���ô�����������л�ַ��
	set_base( p->ldt[ 2 ], new_data_base );			// �������ݶ��������л�ַ��
	
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
 * OK,��������Ҫ��fork �ӳ���.������ϵͳ������Ϣ( task[ n ] )�������ñ�Ҫ�ļĴ���.
 * ���������ظ������ݶ�.
 */
 // ���ƽ��� nr - �����
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

// Ϊ�½���ȡ�ò��ظ��Ľ��̺�last_pid,�����������������е������( ����index ).
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
