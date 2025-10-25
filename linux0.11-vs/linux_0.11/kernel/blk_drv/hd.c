/*
*  linux/kernel/hd.c
*
*  ( C ) 1991  Linus Torvalds
*/

/*
* This is the low-level hd interrupt support. It traverses the
* request-list, using interrupts to jump between functions. As
* all the functions are called within interrupts, we may not
* sleep. Special care is recommended.
*
*  modified by Drew Eckhardt to check nr of hd's from the CMOS.
*/

#include <linux\config.h>
#include <linux\sched.h>
#include <linux\fs.h>
#include <linux\kernel.h>
#include <linux\hdreg.h>
#include <asm\system.h>
#include <asm\io.h>
#include <asm\segment.h>

#define MAJOR_NR 3	// 硬盘主设备号是3
#include "blk.h"

/* Max read/write errors/sector */

#define MAX_ERRORS	7			// 读/写一个扇区时允许的最多出错次数
#define MAX_HD		2			// 系统支持的最多硬盘数

static VOID recal_intr();		// 硬盘中断程序在复位操作时会调用的重新校正函数

static LONG hd_recalibrate	= 1;	// 重新校正标志.
static LONG hd_reset		= 1;	// 复位标志.

__inline UCHAR Hd_CMOS_Read( USHORT addr )
{
	UCHAR _v;

	outb_p( 0x80 | addr, 0x70 );

	_v = inb_p( 0x71 );

	return _v;
}

/*
 * This struct defines the HD's and their types.
 * 
 * 下面结构定义了硬盘参数及类型
 * 各字段分别是磁头数、每磁道扇区数、柱面数、写前预补偿柱面号、磁头着陆区柱面号、控制字节.
 * 
 */

struct hd_i_struct
{
	LONG head, sect, cyl, wpcom, lzone, ctl;
};

#ifdef HD_TYPE	// 如果已经在include/linux/config.h 中定义了HD_TYPE

struct hd_i_struct hd_info[] = { HD_TYPE };	// 取定义好的参数作为hd_info[]的数据
#define NR_HD ( ( sizeof ( hd_info ) )/( sizeof ( struct hd_i_struct ) ) )
#else
struct hd_i_struct hd_info[] = { { 0, 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0, 0 } };
static LONG NR_HD = 0;
#endif

// 定义硬盘分区结构.给出每个分区的物理起始扇区号、分区扇区总数.
// 其中 5 的倍数处的项( 例如hd[ 0 ]和hd[ 5 ]等 )代表整个硬盘中的参数
static struct hd_struct 
{
	LONG start_sect;
	LONG nr_sects;
} hd[ 5 * MAX_HD ] = { { 0, 0 }, };

// 读端口 port,共读 nr 字,保存在 buf 中
static __inline VOID port_read( USHORT port, VOID *buf, LONG nr )
{
	__asm	mov		dx, port
	__asm	mov		edi, buf
	__asm	mov		ecx, nr
	__asm	cld
	__asm	rep		insw
}

// 写端口 port,共写 nr 字,从 buf 中取数据
static __inline VOID port_write( USHORT port, VOID *buf, LONG nr )
{
	__asm	mov		dx, port
	__asm	mov		esi, buf
	__asm	mov		ecx, nr
	__asm	cld
	__asm	rep		outsw
}

extern VOID hd_interrupt();
extern VOID rd_load();

LONG sys_setup( CHAR * BIOS )
/*++

Routine Description:

	该函数的参数由初始化程序 init/main.c 的 init 子程序设置为指向0x90080 处,此处存放着
	setup.s 程序从 BIOS 取得的 2 个硬盘的基本参数表( 32 字节 ).
	
	硬盘参数表信息参见下面列表后的说明.
	本函数主要功能是读取 CMOS 和硬盘参数表信息,用于设置硬盘分区结构 hd ,并加载 RAM 虚拟盘和根文件系统
	
Arguments:

	BIOS - bios 参数

Return Value:

	0 - 成功

--*/
{
	static LONG				callable = 1;
	LONG					i, drive;
	UCHAR					cmos_disks;
	struct partition	*	p;
	Buffer_Head			*	bh;

	// 初始化时callable=1,当运行该函数时将其设置为0,使本函数只能执行一次
	if ( !callable )
	{
		return -1;
	}

	callable = 0;

	// 如果没有在config.h 中定义硬盘参数,就从0x90080处读入
#ifndef HD_TYPE

	for ( drive = 0; drive < 2; drive++ ) 
	{
		hd_info[ drive ].cyl	= *( USHORT * )BIOS;			// 柱面数.
		hd_info[ drive ].head	= *( UCHAR	* )( 2  + BIOS );	// 磁头数.
		hd_info[ drive ].wpcom	= *( USHORT * )( 5  + BIOS );	// 写前预补偿柱面号.
		hd_info[ drive ].ctl	= *( UCHAR	* )( 8  + BIOS );	// 控制字节.
		hd_info[ drive ].lzone	= *( USHORT * )( 12 + BIOS );	// 磁头着陆区柱面号.
		hd_info[ drive ].sect	= *( UCHAR	* )( 14 + BIOS );	// 每磁道扇区数.
		BIOS += 16;												// 每个硬盘的参数表长16 字节,这里 BIOS 指向下一个表.
	}

	// setup.s 程序在取 BIOS 中的硬盘参数表信息时,如果只有 1 个硬盘,就会将对应第 2 个硬盘的
	// 16字节全部清零.因此这里只要判断第 2 个硬盘柱面数是否为 0 就可以知道有没有第 2 个硬盘了.

	if ( hd_info[ 1 ].cyl )
	{
		NR_HD = 2;		// 硬盘数置为2
	}
	else
	{
		NR_HD = 1;
	}
#endif

	// 设置每个硬盘的起始扇区号和扇区总数.其中编号 i*5 含义参见本程序后的有关说明

	for ( i = 0; i < NR_HD; i++ ) 
	{
		hd[ i * 5 ].start_sect = 0;				// 硬盘起始扇区号
		hd[ i * 5 ].nr_sects	= hd_info[ i ].head * hd_info[ i ].sect * hd_info[ i ].cyl;
	}

	/*
	 * We querry CMOS about hard disks : it could be that
	 * we have a SCSI/ESDI/etc controller that is BIOS
	 * compatable with ST-506, and thus showing up in our
	 * BIOS table, but not register compatable, and therefore
	 * not present in CMOS.
	 * 
	 * Furthurmore, we will assume that our ST-506 drives
	 * <if any> are the primary drives in the system, and
	 * the ones reflected as drive 1 or 2.
	 * 
	 * The first drive is stored in the high nibble of CMOS
	 * byte 0x12, the second in the low nibble.  This will be
	 * either a 4 bit drive type or 0xf indicating use byte 0x19
	 * for an 8 bit type, drive 1, 0x1a for drive 2 in CMOS.
	 * 
	 * Needless to say, a non-zero value means we have
	 * an AT controller hard disk for that drive.
	 * 
	 */

	/*
	 * 我们对 CMOS 有关硬盘的信息有些怀疑:可能会出现这样的情况,我们有一块SCSI/ESDI/等的
	 * 控制器,它是以 ST-506 方式与 BIOS 兼容的,因而会出现在我们的 BIOS 参数表中,但却又不
	 * 是寄存器兼容的,因此这些参数在 CMOS 中又不存在.
	 * 
	 * 另外,我们假设 ST-506 驱动器( 如果有的话 )是系统中的基本驱动器,也即以驱动器 1 或 2 出现的驱动器.
	 * 
	 * 第 1 个驱动器参数存放在 CMOS 字节 0x12 的高半字节中,第 2 个存放在低半字节中.该 4 位字节
	 * 信息可以是驱动器类型,也可能仅是0xf.0xf 表示使用 CMOS 中 0x19 字节作为驱动器 1 的 8 位
	 * 类型字节,使用 CMOS 中 0x1A 字节作为驱动器 2 的类型字节.
	 * 
	 * 总之,一个非零值意味着我们有一个 AT 控制器硬盘兼容的驱动器.
	 * 
	 */

	 // 这里根据上述原理来检测硬盘到底是否是 AT 控制器兼容的.有关 CMOS 信息请参见4.2.3.1 节.
	if ( ( cmos_disks = Hd_CMOS_Read( 0x12 ) ) & 0xf0 )
	{
		if ( cmos_disks & 0x0f )
		{
			NR_HD = 2;
		}
		else
		{
			NR_HD = 1;
		}
	}
	else
	{
		NR_HD = 0;
	}

	// 若 NR_HD = 0 , 则两个硬盘都不是 AT 控制器兼容的,硬盘数据结构清零.
	// 若 NR_HD = 1 , 则将第 2 个硬盘的参数清零.

	for ( i = NR_HD; i < 2; i++ ) 
	{
		hd[ i * 5 ].start_sect = 0;
		hd[ i * 5 ].nr_sects   = 0;
	}

	// 读取每一个硬盘上第1 块数据( 第1 个扇区有用 ),获取其中的分区表信息.
	// 首先利用函数bread()读硬盘第1 块数据( fs/buffer.c,267 ),参数中的0x300 是硬盘的主设备号
	// ( 参见列表后的说明 ).然后根据硬盘头1 个扇区位置0x1fe 处的两个字节是否为'55AA'来判断
	// 该扇区中位于0x1BE 开始的分区表是否有效.最后将分区表信息放入硬盘分区数据结构hd 中.

	for ( drive = 0; drive < NR_HD; drive++ ) 
	{
		// 0x300, 0x305 逻辑设备号
		if ( !( bh = bread( 0x300 + drive * 5, 0 ) ) ) 
		{
			printk( "Unable to read partition table of drive %d\n\r",drive );
			panic( "" );
		}

		// 判断硬盘信息有效标志'55AA'
		if ( bh->b_data[ 510 ] != 0x55 || ( UCHAR )
			 bh->b_data[ 511 ] != 0xAA )
		{
			printk( "Bad partition table on drive %d\n\r", drive );
			panic( "" );
		}

		p = ( struct partition* )( 0x1BE + ( CHAR* )bh->b_data );// 分区表位于硬盘第1 扇区的 0x1BE 处

		for ( i = 1; i<5; i++, p++ ) 
		{
			hd[ i + 5 * drive ].start_sect	= p->start_sect;
			hd[ i + 5 * drive ].nr_sects	= p->nr_sects;
		}
		brelse( bh );		// 释放为存放硬盘块而申请的内存缓冲区页
	}

	if ( NR_HD )
	{
		printk( "Partition table%s ok.\n\r", ( NR_HD>1 ) ? "s" : "" );
	}

	rd_load();			// 加载( 创建 )RAMDISK( kernel/blk_drv/ramdisk.c )

	mount_root();		// 安装根文件系统( fs/super.c ).

	return ( 0 );
}

// 判断并循环等待驱动器就绪.
// 读硬盘控制器状态寄存器端口HD_STATUS( 0x1f7 ),并循环检测驱动器就绪比特位和控制器忙位.

static LONG controller_ready()
{
	LONG retries = 10000;

	while ( --retries && ( inb_p( HD_STATUS ) & 0xc0 ) != 0x40 );

	return ( retries );	// 返回等待循环的次数
}

// 检测硬盘执行命令后的状态.( win_表示温切斯特硬盘的缩写 )
// 读取状态寄存器中的命令执行结果状态.返回0 表示正常,1 出错.如果执行命令错,
// 则再读错误寄存器HD_ERROR( 0x1f1 )
static LONG win_result()
{
	LONG i = inb_p( HD_STATUS );	// 取状态信息

	if ( ( i & ( BUSY_STAT | READY_STAT | WRERR_STAT | SEEK_STAT | ERR_STAT ) ) == ( READY_STAT | SEEK_STAT ) )
	{
		return 0; /* ok */
	}

	if ( i & 1 ) 
	{
		i = inb( HD_ERROR );	// 若ERR_STAT 置位,则读取错误寄存器
	}

	return 1;
}

// 向硬盘控制器发送命令块( 参见列表后的说明 ).
// 调用参数:drive - 硬盘号( 0-1 ); nsect - 读写扇区数;
// sect - 起始扇区; head - 磁头号;
// cyl - 柱面号; cmd - 命令码;
// *intr_addr() - 硬盘中断处理程序中将调用的C 处理函数
static VOID hd_out( 
	ULONG			drive, 
	ULONG			nsect, 
	ULONG			sect,
	ULONG			head, 
	ULONG			cyl, 
	ULONG			cmd,
	VOID( *intr_addr )() )
{
	register LONG port;

	if ( drive > 1 || head > 15 )	// 如果驱动器号( 0,1 )>1 或磁头号>15,则程序不支持
	{
		panic( "Trying to write bad sector" );
	}

	if ( !controller_ready() )	// 如果等待一段时间后仍未就绪则出错,死机
	{
		panic( "HD controller not ready" );
	}

	do_hd = intr_addr;	// do_hd 函数指针将在硬盘中断程序中被调用

	outb_p( (UCHAR)hd_info[ drive ].ctl, HD_CMD );	// 向控制寄存器( 0x3f6 )输出控制字节

	port = HD_DATA;// 置dx 为数据寄存器端口( 0x1f0 )

	outb_p( (UCHAR)(hd_info[ drive ].wpcom >> 2	 )	, (USHORT)(++port) );		// 参数:写预补偿柱面号( 需除4 )
	outb_p( (UCHAR)(nsect						 )	, (USHORT)(++port) );		// 参数:读/写扇区总数.
	outb_p( (UCHAR)(sect						 )	, (USHORT)(++port) );		// 参数:起始扇区.
	outb_p( (UCHAR)(cyl							 )	, (USHORT)(++port) );		// 参数:柱面号低8 位.
	outb_p( (UCHAR)(cyl >> 8					 )	, (USHORT)(++port) );		// 参数:柱面号高8 位.
	outb_p( (UCHAR)(0xA0 | ( drive << 4 ) | head )	, (USHORT)(++port) );		// 参数:驱动器号+磁头号
	outb  ( (UCHAR)(cmd							 )	, (USHORT)(++port) );		// 命令:硬盘控制命令
}

// 等待硬盘就绪.也即循环等待主状态控制器忙标志位复位.若仅有就绪或寻道结束标志
// 置位,则成功,返回0.若经过一段时间仍为忙,则返回1.
static LONG drive_busy()
{
	ULONG i;

	for ( i = 0; i < 10000; i++ ) // 循环等待就绪标志位置位
	{
		if ( READY_STAT == ( inb_p( HD_STATUS ) & ( BUSY_STAT | READY_STAT ) ) )
		{
			break;
		}
	}

	i  = inb( HD_STATUS );					// 再取主控制器状态字节
	i &= BUSY_STAT | READY_STAT | SEEK_STAT;// 检测忙位、就绪位和寻道结束位

	if ( i == ( READY_STAT | SEEK_STAT ) )	// 若仅有就绪或寻道结束标志,则返回0
	{
		return 0;
	}

	printk( "HD controller times out\n\r" );// 否则等待超时,显示信息.并返回1

	return 1;
}

// 诊断复位( 重新校正 )硬盘控制器
static VOID reset_controller()
{
	LONG	i;

	outb( 4, HD_CMD );	// 向控制寄存器端口发送控制字节( 4-复位 )

	for ( i = 0; i < 100; i++ ) 
	{
		nop();
	}

	outb( hd_info[ 0 ].ctl & 0x0f, HD_CMD ); // 再发送正常的控制字节( 不禁止重试、重读 )

	if ( drive_busy() ) // 若等待硬盘就绪超时,则显示出错信息
	{
		printk( "HD-controller still busy\n\r" );
	}

	if ( ( i = inb( HD_ERROR ) ) != 1 ) // 取错误寄存器,若不等于1( 无错误 )则出错
	{
		printk( "HD-controller reset failed: %02x\n\r", i );
	}
}

// 复位硬盘nr.首先复位( 重新校正 )硬盘控制器.然后发送硬盘控制器命令"建立驱动器参数",
// 其中recal_intr()是在硬盘中断处理程序中调用的重新校正处理函数
static VOID reset_hd( LONG nr )
{
	reset_controller();

	hd_out( nr,
			hd_info[ nr ].sect,
			hd_info[ nr ].sect, 
			hd_info[ nr ].head - 1,
			hd_info[ nr ].cyl, 
			WIN_SPECIFY, 
			&recal_intr );
}

// 意外硬盘中断调用函数.
// 发生意外硬盘中断时,硬盘中断处理程序中调用的默认C 处理函数.在被调用函数指针为空时
// 调用该函数.参见( kernel/system_call.s,241 行 )
VOID unexpected_hd_interrupt()
{
	printk( "Unexpected HD interrupt\n\r" );
}

// 读写硬盘失败处理调用函数
static VOID bad_rw_intr()
{
	if ( ++CURRENT->errors >= MAX_ERRORS )
	{
		end_request( 0 );
	}
	// 对应缓冲区更新标志复位( 没有更新 )
	if ( CURRENT->errors > MAX_ERRORS / 2 )
	{
		hd_reset = 1;
	}
}

// 读操作中断调用函数.将在执行硬盘中断处理程序中被调用
static VOID read_intr()
{
	if ( win_result() )	// 若控制器忙、读写错或命令执行错
	{ 
		bad_rw_intr();	// 读写硬盘失败处理
		do_hd_request();// 然后再次请求硬盘作相应( 复位 )处理

		return;
	}

	port_read( HD_DATA, CURRENT->buffer, 256 );	// 将数据从数据寄存器口读到请求结构缓冲区

	CURRENT->errors		= 0;	// 清出错次数.
	CURRENT->buffer    += 512;	// 调整缓冲区指针,指向新的空区.
	CURRENT->sector++;			// 起始扇区号加1,

	if ( --CURRENT->nr_sectors ) 
	{
		do_hd = &read_intr;		// 再次置硬盘调用C 函数指针为read_intr()
		return;
	}

	end_request( 1 );	// 若全部扇区数据已经读完,则处理请求结束事宜

	do_hd_request();// 执行其它硬盘请求操作.
}

// 写扇区中断调用函数.在硬盘中断处理程序中被调用.
// 在写命令执行后,会产生硬盘中断信号,执行硬盘中断处理程序,此时在硬盘中断处理程序中调用的
// C 函数指针do_hd()已经指向write_intr(),因此会在写操作完成( 或出错 )后,执行该函数.

static VOID write_intr()
{
	if ( win_result() )
	{
		bad_rw_intr();	 //硬盘读写失败处理
		do_hd_request(); //请求硬盘作相应( 复位 )处理
		return;
	}

	if ( --CURRENT->nr_sectors )
	{
		CURRENT->sector++;
		CURRENT->buffer += 512;
		do_hd = &write_intr;						// 置硬盘中断程序调用函数指针为write_intr
		port_write( HD_DATA, CURRENT->buffer, 256 );// 再向数据寄存器端口写256 字节
		return;
	}

	end_request( 1 );	// 若全部扇区数据已经写完,则处理请求结束事宜,
	do_hd_request();	// 执行其它硬盘请求操作.
}

// 硬盘重新校正( 复位 )中断调用函数.在硬盘中断处理程序中被调用.
// 如果硬盘控制器返回错误信息,则首先进行硬盘读写失败处理,然后请求硬盘作相应( 复位 )处理.

static VOID recal_intr()
{
	if ( win_result() )
	{
		bad_rw_intr();
	}
	do_hd_request();
}

// 执行硬盘读写请求操作
VOID do_hd_request()
{
	LONG	i, r;
	ULONG	block, dev;
	ULONG	sec, head, cyl;
	ULONG	nsect;

	INIT_REQUEST;					// 检测请求项的合法性 参见kernel/blk_drv/blk.h

	dev   = MINOR( CURRENT->dev );	// CURRENT 定义为( blk_dev[ MAJOR_NR ].current_request )
	block = CURRENT->sector;		// 请求的起始扇区.

	// 如果子设备号不存在或者起始扇区大于该分区扇区数-2,则结束该请求,并跳转到标号repeat 处
	// ( 定义在INIT_REQUEST 开始处 ).因为一次要求读写2 个扇区( 512*2 字节 ),所以请求的扇区号
	// 不能大于分区中最后倒数第二个扇区号.
	if ( dev >= 5 * ( ULONG )NR_HD || block + 2 > ( ULONG )hd[ dev ].nr_sects ) 
	{
		end_request( 0 );
		goto repeat;
	}

	block += hd[ dev ].start_sect;	// 将所需读的块对应到整个硬盘上的绝对扇区号
	dev   /= 5;						// 此时dev 代表硬盘号( 0 或1 )

	//从硬盘信息结构中根据起始扇区号和每磁道扇区数计算在磁道中的扇区号( sec )、所在柱面号( cyl )和磁头号( head )
		 
	sec		= block % hd_info[ dev ].sect;
	block  /= hd_info[ dev ].sect;
	head	= block % hd_info[ dev ].head;
	cyl		= block / hd_info[ dev ].head;

	sec++;

	nsect	= CURRENT->nr_sectors;	// 欲读/写的扇区数

	// 如果reset 置1,则执行复位操作.复位硬盘和控制器,并置需要重新校正标志,返回
	if ( hd_reset )
	{
		hd_reset		= 0;
		hd_recalibrate	= 1;
		reset_hd( CURRENT_DEV );
		return;
	}

	// 如果重新校正标志( recalibrate )置位,则首先复位该标志,然后向硬盘控制器发送重新校正命令.

	if ( hd_recalibrate ) 
	{
		hd_recalibrate = 0;

		hd_out( dev, hd_info[ CURRENT_DEV ].sect, 0, 0, 0,WIN_RESTORE, &recal_intr );
			
		return;
	}

	// 如果当前请求是写扇区操作,则发送写命令,循环读取状态寄存器信息并判断请求服务标志
	// DRQ_STAT 是否置位.DRQ_STAT 是硬盘状态寄存器的请求服务位( include/linux/hdreg.h,27 ).

	if ( CURRENT->cmd == WRITE ) 
	{
		hd_out( dev, nsect, sec, head, cyl, WIN_WRITE, &write_intr );

		for ( i = 0; i < 3000 && !( r = inb_p( HD_STATUS )&DRQ_STAT ); i++ )
			/* nothing */;

		// 如果请求服务位置位则退出循环.若等到循环结束也没有置位,则此次写硬盘操作失败,去处理
		// 下一个硬盘请求.否则向硬盘控制器数据寄存器端口HD_DATA 写入1 个扇区的数据
		if ( !r ) 
		{
			bad_rw_intr();
			goto repeat;
		}

		port_write( HD_DATA, CURRENT->buffer, 256 );
	}
	else if ( CURRENT->cmd == READ )
	{
		hd_out( dev, nsect, sec, head, cyl, WIN_READ, &read_intr );
	}
	else
	{
		panic( "unknown hd-command" );
	}	
}

//硬盘系统初始化
VOID hd_init()
{
	blk_dev[ MAJOR_NR ].request_fn = DEVICE_REQUEST;

	set_intr_gate( 0x2E, &hd_interrupt );		// 设置硬盘中断门向量 LONG 0x2E( 46 )

	outb_p	( inb_p( 0x21 ) & 0xfb, 0x21 );		// 复位接联的主8259A int2 的屏蔽位,允许从片发出中断请求信号
	outb	( inb_p( 0xA1 ) & 0xbf, 0xA1 );		// 复位硬盘的中断请求屏蔽位( 在从片上 ),允许硬盘控制器发送中断请求信号
}
