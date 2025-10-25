;
; bootsect.s		(C) 1991 Linus Torvalds
;

; bootsect.s is loaded at 0x7c00 by the bios-startup routines, and moves
; iself out of the way to address 0x90000, and jumps there.
;
; It then loads 'setup' directly after itself (0x90200), and the system
; at 0x10000, using BIOS interrupts. 
;
; NOTE; currently system is at most 8*65536 bytes long. This should be no
; problem, even in the future. I want to keep it simple. This 512 kB
; kernel size should be enough, especially as this doesn't contain the
; buffer cache as in minix
;
; The loader has been made as simple as possible, and continuos
; read errors will result in a unbreakable loop. Reboot by hand. It
; loads pretty fast by getting whole sectors at a time whenever possible.

	.model	tiny, c
	.686P

OPTION	casemap:none

PUBLIC	WinMainCRTStartup

	; SYS_SIZE is the number of clicks (16 bytes) to be loaded.
	; 0x3000 is 0x30000 bytes = 196kB, more than enough for current
	; versions of linux

	; SYSSIZE��Ҫ���صĽ���(16�ֽ�Ϊ1��).3000h��Ϊ30000h�ֽڣ�192kB
	; �Ե�ǰ�İ汾�ռ����㹻��.

SYSSIZE		=	3000h

SETUPLEN	=	4		; nr of setup-sectors					;setup�����������(setup��sectors)ֵ
BOOTSEG		=	07C0h	; original address of boot-sector		;bootsect��ԭʼ��ַ(�Ƕε�ַ,����ͬ)
INITSEG		=	9000h	; we move boot here - out of the way	;��bootsect�Ƶ�����
SETUPSEG	=	9020h	; setup starts here						;setup��������￪ʼ
SYSSEG		=	1000h	; system loaded at 0x10000 (65536).		;systemģ����ص�10000(64kB)��.
ENDSEG		=	SYSSEG+SYSSIZE	; where to stop loading			;ֹͣ���صĶε�ַ
		
; ROOT_DEV:	0x000 - same type of floppy as boot.
; 0x301 - first partition on first drive etc

; DEF_ROOT_DEV:	000h - ���ļ�ϵͳ�豸ʹ��������ʱͬ���������豸.
; 301 - ���ļ�ϵͳ�豸�ڵ�һ��Ӳ�̵ĵ�һ��������,�ȵ�
; ָ�����ļ�ϵͳ�豸�ǵ�1��Ӳ�̵ĵ�1������.����Linux��ʽ��Ӳ������
; ��ʽ,����ֵ�ĺ�������:
; �豸�� �� ���豸��*256 �� ���豸�� 
;           (Ҳ�� dev_no = (major<<8 + minor)
; (���豸��:1���ڴ�,2������,3��Ӳ��,4��ttyx,5��tty,6�����п�,7���������ܵ�)
; 300 - /dev/hd0 �� ����������1��Ӳ��
; 301 - /dev/hd1 �� ��1���̵ĵ�1������
; ... ...
; 304 - /dev/hd4 �� ��1���̵ĵ�4������
; 305 - /dev/hd5 �� ����������2��Ӳ��
; 306 - /dev/hd6 �� ��2���̵ĵ�1������
; ... ...
; 309 - /dev/hd9 �� ��1���̵ĵ�4������ 

ROOT_DEV	=	0301h

sread		=	ds:[ 01C0h ]
head		=	ds:[ 01C2h ]
track		=	ds:[ 01C4h ]
sectors		=	ds:[ 01C6h ]
msg1		=	ds:[ 01C8h ]
root_dev	=	ds:[ 01FCh ]

;************************************************************************
; boot��bios�������ӳ��������7c00h(31k)��,�����Լ��ƶ�����
; ��ַ90000h(576k)��,����ת������.
; ��Ȼ��ʹ��BIOS�жϽ�'setup'ֱ�Ӽ��ص��Լ��ĺ���(90200h)(576.5k),
; ����system���ص���ַ10000h��.
; 
; ע��:Ŀǰ���ں�ϵͳ��󳤶�����Ϊ(8*65536)(512kB)�ֽ�,��ʹ����
; ������ҲӦ��û�������.�����������ּ�����.����512k������ں˳���Ӧ��
; �㹻��,����������û����minix��һ���������������ٻ���.
; 
; ���س����Ѿ����Ĺ�����,���Գ����Ķ�����������ѭ��.ֻ���ֹ�����.
; ֻҪ����,ͨ��һ��ȡȡ���е�����,���ع��̿������ĺܿ��.
;************************************************************************
.code

	ORG	0

WinMainCRTStartup:
	mov		ax, BOOTSEG
	mov		ds, ax
	mov		ax, INITSEG
	mov		es, ax
	mov		cx, 256
	xor		si, si
	xor		di, di
	rep		movsw
	push	INITSEG
	push	0020h		; OFFSET go
	retf
	
	ORG	0020h

go:
	mov		ax, cs		; ��ds,es��ss���ó��ƶ���������ڵĶδ�(9000h)
	mov		ds, ax		; ���ڳ������ж�ջ����(push,pop,call)��˱������ö�ջ.
	mov		es, ax
  ; put		stack at 0x9ff00.
	mov		ss, ax
	mov		sp, 0FF00h	; arbitrary value >>512
						; ���ڴ�����ƶ�����,����Ҫ�������ö�ջ�ε�λ��.
						; spֻҪָ��Զ����512ƫ��(����ַ90200h)��
						; ������.��Ϊ��90200h��ַ��ʼ����Ҫ����setup����,
						; ����ʱsetup�����ԼΪ4������,���spҪָ���
						; ��(200h + 200h*4 + ��ջ��С)��
; load the setup-sectors directly after the bootblock.
; Note that 'es' is already set up.

load_setup:

	; ����10�е���;������BIOS�ж� int 13h��setupģ��Ӵ��̵�2������
	; ��ʼ����90200h��ʼ��,����4������.���������,��λ������,��
	; ����,û����·.
	; int  13h ��ʹ�÷�������:
	; ah = 02h - �������������ڴ�; al = ��Ҫ��������������;
	; ch = �ŵ�(����)�ŵĵ�8λ;    cl = ��ʼ����(0��5λ),�ŵ��Ÿ�2λ(6��7);
	; dh = ��ͷ��;				dl = ��������(�����Ӳ����Ҫ��Ϊ7);
	; es:bx ->ָ�����ݻ�����;  ���������CF��־��λ. 

	mov		dx, 0000h	; drive 0, head 0
	mov		cx,	0002h	; sector 2, track 0
	mov		bx, 0200h	; address = 512, in INITSEG
	mov		ax, 0200h + SETUPLEN	; service 2, nr of sectors
	int 		13h			; read it
	jnc		ok_load_setup	; ok - continue
	mov		dx,	0000h
	mov		ax, 0000h	; reset the diskette
	int 		13h
	jmp		load_setup
	
ok_load_setup:
	
	; Get disk drive parameters, specifically nr of sectors/track
	; ȡ�����������Ĳ���,�ر���ÿ������������.
	; ȡ�������������� int 13h ���ø�ʽ�ͷ�����Ϣ����:
	; ah = 08h	dl = ��������(�����Ӳ����Ҫ��λ7Ϊ1).
	; ������Ϣ:
	; ���������CF��λ,����ah = ״̬��.
	; ah = 0, al = 0, bl = ����������(AT/PS2)
	; ch = ���ŵ��ŵĵ�8λ,cl = ÿ�ŵ����������(λ0-5),���ŵ��Ÿ�2λ(λ6-7)
	; dh = ����ͷ��,       ������ ����������,
	; es:di -> �������̲�����  

	mov		dl, 00h
	mov		ax, 0800h	; AH=8 is get drive parameters
	int 	13h
	mov		ch, 00h
	mov		sectors, cx	; ����ÿ�ŵ�������.
	mov		ax, INITSEG
	mov		es, ax		; ��Ϊ����ȡ���̲����жϸĵ���es��ֵ,�������¸Ļ�.
	
	; Print some inane message
	mov		ah, 03h
	xor		bh, bh
	int 		10h
	mov		cx, SIZEOF $msg1
	mov		bx, 0007h	; page 0, attribute 7 (normal)
	mov		bp, OFFSET msg1
	mov		ax, 1301h
	int 		10h

	; ok, we've written the message, now
	; we want to load the system (at 0x10000)
	mov		ax, SYSSEG
	mov		es, ax		; segment of 0x010000
	call	read_it
	call	kill_motor

	; �˺�,���Ǽ��Ҫʹ���ĸ����ļ�ϵͳ�豸(��Ƹ��豸).����Ѿ�ָ�����豸(!=0)
	; ��ֱ��ʹ�ø������豸.�������Ҫ����BIOS�����ÿ�ŵ���������
	; ȷ������ʹ��/dev/PS0(2,28)����/dev/at0(2,8).
	; 	����һ���������豸�ļ��ĺ���:
	; 	��Linux�����������豸����2(�μӵ�43��ע��),���豸�� = type*4 + nr, ����
	; 	nrΪ0��3�ֱ��Ӧ����A,B,C��D;type������������(2->1.2M��7->1.44M��).
	; 	��Ϊ7*4 + 0 = 28,����/dev/PS0(2,28)ָ����1.44M A������,���豸����021c
	; 	ͬ�� /dev/at0(2,8)ָ����1.2M A������,���豸����0208.

	; After that we check which root-device to use. If the device is
	; defined (!= 0), nothing is done and the given device is used.
	; Otherwise, either /dev/PS0 (2,28) or /dev/at0 (2,8), depending
	; on the number of sectors that the BIOS reports currently.
	
	mov		ax, root_dev
	cmp		ax, 0
	jne		root_defined
	mov		bx, sectors		; ȡ���汣���ÿ�ŵ�������.���sectors=15
							; ��˵����1.2Mb��������;���sectors=18,��˵����
							; 1.44Mb����.��Ϊ�ǿ�������������,���Կ϶���A��.
	mov		ax, 0208h		; /dev/ps0 - 1.2Mb
	cmp		bx, 15			; �ж�ÿ�ŵ��������Ƿ�=15
	je		root_defined	; �������,��ax�о����������������豸��.
	mov		ax, 021Ch		; /dev/PS0 - 1.44Mb
	cmp		bx, 18
	je		root_defined
undef_root:					; �������һ��,����ѭ��(����).
	jmp		undef_root
root_defined:
	mov		root_dev, ax	; ���������豸�ű�������.
	
; after that (everyting loaded), we jump to 
; the setup-routine loaded directly after
; the bootblock:

	push	SETUPSEG
	push	0
	retf

; This routine loads the system at address 0x10000, making sure
; no 64kB boundaries are crossed. We try to load it as fast as
; possible, loading whole tracks whenever we can.
;
; in:	es - starting address segment (normally 0x1000)

; ���ӳ���ϵͳģ����ص��ڴ��ַ10000h��,��ȷ��û�п�Խ64kB���ڴ�߽�.
; ������ͼ����ؽ��м���,ֻҪ����,��ÿ�μ��������ŵ�������
; 
; ����:es �� ��ʼ�ڴ��ַ��ֵ(ͨ����1000h)
; 
read_it:					; ��������Ķ�ֵ.����λ���ڴ��ַ64KB�߽紦,���������ѭ��.
	mov		ax, es			; ��bx�Ĵ���,���ڱ�ʾ��ǰ���ڴ�����ݵĿ�ʼλ��.
	test	ax, 0FFFh
die:
	jne		die			; es must be at 64kB boundary
	xor		bx, bx		; bx is starting address within segment
rp_read:
	; �ж��Ƿ��Ѿ�����ȫ������.�Ƚϵ�ǰ�������Ƿ����ϵͳ����ĩ�������Ķ�(#ENDSEG),���
	; ���Ǿ���ת������ok1_read��Ŵ�����������.�����˳��ӳ��򷵻�.
	mov		ax, es
	cmp		ax, ENDSEG	; have we loaded all yet?
	jb		ok1_read
	ret
ok1_read:
	; �������֤��ǰ�ŵ���Ҫ��ȡ��������,����ax�Ĵ�����.
	; ���ݵ�ǰ�ŵ���δ��ȡ���������Լ����������ֽڿ�ʼƫ��λ��,�������ȫ����ȡ��Щ
	; δ������,�������ֽ����Ƿ�ᳬ��64KB�γ��ȵ�����.���ᳬ��,����ݴ˴�����ܶ�
	; ����ֽ���(64KB - ����ƫ��λ��),������˴���Ҫ��ȡ��������.
	mov		ax, sectors				; ȡÿ�ŵ�������.
	sub		ax, sread				; ��ȥ��ǰ�ŵ��Ѷ�������.
	mov		cx, ax					; ax = ��ǰ�ŵ�δ��������.
	shl		cx, 9					; cx = cx * 512 �ֽ�.  
	add		cx, bx					; cx = cx + ���ڵ�ǰƫ��ֵ(bx)
									;    = �˴ζ�������,���ڹ�������ֽ���.
	jnc		ok2_read				; ��û�г���64KB�ֽ�,����ת��ok2_read��ִ��.
	je		ok2_read				  
	xor		ax, ax					; �����ϴ˴ν����ŵ�������δ������ʱ�ᳬ��64KB,�����
	sub		ax, bx					; ��ʱ����ܶ�����ֽ���(64KB �� ���ڶ�ƫ��λ��),��ת��
	shr		ax, 9					; ����Ҫ��ȡ��������.
ok2_read:							
	call	read_track
	mov		cx, ax					; dx = �ô˲����Ѷ�ȡ��������.
	add		ax, sread				; ��ǰ�ŵ����Ѿ���ȡ��������.
	cmp		ax, sectors				; �����ǰ�ŵ��ϵĻ�������δ��,����ת��ok3_read��.
	jne		ok3_read
	mov		ax, 1
	sub		ax, head				; �жϵ�ǰ��ͷ��.
	jne		ok4_read				; �����0��ͷ,����ȥ��1��ͷ���ϵ���������
	inc		WORD PTR track			; ����ȥ����һ�ŵ�.
ok4_read:
	mov		head, ax				; ���浱ǰ��ͷ��.
	xor		ax, ax					; �嵱ǰ�ŵ��Ѷ�������.
ok3_read:
	mov		sread, ax				; ���浱ǰ�ŵ��Ѷ�������.
	shl		cx, 9					; �ϴ��Ѷ�������*512�ֽ�.
	add		bx, cx					; ������ǰ�������ݿ�ʼλ��.
	jnc		rp_read					; ��С��64KB�߽�ֵ,����ת��rp_read��,����������.
	mov		ax, es					; ���������ǰ��,Ϊ����һ��������׼��.
	add		ax, 1000h				; ���λ�ַ����Ϊָ����һ��64KB���ڴ�.
	mov		es, ax
	xor		bx, bx
	jmp		rp_read
	
	; ����ǰ�ŵ���ָ����ʼ��������������������ݵ�es:bx��ʼ��.
	; al �� ���������; es:bx �� ��������ʼλ��.
read_track:
	push	ax
	push	bx
	push	cx
	push	dx
	mov		dx, track				;// ȡ��ǰ�ŵ���.
	mov		cx, sread				;// ȡ��ǰ�ŵ����Ѷ�������.
	inc		cx						;// cl = ��ʼ������.
	mov		ch, dl					;// ch = ��ǰ�ŵ���.
	mov		dx, head				;// ȡ��ǰ��ͷ��.
	mov		dh, dl					;// dh = ��ͷ��.
	mov		dl, 0					;// dl = ��������(Ϊ0��ʾ��ǰ������).
	and		dx, 0100h				;// ��ͷ�Ų�����1
	mov		ah, 2					;// ah = 2, �������������ܺ�.
	int 		13h
	jc		bad_rt
	pop		dx
	pop		cx
	pop		bx
	pop		ax
	ret
bad_rt:
	mov		ax, 0
	mov		dx, 0
	int 		13h
	pop		dx
	pop		cx
	pop		bx
	pop		ax
	jmp		read_track
	
; This procedure turns off the floppy drive motor, so
; that we enter the kernel in a known state, and
; don't have to worry about it later.

kill_motor:
	push	dx
	mov		dx, 03F2h		 ; �������ƿ��������˿�,ֻд.
	mov		al, 0			 ; A������,�ر�FDC,��ֹDMA���ж�����,�ر����.
	outsb					 ; ��al�е����������dxָ���Ķ˿�ȥ.
	pop		dx
	ret
	
	ORG	01C0h

$sread		WORD	1+SETUPLEN	; sectors read of current track
$head		WORD	0				; current head
$track		WORD	0				; current track
$sectors	WORD	0
$msg1		BYTE	0Dh,0Ah,'Loading system ...',0Dh,0Ah,0Dh,0Ah
	
	ORG	01FCh

_root_dev	WORD	ROOT_DEV
_boot_flag	WORD	0AA55h

END