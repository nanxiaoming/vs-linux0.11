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
 * ������Ҫ������Щ��Ƕ���- ���ں˿ռ䴴������( forking )������û��дʱ��
 * ��( COPY ON WRITE )!!!ֱ��һ��ִ��execve ����.��Զ�ջ���ܴ�������.��
 * ��ķ�������fork()����֮����main()ʹ���κζ�ջ.��˾Ͳ����к�����
 * ��- ����ζ��fork ҲҪʹ����Ƕ�Ĵ���,���������ڴ�fork()�˳�ʱ��Ҫʹ�ö�ջ��.
 * 
 * ʵ����ֻ��pause ��fork ��Ҫʹ����Ƕ��ʽ,�Ա�֤�� os_entry() �в���Ū�Ҷ�ջ,
 * ��������ͬʱ������������һЩ����.
 */

/*  
 * ��unistd.h �е���Ƕ�����.��Ƕ�������ʽ����
 * Linux ��ϵͳ�����ж�0x80.���ж�������ϵͳ���õ�
 * ���.�������ʵ������LONG fork()��������ϵͳ����.
 * syscall0 ����������0 ��ʾ�޲���,1 ��ʾ1 ������.
 */

static __inline _syscall0( int , fork )

//
// LONG pause()ϵͳ����:��ͣ���̵�ִ��,ֱ�� �յ�һ���ź�.
//
static __inline _syscall0( int, pause )

//
// LONG setup( VOID * BIOS )ϵͳ����,������
//
static __inline _syscall1( int, setup, VOID *, BIOS )

//
// LONG sync()ϵͳ����:�����ļ�ϵͳ
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

#define EXT_MEM_K		( *( USHORT *				) 0x90002 )	// 1M �Ժ����չ�ڴ��С(KB).
#define DRIVE_INFO		( *( struct drive_info *	) 0x90080 )	// Ӳ�̲������ַ.
#define ORIG_ROOT_DEV	( *( USHORT *				) 0x901FC )	// ���ļ�ϵͳ�����豸��.

/*
 * Yeah, yeah, it's ugly, but I cannot find how to do this correctly
 * and this seems to work. I anybody has more info on the real-time
 * clock I'd be interested. Most of this was trial and error, and some
 * bios-listing reading. Urghh.
 */

// ��κ��ȡCMOS ʵʱʱ����Ϣ.
// 0x70 ��д�˿ں�,0x80|addr ��Ҫ��ȡ��CMOS �ڴ��ַ.
// 0x71 �Ƕ��˿ں�

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

static LONG memory_end			= 0;				// �������е��ڴ�( �ֽ��� )
static LONG buffer_memory_end	= 0;				// ���ٻ�����ĩ�˵�ַ
static LONG main_memory_start	= 0;				// ���ڴ�( �����ڷ�ҳ )��ʼ��λ��

struct drive_info { CHAR dummy[ 32 ]; } drive_info;	// ���ڴ��Ӳ�̲�������Ϣ.

VOID os_entry()
{

	// ���豸��	-> ROOT_DEV		���ٻ���ĩ�˵�ַ -> buffer_memory_end 
	// �����ڴ���	-> memory_end	���ڴ濪ʼ��ַ   -> main_memory_start 

	ROOT_DEV = ORIG_ROOT_DEV;
	__asm	cld
	drive_info = DRIVE_INFO;
	memory_end  = ( 1 << 20 ) + ( EXT_MEM_K << 10 );// �ڴ��С=1Mb �ֽ�+��չ�ڴ�( k )*1024 �ֽ�.
	memory_end &= 0xfffff000;						// ���Բ���4Kb( 1 ҳ )���ڴ���.
	if ( memory_end > 16 * 1024 * 1024 )			// ����ڴ泬��16Mb,��16Mb ��.
		memory_end = 16 * 1024 * 1024;
	if ( memory_end > 12 * 1024 * 1024 )			// ����ڴ�>12Mb,�����û�����ĩ��=4Mb
		buffer_memory_end = 4 * 1024 * 1024;
	else if ( memory_end > 6 * 1024 * 1024 )		// ��������ڴ�>6Mb,�����û�����ĩ��=2Mb
		buffer_memory_end = 2 * 1024 * 1024;
	else
		buffer_memory_end = 1 * 1024 * 1024;		// ���������û�����ĩ��=1Mb
	main_memory_start = buffer_memory_end;
#ifdef RAMDISK										// ���������������,�����ڴ潫����.
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
	 *  ���´���������0,ʵ���Ͼ���Idle���̵Ĺ���,�������.
	 *  
	 *  ע��!! �����κ�����������,'pause()'����ζ�����Ǳ���ȴ��յ�һ���źŲŻ᷵
	 *  �ؾ�������̬,������0( task0 )��Ψһ�������( �μ�'schedule()' ),
	 *  ��Ϊ���� 0 ���κο���ʱ���ﶼ�ᱻ����( ��û����������������ʱ ),
	 *  ��˶�������0'pause()'����ζ�����Ƿ������鿴�Ƿ������������������,���û
	 *  �еĻ����Ǿͻص�����,һֱѭ��ִ��'pause()'.
	 */
	 
	for ( ;; ) pause();
}

static LONG printf( const CHAR *fmt, ... )
/*++

Routine Description:

printf

	������ʽ����Ϣ���������׼����豸stdout( 1 ),������ָ��Ļ����ʾ.����'*fmt'
	ָ����������õĸ�ʽ,�μ����ֱ�׼C �����鼮.���ӳ���������vsprintf ���ʹ
	�õ�һ������.
	�ó���ʹ��vsprintf()����ʽ�����ַ�������printbuf ������,Ȼ����write()
	���������������������׼�豸( 1--stdout ).

Arguments:

	fmt - ��ʽ����ʽ
	    - �����б�

Return Value:

	����ֵ

--*/
{
	va_list		args;
	LONG		i;

	va_start( args, fmt );
	write( 1, printbuf, i = vsprintf( printbuf, fmt, args ) );
	va_end( args );

	return i;
}

static CHAR * argv_rc[] = { "/bin/sh"		, NULL };	// ����ִ�г���ʱ�������ַ�������.
static CHAR * envp_rc[] = { "HOME=/"		, NULL };	// ����ִ�г���ʱ�Ļ����ַ�������.

static CHAR * argv[]	= { "-/bin/sh"		, NULL };
static CHAR * envp[]	= { "HOME=/usr/root", NULL };

VOID init()
/*++

Routine Description:

	init - ϵͳ��ʼ�� ��Ҫ������������.ʵ����������1,��os_entry��fork��ִ��

	1.��ȡӲ�̲���������������Ϣ�����������̺Ͱ�װ���ļ�ϵͳ�豸 , ���� sys_setup
	2.�ö�д���ʷ�ʽ���豸 "/dev/tty0" - �ն˿���̨,��׼�����豸,����0�ž��
	3.���ƾ��,����1�ž�� -- stdout ��׼����豸.
	4.���ƾ��,����2�ž�� -- stderr ��׼��������豸.
	5.��ʾBuffer,�ڴ��С����Ϣ
	6.ִ��fork,��������2
	7.ִ���ӽ���(����2)��ֹͣ,ִ������3�Լ���������,��Զѭ��

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
		close( 0 );								//�ر� "/dev/tty0" - �ն˿���̨

		if ( open( "/etc/rc", O_RDONLY, 0 ) )   // �� /etc/rc �ļ�
		{
			_exit( 1 );							
		}

		//lb 0x0800b556
		execve( "/bin/sh", argv_rc, envp_rc );	// װ�� /bin/sh ����ִ��

		_exit( 2 );
	}

	/*
	 * ����1�ȴ�����2��ֹ. &i �Ǵ�ŷ���״̬��Ϣ��.
	 * ���wait()����ֵ�������ӽ��̺�,������ȴ�.
	 */
	if ( pid > 0 )
	{
		while ( pid != wait( &i ) )
			;
	}

	/*
	 * ����2�Ѿ���ֹ.
	 * 
	 * �ٴ���һ������3,�����ɹ�,�����ӡһЩ��Ϣ.
	 * 
	 * ����3�Ĺ���:
	 * 
	 *	1)�ر�������ǰ�������ľ��( stdin, stdout, stderr )
	 *  2)�´���һ���Ự�����ý������
	 *	3)�ٴδ� /dev/tty0 -> stdin , �����Ƹþ��Ϊ stdout stderr
	 *  4)�ٴ�ִ��ϵͳ���ͳ��� /bin/sh , �������ϴβ�ͬ
	 *  5)�ٴεȴ�����3�Ľ���,����ʾ��Ϣ.
	 * 
	 * ������Զ�ظ�.
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
			 * wait ���˵ȴ����̽���,����һ�������Ǵ���'�¶�'����.
			 * ��ָ�������Ƚ���,���ӽ��̻��Ϊinit-����1���ӽ���,���ֹ¶�������Դ�ͷ�
			 * ����init-����1�����.
			 */

			if ( pid == wait( &i ) )
			{
				break;
			}
		}
		printf( "\n\rchild %d died with code %04x\n\r", pid, i );
		sync();		//ˢ�»�����
	}
	_exit( 0 );
}
