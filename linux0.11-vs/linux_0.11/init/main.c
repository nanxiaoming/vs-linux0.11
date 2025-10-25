/*
*  linux/init/main.c
*
*  ( C ) 1991  Linus Torvalds
*/

#define __LIBRARY__
#include <unistd.h>
#include <time.h>
#include <sys\types.h>

/*
 * we need this inline - forking from kernel space will result
 * in NO COPY ON WRITE ( !!! ), until an execve is executed. This
 * is no problem, but for the stack. This is handled by not letting
 * main() use the stack at all after fork(). Thus, no function
 * calls - which means inline code for fork too, as otherwise we
 * would use the stack upon exit from 'fork()'.
 *
 * Actually only pause and fork are needed inline, so that there
 * won't be any messing with the stack from main(), but we define
 * some others too.
 */

/* 
 * 我们需要下面这些内嵌语句- 从内核空间创建进程( forking )将导致没有写时复
 * 制( COPY ON WRITE )!!!直到一个执行execve 调用.这对堆栈可能带来问题.处
 * 理的方法是在fork()调用之后不让main()使用任何堆栈.因此就不能有函数调
 * 用- 这意味着fork 也要使用内嵌的代码,否则我们在从fork()退出时就要使用堆栈了.
 * 
 * 实际上只有pause 和fork 需要使用内嵌方式,以保证从 os_entry() 中不会弄乱堆栈,
 * 但是我们同时还定义了其它一些函数.
 */

/*  
 * 是unistd.h 中的内嵌宏代码.以嵌入汇编的形式调用
 * Linux 的系统调用中断0x80.该中断是所有系统调用的
 * 入口.该条语句实际上是LONG fork()创建进程系统调用.
 * syscall0 名称中最后的0 表示无参数,1 表示1 个参数.
 */

static __inline _syscall0( int , fork )

//
// LONG pause()系统调用:暂停进程的执行,直到 收到一个信号.
//
static __inline _syscall0( int, pause )

//
// LONG setup( VOID * BIOS )系统调用,仅用于
//
static __inline _syscall1( int, setup, VOID *, BIOS )

//
// LONG sync()系统调用:更新文件系统
//
static __inline _syscall0( int, sync )

#include <linux\tty.h>	
#include <linux\sched.h>
#include <linux\head.h>	
#include <asm\system.h>	
#include <asm\io.h>		

#include <stddef.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys\types.h>

#include <linux\fs.h>

static CHAR printbuf[ 1024 ];

extern LONG	vsprintf();
extern VOID init();
extern VOID blk_dev_init();
extern VOID chr_dev_init();
extern VOID hd_init();
extern VOID floppy_init();
extern VOID mem_init( LONG start, LONG end );
extern LONG rd_init( LONG mem_start, LONG length );
extern LONG kernel_mktime( struct tm * tm );
extern LONG startup_time;
/*
 * This is set up by the setup-routine at boot-time
 */

#define EXT_MEM_K		( *( USHORT *				) 0x90002 )	// 1M 以后的扩展内存大小(KB).
#define DRIVE_INFO		( *( struct drive_info *	) 0x90080 )	// 硬盘参数表基址.
#define ORIG_ROOT_DEV	( *( USHORT *				) 0x901FC )	// 根文件系统所在设备号.

/*
 * Yeah, yeah, it's ugly, but I cannot find how to do this correctly
 * and this seems to work. I anybody has more info on the real-time
 * clock I'd be interested. Most of this was trial and error, and some
 * bios-listing reading. Urghh.
 */

// 这段宏读取CMOS 实时时钟信息.
// 0x70 是写端口号,0x80|addr 是要读取的CMOS 内存地址.
// 0x71 是读端口号

static __inline LONG CMOS_Read( UCHAR addr )
{
	outb_p( 0x80 | addr, 0x70 );
	return inb_p( 0x71 );
}

#define BCD_TO_BIN( val ) ( ( val )=( ( val )&15 ) + ( ( val )>>4 )*10 )

static VOID time_init()
{
	struct tm time;

	do {
		time.tm_sec  = CMOS_Read( 0 );
		time.tm_min  = CMOS_Read( 2 );
		time.tm_hour = CMOS_Read( 4 );
		time.tm_mday = CMOS_Read( 7 );
		time.tm_mon  = CMOS_Read( 8 );
		time.tm_year = CMOS_Read( 9 );
	} while ( time.tm_sec != CMOS_Read( 0 ) );

	BCD_TO_BIN( time.tm_sec	 );
	BCD_TO_BIN( time.tm_min	 );
	BCD_TO_BIN( time.tm_hour );
	BCD_TO_BIN( time.tm_mday );
	BCD_TO_BIN( time.tm_mon	 );
	BCD_TO_BIN( time.tm_year );

	time.tm_mon--;
	startup_time = kernel_mktime( &time );
}

static LONG memory_end			= 0;				// 机器具有的内存( 字节数 )
static LONG buffer_memory_end	= 0;				// 高速缓冲区末端地址
static LONG main_memory_start	= 0;				// 主内存( 将用于分页 )开始的位置

struct drive_info { CHAR dummy[ 32 ]; } drive_info;	// 用于存放硬盘参数表信息.

VOID os_entry()
{

	// 根设备号	-> ROOT_DEV		高速缓存末端地址 -> buffer_memory_end 
	// 机器内存数	-> memory_end	主内存开始地址   -> main_memory_start 

	ROOT_DEV = ORIG_ROOT_DEV;
	__asm	cld
	drive_info = DRIVE_INFO;
	memory_end  = ( 1 << 20 ) + ( EXT_MEM_K << 10 );// 内存大小=1Mb 字节+扩展内存( k )*1024 字节.
	memory_end &= 0xfffff000;						// 忽略不到4Kb( 1 页 )的内存数.
	if ( memory_end > 16 * 1024 * 1024 )			// 如果内存超过16Mb,则按16Mb 计.
		memory_end = 16 * 1024 * 1024;
	if ( memory_end > 12 * 1024 * 1024 )			// 如果内存>12Mb,则设置缓冲区末端=4Mb
		buffer_memory_end = 4 * 1024 * 1024;
	else if ( memory_end > 6 * 1024 * 1024 )		// 否则如果内存>6Mb,则设置缓冲区末端=2Mb
		buffer_memory_end = 2 * 1024 * 1024;
	else
		buffer_memory_end = 1 * 1024 * 1024;		// 否则则设置缓冲区末端=1Mb
	main_memory_start = buffer_memory_end;
#ifdef RAMDISK										// 如果定义了虚拟盘,则主内存将减少.
	main_memory_start += rd_init( main_memory_start, RAMDISK * 1024 );
#endif
	mem_init( main_memory_start, memory_end );
	trap_init();
	blk_dev_init();
	chr_dev_init();
	tty_init();
	time_init();
	sched_init();
	buffer_init( buffer_memory_end );
	hd_init();			
	floppy_init();

	sti();			

	move_to_user_mode();

	if ( !fork() ) 
	{							
		init();
	}

	/*
	 * NOTE!! For any other task 'pause()' would mean we have to get a
	 * signal to awaken, but task0 is the sole exception ( see 'schedule()' )
	 * as task 0 gets activated at every idle moment ( when no other tasks
	 * can run ). For task0 'pause()' just means we go check if some other
	 * task can run, and if not we return here.
	 */

	/* 
	 *  以下代码是任务0,实际上就是Idle进程的工作,参与调度.
	 *  
	 *  注意!! 对于任何其它的任务,'pause()'将意味着我们必须等待收到一个信号才会返
	 *  回就绪运行态,但任务0( task0 )是唯一例外情况( 参见'schedule()' ),
	 *  因为任务 0 在任何空闲时间里都会被激活( 当没有其它任务在运行时 ),
	 *  因此对于任务0'pause()'仅意味着我们返回来查看是否有其它任务可以运行,如果没
	 *  有的话我们就回到这里,一直循环执行'pause()'.
	 */
	 
	for ( ;; ) pause();
}

static LONG printf( const CHAR *fmt, ... )
/*++

Routine Description:

printf

	产生格式化信息并输出到标准输出设备stdout( 1 ),这里是指屏幕上显示.参数'*fmt'
	指定输出将采用的格式,参见各种标准C 语言书籍.该子程序正好是vsprintf 如何使
	用的一个例子.
	该程序使用vsprintf()将格式化的字符串放入printbuf 缓冲区,然后用write()
	将缓冲区的内容输出到标准设备( 1--stdout ).

Arguments:

	fmt - 格式化样式
	    - 参数列表

Return Value:

	长度值

--*/
{
	va_list		args;
	LONG		i;

	va_start( args, fmt );
	write( 1, printbuf, i = vsprintf( printbuf, fmt, args ) );
	va_end( args );

	return i;
}

static CHAR * argv_rc[] = { "/bin/sh"		, NULL };	// 调用执行程序时参数的字符串数组.
static CHAR * envp_rc[] = { "HOME=/"		, NULL };	// 调用执行程序时的环境字符串数组.

static CHAR * argv[]	= { "-/bin/sh"		, NULL };
static CHAR * envp[]	= { "HOME=/usr/root", NULL };

VOID init()
/*++

Routine Description:

	init - 系统初始化 主要包括以下流程.实际上是任务1,由os_entry的fork后执行

	1.读取硬盘参数包括分区表信息并建立虚拟盘和安装根文件系统设备 , 调用 sys_setup
	2.用读写访问方式打开设备 "/dev/tty0" - 终端控制台,标准输入设备,返回0号句柄
	3.复制句柄,产生1号句柄 -- stdout 标准输出设备.
	4.复制句柄,产生2号句柄 -- stderr 标准出错输出设备.
	5.显示Buffer,内存大小等信息
	6.执行fork,创建任务2
	7.执行子进程(任务2)的停止,执行任务3以及后续任务,永远循环

Arguments:

	VOID

Return Value:

	VOID

--*/
{
	LONG pid, i;

	setup( ( VOID * )&drive_info );

	open( "/dev/tty0", O_RDWR, 0 );	// stdin
									
	dup( 0 );						// stdin
	dup( 0 );						// stderr

	printf( "%d buffers = %d bytes buffer space\n\r", 
			NR_BUFFERS,
			NR_BUFFERS*BLOCK_SIZE );
	
	printf( "Free mem: %d bytes\n\r", memory_end - main_memory_start );

	if ( !( pid = fork() ) )
	{
		close( 0 );								//关闭 "/dev/tty0" - 终端控制台

		if ( open( "/etc/rc", O_RDONLY, 0 ) )   // 打开 /etc/rc 文件
		{
			_exit( 1 );							
		}

		//lb 0x0800b556
		execve( "/bin/sh", argv_rc, envp_rc );	// 装入 /bin/sh 程序并执行

		_exit( 2 );
	}

	/*
	 * 任务1等待任务2终止. &i 是存放返回状态信息的.
	 * 如果wait()返回值不等于子进程号,则继续等待.
	 */
	if ( pid > 0 )
	{
		while ( pid != wait( &i ) )
			;
	}

	/*
	 * 任务2已经终止.
	 * 
	 * 再创建一个任务3,期望成功,否则打印一些信息.
	 * 
	 * 任务3的工作:
	 * 
	 *	1)关闭所有以前还遗留的句柄( stdin, stdout, stderr )
	 *  2)新创建一个会话并设置进程组号
	 *	3)再次打开 /dev/tty0 -> stdin , 并复制该句柄为 stdout stderr
	 *  4)再次执行系统解释程序 /bin/sh , 参数与上次不同
	 *  5)再次等待任务3的结束,并显示信息.
	 * 
	 * 至此永远重复.
	 * 
	 */

	while ( 1 ) 
	{
		if ( ( pid = fork() ) < 0 ) 
		{
			printf( "Fork failed in init\r\n" );
			continue;
		}
		if ( !pid ) 
		{
			close( 0 );
			close( 1 );
			close( 2 );

			setsid();

			open( "/dev/tty0", O_RDWR, 0 );

			dup( 0 );
			dup( 0 );

			_exit( execve( "/bin/sh", argv, envp ) );
		}
		while ( 1 )
		{
			/*
			 * wait 除了等待进程结束,还有一个功能是处理'孤儿'进程.
			 * 是指父进程先结束,其子进程会归为init-任务1的子进程,这种孤儿进程资源释放
			 * 由于init-任务1来完成.
			 */

			if ( pid == wait( &i ) )
			{
				break;
			}
		}
		printf( "\n\rchild %d died with code %04x\n\r", pid, i );
		sync();		//刷新缓冲区
	}
	_exit( 0 );
}
