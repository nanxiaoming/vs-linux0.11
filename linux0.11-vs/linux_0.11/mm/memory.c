/*
 *  linux/mm/memory.c
 *
 *  ( C ) 1991  Linus Torvalds
 */

/*
 * demand-loading started 01.12.91 - seems it is high on the list of
 * things wanted, and it should be easy to implement. - Linus
 */

/*
 * Ok, demand-loading was easy, shared pages a little bit tricker. Shared
 * pages started 02.12.91, seems to work. - Linus.
 *
 * Tested sharing by executing about 30 /bin/sh: under the old kernel it
 * would have taken more than the 6M I have free, but it worked well as
 * far as I could see.
 *
 * Also corrected some "invalidate()"s - I wasn't doing enough of them.
 */

/*
 * OK,需求加载是比较容易编写的,而共享页面却需要有点技巧.共享页面程序是
 * 02.12.91 开始编写的,好象能够工作- Linus.
 *
 * 通过执行大约30 个/bin/sh 对共享操作进行了测试:在老内核当中需要占用多于
 * 6M 的内存,而目前却不用.现在看来工作得很好.
 *
 * 对"invalidate()"函数也进行了修正- 在这方面我还做的不够.
 */


 /* 
  * 
  *--------------------------------------------------------------------
  * 
  * Virtual address 'Stuct'
  *
  * +--------10------+-------10-------+---------12----------+
  * | Page Directory |   Page Table   |		  Offset        |
  * |      Index     |      Index     |       in Page       |
  * +----------------+----------------+---------------------+
  *  \--- PDX(va) --/ \--- PTX(va) --/
  * 
  *--------------------------------------------------------------------
  * 
  * Page Director Entry - 4k 分页
  * |-----------------20-----------------|---------12----------|
  * +------------------------------------+---+-+-+-+-+-+-+-+-+-+
  * |                                    | A | |P| | |P|P|U|R|P|
  * |        PageTable Address           | V |0| |0|A|C|W|/|/| |
  * |                                    | L | |S| | |D|T|S|W| |
  * +------------------------------------+---+-+-+-+-+-+-+-+-+-+
  * 
  *--------------------------------------------------------------------
  * 
  * P/S （PageSize）位，只对PDE有意义.0 -- 4K 页面, 4M页面,直接指向物理页
  *  
  * 
  * Page Table Entry - 4k 分页
  * |-----------------20-----------------|---------12----------|
  * +------------------------------------+---+-+-+-+-+-+-+-+-+-+
  * |                                    | A | |P| | |P|P|U|R|P|
  * |           Page Address             | V |G|A|D|A|C|W|/|/| |
  * |                                    | L | |T| | |D|T|S|W| |
  * +------------------------------------+---+-+-+-+-+-+-+-+-+-+
  * 
  *-------------------------------------------------------------------- 
  * 
  * 0.P  ( Present			)位:	 存在位.
  * 1.R/W( Read/Write		)位:	 0只读,1 读写
  * 2.U/S( User/System		)位: 0特权用户,1普通用户
  * 3.PWT( Write through	)位
  * 4.PCD( Cache disbale	)位
  * 5.A  ( Accessed			)位
  * 6.D  ( Dirty			)位: PTE有意义,0表示没有被写过，1表示被写过
  * 7.PAT( Page Attr Access )位
  * 8.G  ( Global			)位
  * 9.AVL( Available        )位
  * 
  * 
  *  // page directory index
  *	 #define PDX(va)         (((uint)(va) >> PDXSHIFT) & 0x3FF)
  *
  *	 // page table index
  *	 #define PTX(va)         (((uint)(va) >> PTXSHIFT) & 0x3FF)
  *
  *	 // construct virtual address from indexes and offset
  *	 #define PGADDR(d, t, o) ((uint)((d) << PDXSHIFT | (t) << PTXSHIFT | (o)))
  *
  *	 // Page directory and page table constants.
  *	 #define NPDENTRIES      1024    // # directory entries per page directory
  *	 #define NPTENTRIES      1024    // # PTEs per page table
  *	 #define PGSIZE          4096    // bytes mapped by a page
  *
  *	 #define PGSHIFT         12      // log2(PGSIZE)
  *	 #define PTXSHIFT        12      // offset of PTX in a linear address
  *	 #define PDXSHIFT        22      // offset of PDX in a linear address
  *
  *	 #define PGROUNDUP(sz)  (((sz)+PGSIZE-1) & ~(PGSIZE-1))
  *	 #define PGROUNDDOWN(a) (((a)) & ~(PGSIZE-1))
  *
  *	 // Page table/directory entry flags.
  *	 #define PTE_P           0x001   // Present
  *	 #define PTE_W           0x002   // Writeable
  *	 #define PTE_U           0x004   // User
  *	 #define PTE_PWT         0x008   // Write-Through
  *	 #define PTE_PCD         0x010   // Cache-Disable
  *	 #define PTE_A           0x020   // Accessed
  *	 #define PTE_D           0x040   // Dirty
  *	 #define PTE_PS          0x080   // Page Size
  *	 #define PTE_MBZ         0x180   // Bits must be zero
  *
  *	 // Address in page table or page directory entry
  *	 #define PTE_ADDR(pte)   ((uint)(pte) & ~0xFFF)
  *	 #define PTE_FLAGS(pte)  ((uint)(pte) &  0xFFF)
  * 
  */
#include <signal.h>

#include <linux\sched.h>
#include <linux\head.h>
#include <linux\kernel.h>

volatile VOID do_exit( LONG code );

static __inline volatile VOID oom()
{
	printk( "out of memory\n\r" );
	do_exit( SIGSEGV );
}

/*
 * 刷新页变换高速缓冲宏函数.
 * 为了提高地址转换的效率,CPU 将最近使用的页表数据存放在芯片中高速缓冲中.
 * 在修改过页表信息之后,就需要刷新该缓冲区.这里使用重新加载页目录基址
 * 寄存器cr3 的方法来进行刷新.下面 eax = 0 ,是页目录的基址.
 */
#define invalidate() \
	__asm xor	eax, eax \
	__asm mov	cr3, eax

/*
 * 
 *  下面定义若需要改动,则需要与 head.s 等文件中的相关信息一起改变 
 *  linux 0.11 内核默认支持的最大内存容量是 16M ,可以修改这些定义以适合更多的内存 
 * 
 */
#define LOW_MEM			0x100000						// 内存低端( 1MB ).
#define PAGING_MEMORY	( 15*1024*1024 )				// 分页内存15MB.主内存区最多15M.
#define PAGING_PAGES	( PAGING_MEMORY>>12 )			// 分页后的物理内存页数.
#define MAP_NR( addr )	( ( ( addr )-LOW_MEM )>>12 )	// 指定物理内存地址映射为页号
#define USED			100								// 页面被占用标志

// 
// 该宏用于判断给定地址是否位于当前进程的代码段中
// 
#define CODE_SPACE( addr ) ( ( ( ( addr )+4095 )&~4095 ) < current->start_code + current->end_code )
			
//
// 全局变量,存放实际物理内存最高端地址
//
static ULONG HIGH_MEMORY = 0;

#define copy_page( from, to )	\
	__asm mov	esi, from		\
	__asm mov	edi, to			\
	__asm mov	ecx, 1024		\
	__asm cld					\
	__asm rep	movsd

//
// 内存映射字节图( 1 字节代表1 页内存 ),每个页面对应
// 的字节用于标志页面当前被引用( 占用 )次数.
//
static UCHAR mem_map[ PAGING_PAGES ] = { 0, };

/*
 * Get physical address of first ( actually last :- ) free page, and mark it used.
 * If no free pages left, return 0.
 */

ULONG get_free_page()
/*++

Routine Description:

	get_free_page 获取一个空闲页面,并标记为已使用.如果没有空闲页面,就返回0.

Arguments:

	VOID

Return Value:

	VOID

--*/
{
	register ULONG __res;

	VOID *D = mem_map + PAGING_PAGES - 1;

	__asm
	{
		xor	eax	, eax
		mov	edi	, D
		mov	ecx	, PAGING_PAGES
		std
		repne scasb				// 将al( 0 )与对应( di )每个页面的内容比较,
		jne	LN1					// 如果没有等于0 的字节,则跳转结束( 返回0 )
		mov	1[edi] , 1			// 将对应页面的内存映像位置1
		sal	ecx	, 12			// 页面数*4K = 相对页面起始地址
		add	ecx	, LOW_MEM		// 再加上低端内存地址,即获得页面实际物理起始地址
		mov	edx	, ecx			// 将页面实际起始地址 -> edx 寄存器
		mov	ecx	, 1024			// 寄存器ecx 置计数值1024
		lea	edi	, 4092[edx];	// 将4092+edx 的位置 -> edi( 该页面的末端 )
		rep	stosd				// 将edi 所指内存清零( 反方向,也即将该页面清零 )
		mov	eax	, edx
LN1 :
		mov	__res, eax
	}
	return __res;
}

VOID free_page( ULONG addr )
/*++

Routine Description:

	free_page 释放一个页面.本版 linux 物理内存和虚拟内存数值相等

Arguments:

	addr - 物理/虚拟内存地址

Return Value:

	VOID

--*/
{
	if ( addr < LOW_MEM )
		return;

	if ( addr >= HIGH_MEMORY )
		panic( "trying to free nonexistent page" );
	
	addr -= LOW_MEM;
	addr >>= 12;		// 得到页面号

	if ( mem_map[ addr ]-- )
		return;

	mem_map[ addr ] = 0;	// 到此处出错,可能引用和释放不对称,显示出错信息,死机

	panic( "trying to free free page" );
}

/*
 * This function frees a continuos block of page tables, as needed
 * by 'exit()'. As does copy_page_tables(), this handles only 4Mb blocks.
 */
LONG free_page_tables( ULONG from, ULONG size )
/*++

Routine Description:

	free_page_tables.释放以4M为单位的虚拟内存块.

Arguments:

	from - 起始的虚拟地址
	size - 内存块的BYTE长度

Return Value:

	0 成功,失败则panic
	
--*/

{
	ULONG * pg_table;
	ULONG * dir, nr;

	if ( from & 0x3fffff )	// 4M边界
		panic( "free_page_tables called with wrong alignment" );
	if ( !from )
		panic( "Trying to free up swapper memory space" );

	// 
	// 计算 4M 的数倍,也就是页目录项个数. 例如:
	// ( 1        - 0x3fffff ) -> 1
	// ( 0x400000 - 0x7fffff ) -> 2
	// 
	// 一个页目录项管理的正好管理4M内存.目录项指向1024页表,每个页表项指向一个页面4K,所以一个页目录项管理4M内存
	// 
	// PDE -> PT0 -> 4K		
	//		  PT1 -> 4K	
	//		  PT2 -> 4K
	//	      ...
	//		  PT1023
	//
	
	size = ( size + 0x3fffff ) >> 22;

	//
	// 下面一句计算起始目录项.对应的目录项号=from>>22,因每项占4 字节,并且由于页目录是从
	// 物理地址0 开始,因此实际的目录项指针=目录项号<<2,也即( from>>20 ).与上0xffc 确保
	// 目录项指针范围有效.
	//

	//
	// 这里的dir就是页目录表 , *dir 就是页目录项
	// 
	// size 和 *dir 完全是一个级别的东西
	//
	dir = ( ULONG * )( ( from >> 20 ) & 0xffc );

	for ( ; size-- > 0; dir++ )
	{
		if ( !( 1 & *dir ) )
		{
			continue;	//PDE的P位为0
		}

		//
		//  pg_table 就是页表
		// *pg_table 就是页表项
		//
		pg_table = ( ULONG * )( 0xfffff000 & *dir );
		
		for ( nr = 0; nr < 1024; nr++ )
		{	
			if ( 1 & *pg_table )
				free_page( 0xfffff000 & *pg_table );
			
			*pg_table = 0;
			 pg_table++;	
		}

		free_page( 0xfffff000 & *dir );
		*dir = 0;
	}

	invalidate();
	return 0;
}

/*
 * Well, here is one of the most complicated functions in mm. It
 * copies a range of linerar addresses by copying only the pages.
 * Let's hope this is bug-free, 'cause this one I don't want to debug :- )
 *
 * Note! We don't copy just any chunks of memory - addresses have to
 * be divisible by 4Mb ( one page-directory entry ), as this makes the
 * function easier. It's used only by fork anyway.
 *
 * NOTE 2!! When from==0 we are copying kernel space for the first
 * fork(). Then we DONT want to copy a full page-directory entry, as
 * that would lead to some serious memory waste - we just copy the
 * first 160 pages - 640kB. Even that is more than we need, but it
 * doesn't take any more memory - we don't copy-on-write in the low
 * 1 Mb-range, so the pages can be shared with the kernel. Thus the
 * special case for nr=xxxx.
 */
LONG copy_page_tables( ULONG from, ULONG to, LONG size )
/*++

Routine Description:

	copy_page_tables.
	复制页表.注意不是复制内存内容,而是共享数据内容。
	复制的是 PDE-PTE 等对应给管理内容.新的页表项为只读内容.修改新的页面触发写时复制机制

Arguments:

	from	- 源地址
	to		- 目标地址
	size	- 大小

Return Value:

	0 成功,失败则panic

--*/
{
	ULONG * from_page_table;
	ULONG * to_page_table;
	ULONG   this_page;
	ULONG * from_dir, *to_dir;
	ULONG   nr;

	if ( ( from & 0x3fffff ) || ( to & 0x3fffff ) )
	{
		panic( "copy_page_tables called with wrong alignment" );
	}

	from_dir = ( ULONG * )( ( from >> 20 ) & 0xffc ); /* _pg_dir = 0 */
	to_dir   = ( ULONG * )( ( to   >> 20 ) & 0xffc );

	// 
	// 计算 4M 的数倍,也就是页目录项个数. 例如:
	// ( 1        - 0x3fffff ) -> 1
	// ( 0x400000 - 0x7fffff ) -> 2
	// 
	// 一个页目录项管理的正好管理4M内存.目录项指向1024页表,每个页表项指向一个页面4K,所以一个页目录项管理4M内存
	// 
	// PDE -> PT0 -> 4K
	//		  PT1 -> 4K
	//		  PT2 -> 4K
	//	      ...
	//		  PT1023
	//

	size = ( size + 0x3fffff ) >> 22;

	for ( ; size-- > 0; from_dir++, to_dir++ ) 
	{
		if ( 1 & *to_dir )
		{
			panic( "copy_page_tables: already exist" );
		}

		if ( !( 1 & *from_dir ) )
		{
			continue;
		}

		// 取当前源目录项中页表的地址 -> from_page_table
		from_page_table = ( ULONG * )( 0xfffff000 & *from_dir );

		// 申请空闲页面, 用于是存储 '目的页表'
		if ( !( to_page_table = ( ULONG * )get_free_page() ) )
		{
			return -1;
		}

		// 设置目的目录项信息.7 是标志信息,表示( Usr, R/W, Present ).
		*to_dir = ( ( ULONG )to_page_table ) | 7;

		// 针对当前处理的页表,设置需复制的页面数.如果是在内核空间,则仅需复制头 160 页,
		// 否则需要复制 1 个页表中的所有 1024 页面.
		nr = ( from == 0 ) ? 160 : 1024;

		// 对于当前页表,开始复制指定数目 nr 个内存页面
		for ( ; nr-- > 0; from_page_table++, to_page_table++ )
		{
			this_page = *from_page_table;

			if ( !( 1 & this_page ) )
			{
				continue;
			}

			// 复位页表项中R/W 标志(置0).(如果U/S 位是0,则R/W 就没有作用.如果U/S 是1,而R/W 是0,
			// 那么运行在用户层的代码就只能读页面.如果U/S 和R/W 都置位,则就有写的权限.)

			this_page     &= ~2;
			*to_page_table = this_page;	// 将该页表项复制到目的页表中.

			// 1M 以上,需要填充mem_map[]页面号
			// 并以它为索引在页面映射数组相应项中增加引用次数.
			if ( this_page > LOW_MEM )
			{
				*from_page_table  = this_page;
				this_page		 -= LOW_MEM;
				this_page		>>= 12;

				mem_map[ this_page ]++;
			}
		}
	}
	invalidate();
	return 0;
}

/*
 * This function puts a page in memory at the wanted address.
 * It returns the physical address of the page gotten, 0 if
 * out of memory ( either when trying to access page-table or
 * page. )
 */
ULONG put_page( ULONG page, ULONG address )
/*++

Routine Description:

	将物理页面挂到指定虚拟地址上

Arguments:

	page	- 物理地址
	address	- 虚拟地址

Return Value:

	address - 成功 返回物理地址
	0		- 失败

--*/

{
	ULONG tmp, *page_table;

	/* NOTE !!! This uses the fact that _pg_dir=0 */

	if ( page < LOW_MEM || page >= HIGH_MEMORY )
		printk( "Trying to put page %p at %p\n", page, address );

	// 映射字节图中没有置位,此处应该panic
	if ( mem_map[ ( page - LOW_MEM ) >> 12 ] != 1 )
		printk( "mem_map disagrees with %p at %p\n", page, address );

	// page_table页目录表 , *page_table 页目录表项
	page_table = ( ULONG * )( ( address >> 20 ) & 0xffc );

	if ( ( *page_table ) & 1 )
	{
		// 页目录表项( P=1 ) 直接提取页表
		page_table = ( ULONG * )( 0xfffff000 & *page_table );
	}
	else 
	{
		// 否则,申请空闲页面给页表使用,相应标志7( User, U/S, R/W ).
		if ( !( tmp = get_free_page() ) )
		{
			return 0;
		}
		
		*page_table = tmp | 7;

		//页表项挂在页目录表上
		page_table = ( ULONG * )tmp;
	}

	//页面挂在页表项上
	page_table[ ( address >> 12 ) & 0x3ff ] = page | 7;
	
	return page;
}

VOID un_wp_page( ULONG * table_entry )
/*++

Routine Description:

	取消页面的写保护,用于页异常中断过程中写保护异常的处理(写时复制)

Arguments:

	table_entry - 页表项

Return Value:

	VOID

--*/
{
	ULONG old_page, new_page;

	// old_page - 实际上是页再页表中的号码
	old_page = 0xfffff000 & *table_entry;

	//
	// 如果大于1M,再map中的值为1,说明被引用一次.
	// 直接该属性就行
	//

	if ( ( old_page >= LOW_MEM ) && ( mem_map[ MAP_NR( old_page ) ] == 1 ) )
	{
		*table_entry |= 2;
		invalidate();
		return;
	}

	if ( !( new_page = get_free_page() ) )
		oom();
	//
	// 如果大于1M,再map中的值大于1,说明被引用多次.
	// 这时候申请一个新的页面,设置合适属性(U/S, R/W, P => 7 )
	// 并挂载页面,拷贝页面内容
	//
	if ( old_page >= LOW_MEM )
	{
		mem_map[ MAP_NR( old_page ) ]--;
	}

	*table_entry = new_page | 7;
	
	invalidate();

	copy_page( old_page, new_page );
}

/*
* This routine handles present pages, when users try to write
* to a shared page. It is done by copying the page to a new address
* and decrementing the shared-page counter for the old page.
*
* If it's in code space we exit with a segment error.
*/

VOID do_wp_page( ULONG error_code, ULONG address )
/*++

Routine Description:

	do_wp_page 页异常处理函数, page.s程序中调用该函数.

Arguments:

	error_code	- cpu产生
	address		- cpu提供,cr2的值,当时的虚拟地址

Return Value:

	VOID

--*/
{
	un_wp_page( 
				( ULONG * )
					( 
						( ( address >> 10 ) & 0xffc ) + ( 0xfffff000 & *( ( ULONG * )( ( address >> 20 ) & 0xffc ) ) )
					) 
			  );
}

VOID write_verify( ULONG address )
/*++

Routine Description:

	write_verify - 验证虚拟地址可写

Arguments:

	address - 虚拟地址

Return Value:

	页面无效返回NULL;如果页面不可写,则复制页面

--*/
{
	ULONG page;

	// 校验'页目录项'的'P'位置.
	
	if ( !( ( page = *( ( ULONG * )( ( address >> 20 ) & 0xffc ) ) ) & 1 ) )
		return;

	// 取页表的地址,加上指定地址的页面在页表中的页表项偏移值,得对应物理页面的页表项指针.
	page &= 0xfffff000;
	page += ( ( address >> 10 ) & 0xffc );

	// 如果该页面不可写( 标志R/W 没有置位 ),则执行共享检验和复制页面操作( 写时复制 ).

	if ( ( 3 & *( ULONG * )page ) == 1 )  /* non-writeable, present */
		un_wp_page( ( ULONG * )page );
	return;
}

VOID get_empty_page( ULONG address )
/*++

Routine Description:

	将虚拟地址映射一个新的物理页面, get_free_page 和 put_page 的组合

Arguments:

	address - 虚拟地址

Return Value:

	VOID

--*/
{
	ULONG tmp;

	if ( !( tmp = get_free_page() ) || !put_page( tmp, address ) ) 
	{
		free_page( tmp );		/* 0 is ok - ignored */
		oom();
	}
}

/*
 * try_to_share() checks the page at address "address" in the task "p",
 * to see if it exists, and if it is clean. If so, share it with the current
 * task.
 *
 * NOTE! This assumes we have checked that p != current, and that they
 * share the same executable.
 */

static LONG try_to_share( ULONG address , Task_Struct * p )
/*++

Routine Description:

	try_to_share.尝试指定进程+地址页面的共享操作.

	将以上信息共享到当前进程,出错则panic

	该函数假设 current != p , 且具有相同的执行体

Arguments:

	address - p进程内的虚拟地址
	p		- 指定的进程

Return Value:

	1 - 成功
	0 - 失败
--*/
{
	ULONG	from;			// 页表项
	ULONG	to;				// 页表项
	ULONG	from_page;		// 页目录项
	ULONG	to_page;		// 页目录项
	ULONG	phys_addr;

	// 求指定内存地址的页目录项
	from_page = to_page = ( ( address >> 20 ) & 0xffc );

	// p进程的 起始代码所在的页目录项
	from_page += ( ( p->start_code >> 20 ) & 0xffc );

	// 当前进程的 起始代码所在的页目录项
	to_page += ( ( current->start_code >> 20 ) & 0xffc );

	from = *( ULONG * )from_page;
	
	if ( !( from & 1 ) )
	{
		// from的页目录项P位为0
		return 0;
	}

	from &= 0xfffff000;
	
	//这句话相当于指针相加,from 是Address所在页面基地址,address >> 10=> (address >> 12) << 4 => 为偏移值
	from_page	= from + ( ( address >> 10 ) & 0xffc );  
	phys_addr	= *( ULONG * )from_page;

	// 0x41 对应页表项中的 Dirty 和 Present 标志.如果页面不干净或无效则返回.
	if ( ( phys_addr & 0x41 ) != 0x01 )
	{
		return 0;
	}
	// 取页面的地址 -> phys_addr.如果该页面地址不存在或小于内存低端( 1M )也返回退出.
	phys_addr &= 0xfffff000;

	if ( phys_addr >= HIGH_MEMORY || phys_addr < LOW_MEM )
	{
		return 0;
	}

	// 对当前进程页面进行操作.
	// 取页目录项内容 -> to.如果该目录项无效( P=0 ),则取空闲页面,并更新to_page 所指的目录项.

	to = *( ULONG * )to_page;

	if ( !( to & 1 ) )
	{
		if (to = get_free_page())
		{
			*(ULONG*)to_page = to | 7;
		}
		else
		{
			oom();
		}
	}
	// 取对应页表地址 -> to,页表项地址 to_page.如果对应的页面已经存在,则出错,死机.
	to &= 0xfffff000;
	to_page = to + ( ( address >> 10 ) & 0xffc );
	if ( 1 & *( ULONG * )to_page )
	{
		panic( "try_to_share: to_page already exists" );
	}
	/* share them: write-protect */

	// 对它们进行共享处理:写保护 
	// 对 p 进程中页面置写保护标志( 置R/W=0 只读 ).并且当前进程中的对应页表项指向它.

	*( ULONG * )from_page &= ~2;
	*( ULONG * )to_page    = *( ULONG * )from_page;
	invalidate();
	phys_addr -= LOW_MEM;
	phys_addr >>= 12;

	// 计算所操作页面的页面号,并将对应页面映射数组项中的引用递增1

	mem_map[phys_addr]++;
	return 1;
}

static LONG share_page( ULONG address )
/*++

Routine Description:

	share_page.试图找到一个进程,它可以与当前进程共享页面.
	遍历任务列表,对具有相同的可执行文件的进程进行空间共享

Arguments:

	address - 当前进程的虚拟地址

Return Value:

	1 - 成功
	0 - 失败

--*/
{
	Task_Struct ** p;

	if ( !current->executable )
	{
		return 0;
	}

	// 如果只能单独执行( executable->i_count=1 )退出
	if ( current->executable->i_count < 2 )
	{
		return 0;
	}

	for ( p = &LAST_TASK; p > &FIRST_TASK; --p ) 
	{
		if ( !*p )
		{
			continue;
		}

		if ( current == *p )
		{
			continue;
		}

		if ( ( *p )->executable != current->executable )
		{
			continue;
		}

		if ( try_to_share( address, *p ) )	// 尝试共享页面
		{
			return 1;
		}
	}
	return 0;
}

VOID do_no_page( ULONG error_code, ULONG address )
/*++

Routine Description:

	do_no_page.

	缺页中断处处理函数.由 page.s 程序中被调用.
	参数error_code 是由CPU 自动产生,address 是页面线性地址

Arguments:

	error_code	- cpu产生
	address		- cpu提供,cr2的值,当时的虚拟地址

Return Value:

	VOID

--*/
{
	LONG	nr[4];
	ULONG	tmp;
	ULONG	page;
	LONG	block, i;

	address &= 0xfffff000;	
	
	tmp = address - current->start_code;//虚拟地址到进程基地址的偏移

	//
	// 如果 当前进程的executable空,或者指定地址超出代码+数据长度,则申请一页物理内存,并映射映射到指定的虚拟地址处.
	// 1.executable 是进程的i 节点结构.该值为0,表明进程刚开始设置,需要内存
	// 2.指定的线性地址超出代码加数据长度,表明进程在申请新的内存空间,也需要给予.
	// 
	// 因此就直接调用 get_empty_page() 函数,申请一页物理内存并映射到指定线性地址处即可.
	// start_code是进程代码段地址,end_data 是代码加数据长度.对于 linux 内核,它的代码段和
	// 数据段是起始基址是相同的.
	//
	if ( !current->executable || tmp >= current->end_data )
	{
		get_empty_page( address );
		return;
	}

	// 如果尝试共享页面成功,则退出
	if ( share_page( tmp ) )
	{
		return;
	}

	// 取空闲页面,如果内存不够了,则显示内存不够,终止进程
	if ( !( page = get_free_page() ) )
	{
		oom();
	}

	/* remember that 1 block is used for header */
	/* 记住,( 程序 )头要使用1个数据块 */
	// 首先计算缺页所在的数据块项.BLOCK_SIZE = 1024 字节,因此一页内存需要4 个数据块.
	block = 1 + tmp / BLOCK_SIZE;

	for ( i = 0 ; i<4 ; block++, i++ )
	{
		nr[ i ] = bmap( current->executable, block );
	}

	// 读设备上一个页面的数据( 4 个逻辑块 )到指定物理地址page 处
	bread_page( page, current->executable->i_dev, nr );

	// 在增加了一页内存后,该页内存的部分可能会超过进程的end_data 位置.下面的循环即是对物理
	// 页面超出的部分进行清零处理.

	i   = tmp  + 4096 - current->end_data;
	tmp = page + 4096;

	while ( i-- > 0 )
	{
		tmp--;
		*(CHAR *)tmp = 0;
	}

	// 如果把物理页面映射到指定线性地址的操作成功,就返回.否则就释放内存页,显示内存不够.
	if ( put_page( page, address ) )
	{
		return;
	}
	free_page( page );
	oom();
}

VOID mem_init( LONG start_mem, LONG end_mem )
/*++

Routine Description:

	mem_init 初始化内存管理

	将可用作分页处理的物理内存起始位置( 已去除RAMDISK 所占内存空间等 ).
	到实际物理内存最大地址初始化.

	在该版的 linux 内核中,最多能使用 16Mb 的内存,大于16Mb 的内存将不于考虑,弃置不用.

	0 - 1Mb 内存空间用于内核系统( 其实是0-640Kb ).

Arguments:

	start_mem	- 起始物理内存
	end_mem		- 终止物理内存

Return Value:

	VOID

--*/
{
	LONG i;

	HIGH_MEMORY = end_mem;

	for ( i = 0; i<PAGING_PAGES; i++ )
	{
		mem_map[ i ] = USED;
	}
	i = MAP_NR( start_mem ); //除去系统使用内存的物理页号

	end_mem -= start_mem;
	end_mem >>= 12;

	while ( end_mem-->0 )
	{
		mem_map[ i++ ] = 0;	//位图初始化
	}
}

VOID calc_mem()
/*++

Routine Description:

	calc_mem-计算内存空闲页面数并显示

Arguments:

	VOID

Return Value:

	VOID

--*/
{
	LONG i, j, k, free = 0;
	LONG * pg_tbl;

	for ( i = 0; i < PAGING_PAGES; i++ )
	{
		if ( !mem_map[ i ] ) 
		{
			free++;
		}
	}
		
	printk( "%d pages free ( of %d )\n\r", free, PAGING_PAGES );

	for ( i = 2; i < 1024; i++ ) 
	{
		if ( 1 & pg_dir[ i ] ) 
		{
			pg_tbl = ( LONG * )( 0xfffff000 & pg_dir[ i ] );
		
			for ( j = k = 0; j < 1024; j++ )
			{
				if ( pg_tbl[ j ] & 1 )
				{
					k++;
				}
			}
			printk( "Pg-dir[ %d ] uses %d pages\n", i, k );
		}
	}
}
