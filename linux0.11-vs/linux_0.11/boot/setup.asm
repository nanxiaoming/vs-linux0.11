;
; setup.s		(C) 1991 Linus Torvalds
;
; setup.s is responsible for getting the system data from the BIOS,
; and putting them into the appropriate places in system memory.
; both setup.s and system has been loaded by the bootblock.
;
; This code asks the bios for memory/disk/other parameters, and
; puts them in a "safe" place: 0x90000-0x901FF, ie where the
; boot-block used to be. It is then up to the protected mode
; system to read them from there before the area is overwritten
; for buffer-blocks.

	.model	tiny, c
	.686P

OPTION	casemap:none

PUBLIC	WinMainCRTStartup

; NOTE! These had better be the same as in bootsect.s!

; setup.asm负责从BIOS 中获取系统数据,并将这些数据放到系统内存的适当地方.
; 此时setup.s 和system 已经由bootsect 引导块加载到内存中.
; 这段代码询问bios 有关内存/磁盘/其它参数,并将这些参数放到一个
; "安全的"地方:90000-901FF,也即原来bootsect 代码块曾经在
; 的地方,然后在被缓冲块覆盖掉之前由保护模式的system 读取.
; 以下这些参数最好和bootsect.asm 中的相同 

INITSEG		=	9000h	; we move boot here - out of the way
SYSSEG		=	1000h	; system loaded at 0x10000 (65536).
SETUPSEG	=	9020h	; this is the current segment

idt_48		=	ds:[ 0718h ]
gdt_48		=	ds:[ 071Eh ]

; 
;
;   内存地址    长度      意义
; -----------+------+--------------
;   0x90000	 |  2	|   光标位置    
; -----------+------+--------------
;   0x90002	 |  2	|   扩展内存数   
; -----------+------+--------------
;   0x90004	 |  2	|   显示页面    
; -----------+------+--------------
;   0x90006	 |  1	|   显示模式    
; -----------+------+--------------
;   0x90007	 |  1	|   字符列数    
; -----------+------+--------------
;   0x90008	 |  2	|   未知        
; -----------+------+--------------
;   0x9000A	 |  1	|   显示内存    
; -----------+------+--------------
;   0x9000B	 |  1	|   显示状态    
; -----------+------+--------------
;   0x9000C	 |  2	|   显卡特性参数 
; -----------+------+--------------
;   0x9000E	 |  1	|   屏幕行数    
; -----------+------+--------------
;   0x9000F	 |  1	|   屏幕列数    
; -----------+------+--------------
;   0x90080	 |  16	|   硬盘1参数表  
; -----------+------+--------------
;   0x90090	 |  16	|   硬盘2参数表  
; -----------+------+--------------
;   0x901FC	 |  2	|   根设备号    
; -----------+------+--------------
; 

.code

	ORG	0

WinMainCRTStartup:

; ok, the read went well so we get current cursor position and save it for
; posterity.

	mov		ax, INITSEG		; this is done in bootsect already, but...	将ds 置成INITSEG(9000).这已经在bootsect 程序中
	mov		ds, ax			;											设置过,但是现在是setup 程序,Linus 觉得需要再重新

	mov		ah, 03h			; read cursor pos							BIOS 中断10 的读光标功能号ah = 03
	xor		bh, bh			; 输入:bh = 页号
	int 	10h				; save it in known place, con_init fetches  返回:ch = 扫描开始线,cl = 扫描结束线,dh = 行号(00 是顶端),dl = 列号(00 是左边)
	mov		ds:[0], dx		; it from 0x90000.							dh = 行号(00 是顶端),dl = 列号(00 是左边)
	
	; Get memory size (extended mem, kB)

	mov		ah, 88h			; 这3句取扩展内存的大小值(KB).			
	int 	15h				; 是调用中断15,功能号ah = 88 返回:ax = 从100000(1M)处开始的扩展内存大小(KB).
	mov		ds:[2], ax		; 若出错则CF 置位,ax = 出错码.
	
	; Get video-card data:
	
	; 下面这段用于取显示卡当前显示模式.
	; 调用BIOS 中断10,功能号ah = 0f
	; 返回:ah = 字符列数,al = 显示模式,bh = 当前显示页.
	; 90004(1 字)存放当前页,90006 显示模式,90007 字符列数.
	
	mov		ah, 0Fh
	int 	10h
	mov		ds:[4], bx		; bh = display page
	mov		ds:[6], ax		; al = video mode, ah = window width
	
	; check for EGA/VGA and some config parameters

	; 检查显示方式(EGA/VGA)并取参数.
	; 调用BIOS 中断10,附加功能选择-取方式信息
	; 功能号:ah = 12,bl = 10
	; 返回:bh = 显示状态
	; (00 - 彩色模式,I/O 端口=3dX)
	; (01 - 单色模式,I/O 端口=3bX)
	; bl = 安装的显示内存
	; (00 - 64k, 01 - 128k, 02 - 192k, 03 = 256k)
	; cx = 显示卡特性参数(参见程序后的说明)
	mov		ah, 12h
	mov		bl, 10h
	int 	10h
	mov		ds:[8] , ax
	mov		ds:[10], bx
	mov		ds:[12], cx
	
	; Get hd0 data
	; 取第一个硬盘的信息(复制硬盘参数表).
	; 第1 个硬盘参数表的首地址竟然是中断向量41 的向量值！而第2 个硬盘
	; 参数表紧接第1 个表的后面,中断向量46 的向量值也指向这第2 个硬盘
	; 的参数表首址.表的长度是16 个字节(10).
	; 下面两段程序分别复制BIOS 有关两个硬盘的参数表,90080 处存放第1 个
	; 硬盘的表,90090 处存放第2 个硬盘的表.
	mov		ax, 0000h
	mov		ds, ax
	lds		si, ds:[4*41h]	; 取中断向量41 的值,也即hd0 参数表的地址 ds:si
	mov		ax, INITSEG
	mov		es, ax
	mov		di, 0080h		; 传输的目的地址: 9000:0080 -> es:di	
	mov		cx, 10h			; 共传输10字节 
	rep		movsb
	
	; Get hd1 data

	mov		ax, 0000h
	mov		ds, ax
	lds		si, ds:[4*46h]	; 取中断向量46 的值,也即hd1 参数表的地址 -> ds:si
	mov		ax, INITSEG
	mov		es, ax
	mov		di, 0090h		; 传输的目的地址: 9000:0090 -> es:di
	mov		cx, 10h
	rep		movsb
	
	; Check that there IS a hd1 :-)

	; 检查系统是否存在第2个硬盘,如果不存在则第2个表清零.
	; 利用BIOS 中断调用13 的取盘类型功能.
	; 功能号ah = 15;
	; 输入:dl = 驱动器号(8X 是硬盘:80 指第1 个硬盘,81 第2 个硬盘)
	; 输出:ah = 类型码 00 --没有这个盘 CF 置位 01 --是软驱,没有change-line 支持;
	; 02--是软驱(或其它可移动设备) 有change-line 支持 03 --是硬盘 
	mov		ax, 1500h
	mov		dl, 81h
	int 	13h
	jc		no_disk1
	cmp		ah, 3		; 类型3硬盘
	je		is_disk1
no_disk1:
	mov		ax, INITSEG	; 第2个硬盘不存在,则对第2个硬盘表清零
	mov		es, ax
	mov		di, 0090h
	mov		cx, 10h
	mov		ax, 00h
	rep		stosb

is_disk1:

	; now we want to move to protected mode ... 进入保护模式

	cli					; no interrupts allowed !
	
	; 首先我们将system 模块移到正确的位置.
	; bootsect 引导程序是将system 模块读入到从10000(64k)开始的位置.由于当时假设
	; system 模块最大长度不会超过80000(512k),也即其末端不会超过内存地址90000,
	; 所以bootsect 会将自己移动到90000 开始的地方,并把setup 加载到它的后面.
	; 下面这段程序的用途是再把整个system 模块移动到00000 位置,即把从10000 到8ffff
	; 的内存数据块(512k),整块地向内存低端移动了10000(64k)的位置 

	; first we move the system to it's rightful place
	
	mov		ax, 0000h
	cld					; 'direction'=0, movs moves forward

do_move:

	mov		es, ax		; destination segment es:di -> 目的地址(初始为0000:0)
	add		ax, 1000h
	cmp		ax, 9000h	; 已经把从8000 段开始的64k 代码移动完 
	jz		end_move
	mov		ds, ax		; source segment  ds:si -> 源地址(初始为1000:0)
	xor		di, di
	xor		si, si
	mov		cx, 8000h
	rep		movsw
	jmp		do_move
	
	; then we load the segment descriptors	
	
	; 此后,我们加载段描述符.
	; 从这里开始会遇到32 位保护模式的操作,因此需要Intel 32 位保护模式编程方面的知识了,
	; 有关这方面的信息请查阅列表后的简单介绍或附录中的详细说明.这里仅作概要说明.
	; 
	; lidt 指令用于加载中断描述符表(idt)寄存器,它的操作数是6 个字节,0-1 字节是描述符表的
	; 长度值(字节);2-5 字节是描述符表的32 位线性基地址(首地址),其形式参见下面
	; 219-220 行和223-224 行的说明.中断描述符表中的每一个表项(8 字节)指出发生中断时
	; 需要调用的代码的信息,与中断向量有些相似,但要包含更多的信息.
	; 
	; lgdt 指令用于加载全局描述符表(gdt)寄存器,其操作数格式与lidt 指令的相同.全局描述符
	; 表中的每个描述符项(8 字节)描述了保护模式下数据和代码段(块)的信息.其中包括段的
	; 最大长度限制(16 位),段的线性基址(32 位),段的特权级,段是否在内存,读写许可以及
	; 其它一些保护模式运行的标志.参见后面205-216行

end_move:

	mov		ax, SETUPSEG		; right, forgot this at first. didn't work :-)
	mov		ds,	ax				; ds 指向本程序(setup)段.因为上面操作改变了ds的值.
	lidt	FWORD PTR idt_48	; load idt with 0,0 加载中断描述符表(idt)寄存器,idt_48 是6 字节操作数的位置
	lgdt	FWORD PTR gdt_48	; load gdt with whatever appropriate 前2 字节表示idt 表的限长,后4 字节表示idt 表所处的基地址.
								; 加载全局描述符表(gdt)寄存器,gdt_48 是6 字节操作数的位置

	; that was painless, now we enable A20

	call	empty_8042			; 等待输入缓冲器空 只有当输入缓冲器为空时才可以对其进行写命令

	mov		al, 0D1h			; D1 命令码-表示要写数据到8042 的P2 端口.P2 端
	out		64h, al				; 口的位1 用于A20 线的选通.数据要写到60 口.
	call	empty_8042			; 等待输入缓冲器空,看命令是否被接受
	mov		al, 0DFh			; A20 on 选通A20 地址线的参数
	out		60h, al
	call	empty_8042			; 输入缓冲器为空,则表示A20 线已经选通
	
	; well, that went ok, I hope. Now we have to reprogram the interrupts :-(
	; we put them right after the intel-reserved hardware interrupts, at
	; int  0x20-0x2F. There they won't mess up anything. Sadly IBM really
	; messed this up with the original PC, and they haven't been able to
	; rectify it afterwards. Thus the bios puts interrupts at 0x08-0x0f,
	; which is used for the internal hardware interrupts as well. We just
	; have to reprogram the 8259's, and it isn't fun.

	; 希望以上一切正常.现在我们必须重新对中断进行编程 
	; 我们将它们放在正好处于intel 保留的硬件中断后面,在int  20-2F.
	; 在那里它们不会引起冲突.不幸的是 IBM 在原 PC 机中搞糟了,以后也没有纠正过来.
	; PC 机的 bios 将中断放在了08-0f,这些中断也被用于内部硬件中断.
	; 所以我们就必须重新对8259 中断控制器进行编程,这一点都没劲.

	mov		al, 11h		; initialization sequence
						; 11 表示初始化命令开始,是ICW1 命令字,表示边
						; 沿触发,多片8259 级连,最后要发送ICW4 命令字.
						; 发送到8259A 主芯片
	out		20h, al		; send it to 8259A-1 发送到8259A 主芯片
	jmp		$+2
	jmp		$+2
	out		0A0h, al	; and to 8259A-2  再发送到8259A 从芯片
	jmp		$+2
	jmp		$+2
	mov		al, 20h		; start of hardware int 's (0x20)
	out		21h, al		; 送主芯片ICW2 命令字,起始中断号,要送奇地址 
	jmp		$+2
	jmp		$+2
	mov		al, 28h		; start of hardware int 's 2 (0x28)
	out		0A1h, al	; 送从芯片ICW2 命令字,从芯片的起始中断号
	jmp		$+2
	jmp		$+2
	mov		al, 04h		; 8259-1 is master
	out		21h, al		; 送主芯片ICW3 命令字,主芯片的IR2 连从芯片int .
	jmp		$+2
	jmp		$+2
	mov		al, 02h		; 8259-2 is slave
	out		0A1h, al	; 送从芯片ICW3 命令字,表示从芯片的int  连到主芯片的IR2 引脚上
	jmp		$+2
	jmp		$+2
	mov		al, 01h		; 8086 mode for both
	out		21h, al		; 送主芯片ICW4 命令字.8086 模式;普通EOI 方式,需发送指令来复位.初始化结束,芯片就绪
	jmp		$+2
	jmp		$+2
	out		0A1h, al	; 送从芯片ICW4 命令字,内容同上.
	jmp		$+2
	jmp		$+2
	mov		al, 0FFh	; mask off all interrupts for now
	out		21h, al		; 屏蔽主芯片所有中断请求.
	jmp		$+2
	jmp		$+2
	out		0A1h, al	; 屏蔽从芯片所有中断请求.

	; well, that certainly wasn't fun :-(. Hopefully it works, and we don't
	; need no steenking BIOS anyway (except for the initial loading :-).
	; The BIOS-routine wants lots of unnecessary data, and it's less
	; "interesting" anyway. This is how REAL programmers do it.
	;
	; Well, now's the time to actually move into protected mode. To make
	; things as simple as possible, we do no register set-up or anything,
	; we let the gnu-compiled 32-bit programs do that. We just jump to
	; absolute address 0x00000, in 32-bit protected mode.


	; 上面这段当然没劲 ,希望这样能工作,而且我们也不再需要乏味的BIOS 了(除了
	; 初始的加载..BIOS 子程序要求很多不必要的数据,而且它一点都没趣.那是"真正"的
	; 程序员所做的事.
	  
	; 这里设置进入32 位保护模式运行.首先加载机器状态字(lmsw - Load Machine Status Word),
	; 也称控制寄存器CR0,其比特位0 置1 将导致CPU 工作在保护模式.

	mov		ax, 0001h	; protected mode (PE) bit
	lmsw	ax			; This is it!  就这样加载机器状态字
	push	0008h		; ldsym global "D:\\GitCode\\SvnBase\\Bins\\system-map-for-bohcs.map"
	push	1000h		; b 0x0090309
	retf

	; 我们已经将system 模块移动到00000 开始的地方,所以这里的偏移地址是0.这里的段值
	; 的8 已经是保护模式下的段选择符了,用于选择描述符表和描述符表项以及所要求的特权级.
	; 段选择符长度为16 位(2 字节);位0-1 表示请求的特权级0-3,linux 操作系统只用
	; 到两级:0 级(系统级)和3 级(用户级);位2 用于选择全局描述符表(0)还是局部描
	; 述符表(1);位3-15 是描述符表项的索引,指出选择第几项描述符.所以段选择符
	; 8(00000,0000,0000,1000)表示请求特权级0,使用全局描述符表中的第1 项,该项指出
	; 代码的基地址是0,因此这里的跳转指令就会去执行system 中的代码.

	; This routine checks that the keyboard command queue is empty
	; No timeout is used - if this hangs there is something wrong with
	; the machine, and we probably couldn't proceed anyway.

	; 下面这个子程序检查键盘命令队列是否为空.这里不使用超时方法 - 如果这里死机,
	; 则说明PC 机有问题,我们就没有办法再处理下去了.
	; 只有当输入缓冲器为空时(状态寄存器位2 = 0)才可以对其进行写命令

empty_8042:

	jmp		$+2
	jmp		$+2
	in		al , 64h	; 读 AT 键盘控制器状态寄存器.
	test	al , 02h	; is input buffer full 测试位2 输入缓冲器满 
	jnz		empty_8042	; yes - loop 
	ret
	
	ORG	0700h
	
	; 全局描述符表开始处.描述符表由多个 8 字节长的描述符项组成.
	; 这里给出了 3 个描述符项.第1 项无用,但须存在.第2 项是系统代码段
	; 描述符(208-211 行),第3 项是系统数据段描述符(213-216 行).每个描述符的具体
	; 含义参见列表后说明 
$gdt		WORD	0,0,0,0	; dummy

			WORD	07FFh				; 8Mb - limit=2047 (2048*4096=8Mb)
			WORD	0000h				; base address=0
			WORD	9A00h				; code read/exec
			WORD	00C0h				; granularity=4096, 386

			WORD	07FFh				; 8Mb - limit=2047 (2048*4096=8Mb)
			WORD	0000h				; base address=0
			WORD	9200h				; data read/write
			WORD	00C0h				; granularity=4096, 386
			
$idt_48		WORD	0					; idt limit=0
			WORD	0,0					; idt base=0L
			
$gdt_48		WORD	0800h				; gdt limit=2048, 256 GDT entries
			WORD	0200h+0700h,0009h	; gdt base = 0X9xxxx
	
	ORG	800h

END