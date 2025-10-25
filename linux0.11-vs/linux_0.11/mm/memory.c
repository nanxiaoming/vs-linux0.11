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
 * OK,��������ǱȽ����ױ�д��,������ҳ��ȴ��Ҫ�е㼼��.����ҳ�������
 * 02.12.91 ��ʼ��д��,�����ܹ�����- Linus.
 *
 * ͨ��ִ�д�Լ30 ��/bin/sh �Թ�����������˲���:�����ں˵�����Ҫռ�ö���
 * 6M ���ڴ�,��Ŀǰȴ����.���ڿ��������úܺ�.
 *
 * ��"invalidate()"����Ҳ����������- ���ⷽ���һ����Ĳ���.
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
  * Page Director Entry - 4k ��ҳ
  * |-----------------20-----------------|---------12----------|
  * +------------------------------------+---+-+-+-+-+-+-+-+-+-+
  * |                                    | A | |P| | |P|P|U|R|P|
  * |        PageTable Address           | V |0| |0|A|C|W|/|/| |
  * |                                    | L | |S| | |D|T|S|W| |
  * +------------------------------------+---+-+-+-+-+-+-+-+-+-+
  * 
  *--------------------------------------------------------------------
  * 
  * P/S ��PageSize��λ��ֻ��PDE������.0 -- 4K ҳ��, 4Mҳ��,ֱ��ָ������ҳ
  *  
  * 
  * Page Table Entry - 4k ��ҳ
  * |-----------------20-----------------|---------12----------|
  * +------------------------------------+---+-+-+-+-+-+-+-+-+-+
  * |                                    | A | |P| | |P|P|U|R|P|
  * |           Page Address             | V |G|A|D|A|C|W|/|/| |
  * |                                    | L | |T| | |D|T|S|W| |
  * +------------------------------------+---+-+-+-+-+-+-+-+-+-+
  * 
  *-------------------------------------------------------------------- 
  * 
  * 0.P  ( Present			)λ:	 ����λ.
  * 1.R/W( Read/Write		)λ:	 0ֻ��,1 ��д
  * 2.U/S( User/System		)λ: 0��Ȩ�û�,1��ͨ�û�
  * 3.PWT( Write through	)λ
  * 4.PCD( Cache disbale	)λ
  * 5.A  ( Accessed			)λ
  * 6.D  ( Dirty			)λ: PTE������,0��ʾû�б�д����1��ʾ��д��
  * 7.PAT( Page Attr Access )λ
  * 8.G  ( Global			)λ
  * 9.AVL( Available        )λ
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
 * ˢ��ҳ�任���ٻ���꺯��.
 * Ϊ����ߵ�ַת����Ч��,CPU �����ʹ�õ�ҳ�����ݴ����оƬ�и��ٻ�����.
 * ���޸Ĺ�ҳ����Ϣ֮��,����Ҫˢ�¸û�����.����ʹ�����¼���ҳĿ¼��ַ
 * �Ĵ���cr3 �ķ���������ˢ��.���� eax = 0 ,��ҳĿ¼�Ļ�ַ.
 */
#define invalidate() \
	__asm xor	eax, eax \
	__asm mov	cr3, eax

/*
 * 
 *  ���涨������Ҫ�Ķ�,����Ҫ�� head.s ���ļ��е������Ϣһ��ı� 
 *  linux 0.11 �ں�Ĭ��֧�ֵ�����ڴ������� 16M ,�����޸���Щ�������ʺϸ�����ڴ� 
 * 
 */
#define LOW_MEM			0x100000						// �ڴ�Ͷ�( 1MB ).
#define PAGING_MEMORY	( 15*1024*1024 )				// ��ҳ�ڴ�15MB.���ڴ������15M.
#define PAGING_PAGES	( PAGING_MEMORY>>12 )			// ��ҳ��������ڴ�ҳ��.
#define MAP_NR( addr )	( ( ( addr )-LOW_MEM )>>12 )	// ָ�������ڴ��ַӳ��Ϊҳ��
#define USED			100								// ҳ�汻ռ�ñ�־

// 
// �ú������жϸ�����ַ�Ƿ�λ�ڵ�ǰ���̵Ĵ������
// 
#define CODE_SPACE( addr ) ( ( ( ( addr )+4095 )&~4095 ) < current->start_code + current->end_code )
			
//
// ȫ�ֱ���,���ʵ�������ڴ���߶˵�ַ
//
static ULONG HIGH_MEMORY = 0;

#define copy_page( from, to )	\
	__asm mov	esi, from		\
	__asm mov	edi, to			\
	__asm mov	ecx, 1024		\
	__asm cld					\
	__asm rep	movsd

//
// �ڴ�ӳ���ֽ�ͼ( 1 �ֽڴ���1 ҳ�ڴ� ),ÿ��ҳ���Ӧ
// ���ֽ����ڱ�־ҳ�浱ǰ������( ռ�� )����.
//
static UCHAR mem_map[ PAGING_PAGES ] = { 0, };

/*
 * Get physical address of first ( actually last :- ) free page, and mark it used.
 * If no free pages left, return 0.
 */

ULONG get_free_page()
/*++

Routine Description:

	get_free_page ��ȡһ������ҳ��,�����Ϊ��ʹ��.���û�п���ҳ��,�ͷ���0.

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
		repne scasb				// ��al( 0 )���Ӧ( di )ÿ��ҳ������ݱȽ�,
		jne	LN1					// ���û�е���0 ���ֽ�,����ת����( ����0 )
		mov	1[edi] , 1			// ����Ӧҳ����ڴ�ӳ��λ��1
		sal	ecx	, 12			// ҳ����*4K = ���ҳ����ʼ��ַ
		add	ecx	, LOW_MEM		// �ټ��ϵͶ��ڴ��ַ,�����ҳ��ʵ��������ʼ��ַ
		mov	edx	, ecx			// ��ҳ��ʵ����ʼ��ַ -> edx �Ĵ���
		mov	ecx	, 1024			// �Ĵ���ecx �ü���ֵ1024
		lea	edi	, 4092[edx];	// ��4092+edx ��λ�� -> edi( ��ҳ���ĩ�� )
		rep	stosd				// ��edi ��ָ�ڴ�����( ������,Ҳ������ҳ������ )
		mov	eax	, edx
LN1 :
		mov	__res, eax
	}
	return __res;
}

VOID free_page( ULONG addr )
/*++

Routine Description:

	free_page �ͷ�һ��ҳ��.���� linux �����ڴ�������ڴ���ֵ���

Arguments:

	addr - ����/�����ڴ��ַ

Return Value:

	VOID

--*/
{
	if ( addr < LOW_MEM )
		return;

	if ( addr >= HIGH_MEMORY )
		panic( "trying to free nonexistent page" );
	
	addr -= LOW_MEM;
	addr >>= 12;		// �õ�ҳ���

	if ( mem_map[ addr ]-- )
		return;

	mem_map[ addr ] = 0;	// ���˴�����,�������ú��ͷŲ��Գ�,��ʾ������Ϣ,����

	panic( "trying to free free page" );
}

/*
 * This function frees a continuos block of page tables, as needed
 * by 'exit()'. As does copy_page_tables(), this handles only 4Mb blocks.
 */
LONG free_page_tables( ULONG from, ULONG size )
/*++

Routine Description:

	free_page_tables.�ͷ���4MΪ��λ�������ڴ��.

Arguments:

	from - ��ʼ�������ַ
	size - �ڴ���BYTE����

Return Value:

	0 �ɹ�,ʧ����panic
	
--*/

{
	ULONG * pg_table;
	ULONG * dir, nr;

	if ( from & 0x3fffff )	// 4M�߽�
		panic( "free_page_tables called with wrong alignment" );
	if ( !from )
		panic( "Trying to free up swapper memory space" );

	// 
	// ���� 4M ������,Ҳ����ҳĿ¼�����. ����:
	// ( 1        - 0x3fffff ) -> 1
	// ( 0x400000 - 0x7fffff ) -> 2
	// 
	// һ��ҳĿ¼���������ù���4M�ڴ�.Ŀ¼��ָ��1024ҳ��,ÿ��ҳ����ָ��һ��ҳ��4K,����һ��ҳĿ¼�����4M�ڴ�
	// 
	// PDE -> PT0 -> 4K		
	//		  PT1 -> 4K	
	//		  PT2 -> 4K
	//	      ...
	//		  PT1023
	//
	
	size = ( size + 0x3fffff ) >> 22;

	//
	// ����һ�������ʼĿ¼��.��Ӧ��Ŀ¼���=from>>22,��ÿ��ռ4 �ֽ�,��������ҳĿ¼�Ǵ�
	// �����ַ0 ��ʼ,���ʵ�ʵ�Ŀ¼��ָ��=Ŀ¼���<<2,Ҳ��( from>>20 ).����0xffc ȷ��
	// Ŀ¼��ָ�뷶Χ��Ч.
	//

	//
	// �����dir����ҳĿ¼�� , *dir ����ҳĿ¼��
	// 
	// size �� *dir ��ȫ��һ������Ķ���
	//
	dir = ( ULONG * )( ( from >> 20 ) & 0xffc );

	for ( ; size-- > 0; dir++ )
	{
		if ( !( 1 & *dir ) )
		{
			continue;	//PDE��PλΪ0
		}

		//
		//  pg_table ����ҳ��
		// *pg_table ����ҳ����
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
	����ҳ��.ע�ⲻ�Ǹ����ڴ�����,���ǹ����������ݡ�
	���Ƶ��� PDE-PTE �ȶ�Ӧ����������.�µ�ҳ����Ϊֻ������.�޸��µ�ҳ�津��дʱ���ƻ���

Arguments:

	from	- Դ��ַ
	to		- Ŀ���ַ
	size	- ��С

Return Value:

	0 �ɹ�,ʧ����panic

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
	// ���� 4M ������,Ҳ����ҳĿ¼�����. ����:
	// ( 1        - 0x3fffff ) -> 1
	// ( 0x400000 - 0x7fffff ) -> 2
	// 
	// һ��ҳĿ¼���������ù���4M�ڴ�.Ŀ¼��ָ��1024ҳ��,ÿ��ҳ����ָ��һ��ҳ��4K,����һ��ҳĿ¼�����4M�ڴ�
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

		// ȡ��ǰԴĿ¼����ҳ��ĵ�ַ -> from_page_table
		from_page_table = ( ULONG * )( 0xfffff000 & *from_dir );

		// �������ҳ��, �����Ǵ洢 'Ŀ��ҳ��'
		if ( !( to_page_table = ( ULONG * )get_free_page() ) )
		{
			return -1;
		}

		// ����Ŀ��Ŀ¼����Ϣ.7 �Ǳ�־��Ϣ,��ʾ( Usr, R/W, Present ).
		*to_dir = ( ( ULONG )to_page_table ) | 7;

		// ��Ե�ǰ�����ҳ��,�����踴�Ƶ�ҳ����.��������ں˿ռ�,����踴��ͷ 160 ҳ,
		// ������Ҫ���� 1 ��ҳ���е����� 1024 ҳ��.
		nr = ( from == 0 ) ? 160 : 1024;

		// ���ڵ�ǰҳ��,��ʼ����ָ����Ŀ nr ���ڴ�ҳ��
		for ( ; nr-- > 0; from_page_table++, to_page_table++ )
		{
			this_page = *from_page_table;

			if ( !( 1 & this_page ) )
			{
				continue;
			}

			// ��λҳ������R/W ��־(��0).(���U/S λ��0,��R/W ��û������.���U/S ��1,��R/W ��0,
			// ��ô�������û���Ĵ����ֻ�ܶ�ҳ��.���U/S ��R/W ����λ,�����д��Ȩ��.)

			this_page     &= ~2;
			*to_page_table = this_page;	// ����ҳ����Ƶ�Ŀ��ҳ����.

			// 1M ����,��Ҫ���mem_map[]ҳ���
			// ������Ϊ������ҳ��ӳ��������Ӧ�����������ô���.
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

	������ҳ��ҵ�ָ�������ַ��

Arguments:

	page	- �����ַ
	address	- �����ַ

Return Value:

	address - �ɹ� ���������ַ
	0		- ʧ��

--*/

{
	ULONG tmp, *page_table;

	/* NOTE !!! This uses the fact that _pg_dir=0 */

	if ( page < LOW_MEM || page >= HIGH_MEMORY )
		printk( "Trying to put page %p at %p\n", page, address );

	// ӳ���ֽ�ͼ��û����λ,�˴�Ӧ��panic
	if ( mem_map[ ( page - LOW_MEM ) >> 12 ] != 1 )
		printk( "mem_map disagrees with %p at %p\n", page, address );

	// page_tableҳĿ¼�� , *page_table ҳĿ¼����
	page_table = ( ULONG * )( ( address >> 20 ) & 0xffc );

	if ( ( *page_table ) & 1 )
	{
		// ҳĿ¼����( P=1 ) ֱ����ȡҳ��
		page_table = ( ULONG * )( 0xfffff000 & *page_table );
	}
	else 
	{
		// ����,�������ҳ���ҳ��ʹ��,��Ӧ��־7( User, U/S, R/W ).
		if ( !( tmp = get_free_page() ) )
		{
			return 0;
		}
		
		*page_table = tmp | 7;

		//ҳ�������ҳĿ¼����
		page_table = ( ULONG * )tmp;
	}

	//ҳ�����ҳ������
	page_table[ ( address >> 12 ) & 0x3ff ] = page | 7;
	
	return page;
}

VOID un_wp_page( ULONG * table_entry )
/*++

Routine Description:

	ȡ��ҳ���д����,����ҳ�쳣�жϹ�����д�����쳣�Ĵ���(дʱ����)

Arguments:

	table_entry - ҳ����

Return Value:

	VOID

--*/
{
	ULONG old_page, new_page;

	// old_page - ʵ������ҳ��ҳ���еĺ���
	old_page = 0xfffff000 & *table_entry;

	//
	// �������1M,��map�е�ֵΪ1,˵��������һ��.
	// ֱ�Ӹ����Ծ���
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
	// �������1M,��map�е�ֵ����1,˵�������ö��.
	// ��ʱ������һ���µ�ҳ��,���ú�������(U/S, R/W, P => 7 )
	// ������ҳ��,����ҳ������
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

	do_wp_page ҳ�쳣������, page.s�����е��øú���.

Arguments:

	error_code	- cpu����
	address		- cpu�ṩ,cr2��ֵ,��ʱ�������ַ

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

	write_verify - ��֤�����ַ��д

Arguments:

	address - �����ַ

Return Value:

	ҳ����Ч����NULL;���ҳ�治��д,����ҳ��

--*/
{
	ULONG page;

	// У��'ҳĿ¼��'��'P'λ��.
	
	if ( !( ( page = *( ( ULONG * )( ( address >> 20 ) & 0xffc ) ) ) & 1 ) )
		return;

	// ȡҳ��ĵ�ַ,����ָ����ַ��ҳ����ҳ���е�ҳ����ƫ��ֵ,�ö�Ӧ����ҳ���ҳ����ָ��.
	page &= 0xfffff000;
	page += ( ( address >> 10 ) & 0xffc );

	// �����ҳ�治��д( ��־R/W û����λ ),��ִ�й������͸���ҳ�����( дʱ���� ).

	if ( ( 3 & *( ULONG * )page ) == 1 )  /* non-writeable, present */
		un_wp_page( ( ULONG * )page );
	return;
}

VOID get_empty_page( ULONG address )
/*++

Routine Description:

	�������ַӳ��һ���µ�����ҳ��, get_free_page �� put_page �����

Arguments:

	address - �����ַ

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

	try_to_share.����ָ������+��ַҳ��Ĺ������.

	��������Ϣ������ǰ����,������panic

	�ú������� current != p , �Ҿ�����ͬ��ִ����

Arguments:

	address - p�����ڵ������ַ
	p		- ָ���Ľ���

Return Value:

	1 - �ɹ�
	0 - ʧ��
--*/
{
	ULONG	from;			// ҳ����
	ULONG	to;				// ҳ����
	ULONG	from_page;		// ҳĿ¼��
	ULONG	to_page;		// ҳĿ¼��
	ULONG	phys_addr;

	// ��ָ���ڴ��ַ��ҳĿ¼��
	from_page = to_page = ( ( address >> 20 ) & 0xffc );

	// p���̵� ��ʼ�������ڵ�ҳĿ¼��
	from_page += ( ( p->start_code >> 20 ) & 0xffc );

	// ��ǰ���̵� ��ʼ�������ڵ�ҳĿ¼��
	to_page += ( ( current->start_code >> 20 ) & 0xffc );

	from = *( ULONG * )from_page;
	
	if ( !( from & 1 ) )
	{
		// from��ҳĿ¼��PλΪ0
		return 0;
	}

	from &= 0xfffff000;
	
	//��仰�൱��ָ�����,from ��Address����ҳ�����ַ,address >> 10=> (address >> 12) << 4 => Ϊƫ��ֵ
	from_page	= from + ( ( address >> 10 ) & 0xffc );  
	phys_addr	= *( ULONG * )from_page;

	// 0x41 ��Ӧҳ�����е� Dirty �� Present ��־.���ҳ�治�ɾ�����Ч�򷵻�.
	if ( ( phys_addr & 0x41 ) != 0x01 )
	{
		return 0;
	}
	// ȡҳ��ĵ�ַ -> phys_addr.�����ҳ���ַ�����ڻ�С���ڴ�Ͷ�( 1M )Ҳ�����˳�.
	phys_addr &= 0xfffff000;

	if ( phys_addr >= HIGH_MEMORY || phys_addr < LOW_MEM )
	{
		return 0;
	}

	// �Ե�ǰ����ҳ����в���.
	// ȡҳĿ¼������ -> to.�����Ŀ¼����Ч( P=0 ),��ȡ����ҳ��,������to_page ��ָ��Ŀ¼��.

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
	// ȡ��Ӧҳ���ַ -> to,ҳ�����ַ to_page.�����Ӧ��ҳ���Ѿ�����,�����,����.
	to &= 0xfffff000;
	to_page = to + ( ( address >> 10 ) & 0xffc );
	if ( 1 & *( ULONG * )to_page )
	{
		panic( "try_to_share: to_page already exists" );
	}
	/* share them: write-protect */

	// �����ǽ��й�����:д���� 
	// �� p ������ҳ����д������־( ��R/W=0 ֻ�� ).���ҵ�ǰ�����еĶ�Ӧҳ����ָ����.

	*( ULONG * )from_page &= ~2;
	*( ULONG * )to_page    = *( ULONG * )from_page;
	invalidate();
	phys_addr -= LOW_MEM;
	phys_addr >>= 12;

	// ����������ҳ���ҳ���,������Ӧҳ��ӳ���������е����õ���1

	mem_map[phys_addr]++;
	return 1;
}

static LONG share_page( ULONG address )
/*++

Routine Description:

	share_page.��ͼ�ҵ�һ������,�������뵱ǰ���̹���ҳ��.
	���������б�,�Ծ�����ͬ�Ŀ�ִ���ļ��Ľ��̽��пռ乲��

Arguments:

	address - ��ǰ���̵������ַ

Return Value:

	1 - �ɹ�
	0 - ʧ��

--*/
{
	Task_Struct ** p;

	if ( !current->executable )
	{
		return 0;
	}

	// ���ֻ�ܵ���ִ��( executable->i_count=1 )�˳�
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

		if ( try_to_share( address, *p ) )	// ���Թ���ҳ��
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

	ȱҳ�жϴ�������.�� page.s �����б�����.
	����error_code ����CPU �Զ�����,address ��ҳ�����Ե�ַ

Arguments:

	error_code	- cpu����
	address		- cpu�ṩ,cr2��ֵ,��ʱ�������ַ

Return Value:

	VOID

--*/
{
	LONG	nr[4];
	ULONG	tmp;
	ULONG	page;
	LONG	block, i;

	address &= 0xfffff000;	
	
	tmp = address - current->start_code;//�����ַ�����̻���ַ��ƫ��

	//
	// ��� ��ǰ���̵�executable��,����ָ����ַ��������+���ݳ���,������һҳ�����ڴ�,��ӳ��ӳ�䵽ָ���������ַ��.
	// 1.executable �ǽ��̵�i �ڵ�ṹ.��ֵΪ0,�������̸տ�ʼ����,��Ҫ�ڴ�
	// 2.ָ�������Ե�ַ������������ݳ���,���������������µ��ڴ�ռ�,Ҳ��Ҫ����.
	// 
	// ��˾�ֱ�ӵ��� get_empty_page() ����,����һҳ�����ڴ沢ӳ�䵽ָ�����Ե�ַ������.
	// start_code�ǽ��̴���ε�ַ,end_data �Ǵ�������ݳ���.���� linux �ں�,���Ĵ���κ�
	// ���ݶ�����ʼ��ַ����ͬ��.
	//
	if ( !current->executable || tmp >= current->end_data )
	{
		get_empty_page( address );
		return;
	}

	// ������Թ���ҳ��ɹ�,���˳�
	if ( share_page( tmp ) )
	{
		return;
	}

	// ȡ����ҳ��,����ڴ治����,����ʾ�ڴ治��,��ֹ����
	if ( !( page = get_free_page() ) )
	{
		oom();
	}

	/* remember that 1 block is used for header */
	/* ��ס,( ���� )ͷҪʹ��1�����ݿ� */
	// ���ȼ���ȱҳ���ڵ����ݿ���.BLOCK_SIZE = 1024 �ֽ�,���һҳ�ڴ���Ҫ4 �����ݿ�.
	block = 1 + tmp / BLOCK_SIZE;

	for ( i = 0 ; i<4 ; block++, i++ )
	{
		nr[ i ] = bmap( current->executable, block );
	}

	// ���豸��һ��ҳ�������( 4 ���߼��� )��ָ�������ַpage ��
	bread_page( page, current->executable->i_dev, nr );

	// ��������һҳ�ڴ��,��ҳ�ڴ�Ĳ��ֿ��ܻᳬ�����̵�end_data λ��.�����ѭ�����Ƕ�����
	// ҳ�泬���Ĳ��ֽ������㴦��.

	i   = tmp  + 4096 - current->end_data;
	tmp = page + 4096;

	while ( i-- > 0 )
	{
		tmp--;
		*(CHAR *)tmp = 0;
	}

	// ���������ҳ��ӳ�䵽ָ�����Ե�ַ�Ĳ����ɹ�,�ͷ���.������ͷ��ڴ�ҳ,��ʾ�ڴ治��.
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

	mem_init ��ʼ���ڴ����

	����������ҳ����������ڴ���ʼλ��( ��ȥ��RAMDISK ��ռ�ڴ�ռ�� ).
	��ʵ�������ڴ�����ַ��ʼ��.

	�ڸð�� linux �ں���,�����ʹ�� 16Mb ���ڴ�,����16Mb ���ڴ潫���ڿ���,���ò���.

	0 - 1Mb �ڴ�ռ������ں�ϵͳ( ��ʵ��0-640Kb ).

Arguments:

	start_mem	- ��ʼ�����ڴ�
	end_mem		- ��ֹ�����ڴ�

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
	i = MAP_NR( start_mem ); //��ȥϵͳʹ���ڴ������ҳ��

	end_mem -= start_mem;
	end_mem >>= 12;

	while ( end_mem-->0 )
	{
		mem_map[ i++ ] = 0;	//λͼ��ʼ��
	}
}

VOID calc_mem()
/*++

Routine Description:

	calc_mem-�����ڴ����ҳ��������ʾ

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
