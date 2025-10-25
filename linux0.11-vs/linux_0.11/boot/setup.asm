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

; setup.asm�����BIOS �л�ȡϵͳ����,������Щ���ݷŵ�ϵͳ�ڴ���ʵ��ط�.
; ��ʱsetup.s ��system �Ѿ���bootsect ��������ص��ڴ���.
; ��δ���ѯ��bios �й��ڴ�/����/��������,������Щ�����ŵ�һ��
; "��ȫ��"�ط�:90000-901FF,Ҳ��ԭ��bootsect �����������
; �ĵط�,Ȼ���ڱ�����鸲�ǵ�֮ǰ�ɱ���ģʽ��system ��ȡ.
; ������Щ������ú�bootsect.asm �е���ͬ 

INITSEG		=	9000h	; we move boot here - out of the way
SYSSEG		=	1000h	; system loaded at 0x10000 (65536).
SETUPSEG	=	9020h	; this is the current segment

idt_48		=	ds:[ 0718h ]
gdt_48		=	ds:[ 071Eh ]

; 
;
;   �ڴ��ַ    ����      ����
; -----------+------+--------------
;   0x90000	 |  2	|   ���λ��    
; -----------+------+--------------
;   0x90002	 |  2	|   ��չ�ڴ���   
; -----------+------+--------------
;   0x90004	 |  2	|   ��ʾҳ��    
; -----------+------+--------------
;   0x90006	 |  1	|   ��ʾģʽ    
; -----------+------+--------------
;   0x90007	 |  1	|   �ַ�����    
; -----------+------+--------------
;   0x90008	 |  2	|   δ֪        
; -----------+------+--------------
;   0x9000A	 |  1	|   ��ʾ�ڴ�    
; -----------+------+--------------
;   0x9000B	 |  1	|   ��ʾ״̬    
; -----------+------+--------------
;   0x9000C	 |  2	|   �Կ����Բ��� 
; -----------+------+--------------
;   0x9000E	 |  1	|   ��Ļ����    
; -----------+------+--------------
;   0x9000F	 |  1	|   ��Ļ����    
; -----------+------+--------------
;   0x90080	 |  16	|   Ӳ��1������  
; -----------+------+--------------
;   0x90090	 |  16	|   Ӳ��2������  
; -----------+------+--------------
;   0x901FC	 |  2	|   ���豸��    
; -----------+------+--------------
; 

.code

	ORG	0

WinMainCRTStartup:

; ok, the read went well so we get current cursor position and save it for
; posterity.

	mov		ax, INITSEG		; this is done in bootsect already, but...	��ds �ó�INITSEG(9000).���Ѿ���bootsect ������
	mov		ds, ax			;											���ù�,����������setup ����,Linus ������Ҫ������

	mov		ah, 03h			; read cursor pos							BIOS �ж�10 �Ķ���깦�ܺ�ah = 03
	xor		bh, bh			; ����:bh = ҳ��
	int 	10h				; save it in known place, con_init fetches  ����:ch = ɨ�迪ʼ��,cl = ɨ�������,dh = �к�(00 �Ƕ���),dl = �к�(00 �����)
	mov		ds:[0], dx		; it from 0x90000.							dh = �к�(00 �Ƕ���),dl = �к�(00 �����)
	
	; Get memory size (extended mem, kB)

	mov		ah, 88h			; ��3��ȡ��չ�ڴ�Ĵ�Сֵ(KB).			
	int 	15h				; �ǵ����ж�15,���ܺ�ah = 88 ����:ax = ��100000(1M)����ʼ����չ�ڴ��С(KB).
	mov		ds:[2], ax		; ��������CF ��λ,ax = ������.
	
	; Get video-card data:
	
	; �����������ȡ��ʾ����ǰ��ʾģʽ.
	; ����BIOS �ж�10,���ܺ�ah = 0f
	; ����:ah = �ַ�����,al = ��ʾģʽ,bh = ��ǰ��ʾҳ.
	; 90004(1 ��)��ŵ�ǰҳ,90006 ��ʾģʽ,90007 �ַ�����.
	
	mov		ah, 0Fh
	int 	10h
	mov		ds:[4], bx		; bh = display page
	mov		ds:[6], ax		; al = video mode, ah = window width
	
	; check for EGA/VGA and some config parameters

	; �����ʾ��ʽ(EGA/VGA)��ȡ����.
	; ����BIOS �ж�10,���ӹ���ѡ��-ȡ��ʽ��Ϣ
	; ���ܺ�:ah = 12,bl = 10
	; ����:bh = ��ʾ״̬
	; (00 - ��ɫģʽ,I/O �˿�=3dX)
	; (01 - ��ɫģʽ,I/O �˿�=3bX)
	; bl = ��װ����ʾ�ڴ�
	; (00 - 64k, 01 - 128k, 02 - 192k, 03 = 256k)
	; cx = ��ʾ�����Բ���(�μ�������˵��)
	mov		ah, 12h
	mov		bl, 10h
	int 	10h
	mov		ds:[8] , ax
	mov		ds:[10], bx
	mov		ds:[12], cx
	
	; Get hd0 data
	; ȡ��һ��Ӳ�̵���Ϣ(����Ӳ�̲�����).
	; ��1 ��Ӳ�̲�������׵�ַ��Ȼ���ж�����41 ������ֵ������2 ��Ӳ��
	; ��������ӵ�1 ����ĺ���,�ж�����46 ������ֵҲָ�����2 ��Ӳ��
	; �Ĳ�������ַ.��ĳ�����16 ���ֽ�(10).
	; �������γ���ֱ���BIOS �й�����Ӳ�̵Ĳ�����,90080 ����ŵ�1 ��
	; Ӳ�̵ı�,90090 ����ŵ�2 ��Ӳ�̵ı�.
	mov		ax, 0000h
	mov		ds, ax
	lds		si, ds:[4*41h]	; ȡ�ж�����41 ��ֵ,Ҳ��hd0 ������ĵ�ַ ds:si
	mov		ax, INITSEG
	mov		es, ax
	mov		di, 0080h		; �����Ŀ�ĵ�ַ: 9000:0080 -> es:di	
	mov		cx, 10h			; ������10�ֽ� 
	rep		movsb
	
	; Get hd1 data

	mov		ax, 0000h
	mov		ds, ax
	lds		si, ds:[4*46h]	; ȡ�ж�����46 ��ֵ,Ҳ��hd1 ������ĵ�ַ -> ds:si
	mov		ax, INITSEG
	mov		es, ax
	mov		di, 0090h		; �����Ŀ�ĵ�ַ: 9000:0090 -> es:di
	mov		cx, 10h
	rep		movsb
	
	; Check that there IS a hd1 :-)

	; ���ϵͳ�Ƿ���ڵ�2��Ӳ��,������������2��������.
	; ����BIOS �жϵ���13 ��ȡ�����͹���.
	; ���ܺ�ah = 15;
	; ����:dl = ��������(8X ��Ӳ��:80 ָ��1 ��Ӳ��,81 ��2 ��Ӳ��)
	; ���:ah = ������ 00 --û������� CF ��λ 01 --������,û��change-line ֧��;
	; 02--������(���������ƶ��豸) ��change-line ֧�� 03 --��Ӳ�� 
	mov		ax, 1500h
	mov		dl, 81h
	int 	13h
	jc		no_disk1
	cmp		ah, 3		; ����3Ӳ��
	je		is_disk1
no_disk1:
	mov		ax, INITSEG	; ��2��Ӳ�̲�����,��Ե�2��Ӳ�̱�����
	mov		es, ax
	mov		di, 0090h
	mov		cx, 10h
	mov		ax, 00h
	rep		stosb

is_disk1:

	; now we want to move to protected mode ... ���뱣��ģʽ

	cli					; no interrupts allowed !
	
	; �������ǽ�system ģ���Ƶ���ȷ��λ��.
	; bootsect ���������ǽ�system ģ����뵽��10000(64k)��ʼ��λ��.���ڵ�ʱ����
	; system ģ����󳤶Ȳ��ᳬ��80000(512k),Ҳ����ĩ�˲��ᳬ���ڴ��ַ90000,
	; ����bootsect �Ὣ�Լ��ƶ���90000 ��ʼ�ĵط�,����setup ���ص����ĺ���.
	; ������γ������;���ٰ�����system ģ���ƶ���00000 λ��,���Ѵ�10000 ��8ffff
	; ���ڴ����ݿ�(512k),��������ڴ�Ͷ��ƶ���10000(64k)��λ�� 

	; first we move the system to it's rightful place
	
	mov		ax, 0000h
	cld					; 'direction'=0, movs moves forward

do_move:

	mov		es, ax		; destination segment es:di -> Ŀ�ĵ�ַ(��ʼΪ0000:0)
	add		ax, 1000h
	cmp		ax, 9000h	; �Ѿ��Ѵ�8000 �ο�ʼ��64k �����ƶ��� 
	jz		end_move
	mov		ds, ax		; source segment  ds:si -> Դ��ַ(��ʼΪ1000:0)
	xor		di, di
	xor		si, si
	mov		cx, 8000h
	rep		movsw
	jmp		do_move
	
	; then we load the segment descriptors	
	
	; �˺�,���Ǽ��ض�������.
	; �����￪ʼ������32 λ����ģʽ�Ĳ���,�����ҪIntel 32 λ����ģʽ��̷����֪ʶ��,
	; �й��ⷽ�����Ϣ������б��ļ򵥽��ܻ�¼�е���ϸ˵��.���������Ҫ˵��.
	; 
	; lidt ָ�����ڼ����ж���������(idt)�Ĵ���,���Ĳ�������6 ���ֽ�,0-1 �ֽ������������
	; ����ֵ(�ֽ�);2-5 �ֽ������������32 λ���Ի���ַ(�׵�ַ),����ʽ�μ�����
	; 219-220 �к�223-224 �е�˵��.�ж����������е�ÿһ������(8 �ֽ�)ָ�������ж�ʱ
	; ��Ҫ���õĴ������Ϣ,���ж�������Щ����,��Ҫ�����������Ϣ.
	; 
	; lgdt ָ�����ڼ���ȫ����������(gdt)�Ĵ���,���������ʽ��lidt ָ�����ͬ.ȫ��������
	; ���е�ÿ����������(8 �ֽ�)�����˱���ģʽ�����ݺʹ����(��)����Ϣ.���а����ε�
	; ��󳤶�����(16 λ),�ε����Ի�ַ(32 λ),�ε���Ȩ��,���Ƿ����ڴ�,��д����Լ�
	; ����һЩ����ģʽ���еı�־.�μ�����205-216��

end_move:

	mov		ax, SETUPSEG		; right, forgot this at first. didn't work :-)
	mov		ds,	ax				; ds ָ�򱾳���(setup)��.��Ϊ��������ı���ds��ֵ.
	lidt	FWORD PTR idt_48	; load idt with 0,0 �����ж���������(idt)�Ĵ���,idt_48 ��6 �ֽڲ�������λ��
	lgdt	FWORD PTR gdt_48	; load gdt with whatever appropriate ǰ2 �ֽڱ�ʾidt ����޳�,��4 �ֽڱ�ʾidt �������Ļ���ַ.
								; ����ȫ����������(gdt)�Ĵ���,gdt_48 ��6 �ֽڲ�������λ��

	; that was painless, now we enable A20

	call	empty_8042			; �ȴ����뻺������ ֻ�е����뻺����Ϊ��ʱ�ſ��Զ������д����

	mov		al, 0D1h			; D1 ������-��ʾҪд���ݵ�8042 ��P2 �˿�.P2 ��
	out		64h, al				; �ڵ�λ1 ����A20 �ߵ�ѡͨ.����Ҫд��60 ��.
	call	empty_8042			; �ȴ����뻺������,�������Ƿ񱻽���
	mov		al, 0DFh			; A20 on ѡͨA20 ��ַ�ߵĲ���
	out		60h, al
	call	empty_8042			; ���뻺����Ϊ��,���ʾA20 ���Ѿ�ѡͨ
	
	; well, that went ok, I hope. Now we have to reprogram the interrupts :-(
	; we put them right after the intel-reserved hardware interrupts, at
	; int  0x20-0x2F. There they won't mess up anything. Sadly IBM really
	; messed this up with the original PC, and they haven't been able to
	; rectify it afterwards. Thus the bios puts interrupts at 0x08-0x0f,
	; which is used for the internal hardware interrupts as well. We just
	; have to reprogram the 8259's, and it isn't fun.

	; ϣ������һ������.�������Ǳ������¶��жϽ��б�� 
	; ���ǽ����Ƿ������ô���intel ������Ӳ���жϺ���,��int  20-2F.
	; ���������ǲ��������ͻ.���ҵ��� IBM ��ԭ PC ���и�����,�Ժ�Ҳû�о�������.
	; PC ���� bios ���жϷ�����08-0f,��Щ�ж�Ҳ�������ڲ�Ӳ���ж�.
	; �������Ǿͱ������¶�8259 �жϿ��������б��,��һ�㶼û��.

	mov		al, 11h		; initialization sequence
						; 11 ��ʾ��ʼ�����ʼ,��ICW1 ������,��ʾ��
						; �ش���,��Ƭ8259 ����,���Ҫ����ICW4 ������.
						; ���͵�8259A ��оƬ
	out		20h, al		; send it to 8259A-1 ���͵�8259A ��оƬ
	jmp		$+2
	jmp		$+2
	out		0A0h, al	; and to 8259A-2  �ٷ��͵�8259A ��оƬ
	jmp		$+2
	jmp		$+2
	mov		al, 20h		; start of hardware int 's (0x20)
	out		21h, al		; ����оƬICW2 ������,��ʼ�жϺ�,Ҫ�����ַ 
	jmp		$+2
	jmp		$+2
	mov		al, 28h		; start of hardware int 's 2 (0x28)
	out		0A1h, al	; �ʹ�оƬICW2 ������,��оƬ����ʼ�жϺ�
	jmp		$+2
	jmp		$+2
	mov		al, 04h		; 8259-1 is master
	out		21h, al		; ����оƬICW3 ������,��оƬ��IR2 ����оƬint .
	jmp		$+2
	jmp		$+2
	mov		al, 02h		; 8259-2 is slave
	out		0A1h, al	; �ʹ�оƬICW3 ������,��ʾ��оƬ��int  ������оƬ��IR2 ������
	jmp		$+2
	jmp		$+2
	mov		al, 01h		; 8086 mode for both
	out		21h, al		; ����оƬICW4 ������.8086 ģʽ;��ͨEOI ��ʽ,�跢��ָ������λ.��ʼ������,оƬ����
	jmp		$+2
	jmp		$+2
	out		0A1h, al	; �ʹ�оƬICW4 ������,����ͬ��.
	jmp		$+2
	jmp		$+2
	mov		al, 0FFh	; mask off all interrupts for now
	out		21h, al		; ������оƬ�����ж�����.
	jmp		$+2
	jmp		$+2
	out		0A1h, al	; ���δ�оƬ�����ж�����.

	; well, that certainly wasn't fun :-(. Hopefully it works, and we don't
	; need no steenking BIOS anyway (except for the initial loading :-).
	; The BIOS-routine wants lots of unnecessary data, and it's less
	; "interesting" anyway. This is how REAL programmers do it.
	;
	; Well, now's the time to actually move into protected mode. To make
	; things as simple as possible, we do no register set-up or anything,
	; we let the gnu-compiled 32-bit programs do that. We just jump to
	; absolute address 0x00000, in 32-bit protected mode.


	; ������ε�Ȼû�� ,ϣ�������ܹ���,��������Ҳ������Ҫ��ζ��BIOS ��(����
	; ��ʼ�ļ���..BIOS �ӳ���Ҫ��ܶ಻��Ҫ������,������һ�㶼ûȤ.����"����"��
	; ����Ա��������.
	  
	; �������ý���32 λ����ģʽ����.���ȼ��ػ���״̬��(lmsw - Load Machine Status Word),
	; Ҳ�ƿ��ƼĴ���CR0,�����λ0 ��1 ������CPU �����ڱ���ģʽ.

	mov		ax, 0001h	; protected mode (PE) bit
	lmsw	ax			; This is it!  ���������ػ���״̬��
	push	0008h		; ldsym global "D:\\GitCode\\SvnBase\\Bins\\system-map-for-bohcs.map"
	push	1000h		; b 0x0090309
	retf

	; �����Ѿ���system ģ���ƶ���00000 ��ʼ�ĵط�,���������ƫ�Ƶ�ַ��0.����Ķ�ֵ
	; ��8 �Ѿ��Ǳ���ģʽ�µĶ�ѡ�����,����ѡ����������������������Լ���Ҫ�����Ȩ��.
	; ��ѡ�������Ϊ16 λ(2 �ֽ�);λ0-1 ��ʾ�������Ȩ��0-3,linux ����ϵͳֻ��
	; ������:0 ��(ϵͳ��)��3 ��(�û���);λ2 ����ѡ��ȫ����������(0)���Ǿֲ���
	; ������(1);λ3-15 �����������������,ָ��ѡ��ڼ���������.���Զ�ѡ���
	; 8(00000,0000,0000,1000)��ʾ������Ȩ��0,ʹ��ȫ�����������еĵ�1 ��,����ָ��
	; ����Ļ���ַ��0,����������תָ��ͻ�ȥִ��system �еĴ���.

	; This routine checks that the keyboard command queue is empty
	; No timeout is used - if this hangs there is something wrong with
	; the machine, and we probably couldn't proceed anyway.

	; ��������ӳ����������������Ƿ�Ϊ��.���ﲻʹ�ó�ʱ���� - �����������,
	; ��˵��PC ��������,���Ǿ�û�а취�ٴ�����ȥ��.
	; ֻ�е����뻺����Ϊ��ʱ(״̬�Ĵ���λ2 = 0)�ſ��Զ������д����

empty_8042:

	jmp		$+2
	jmp		$+2
	in		al , 64h	; �� AT ���̿�����״̬�Ĵ���.
	test	al , 02h	; is input buffer full ����λ2 ���뻺������ 
	jnz		empty_8042	; yes - loop 
	ret
	
	ORG	0700h
	
	; ȫ����������ʼ��.���������ɶ�� 8 �ֽڳ��������������.
	; ��������� 3 ����������.��1 ������,�������.��2 ����ϵͳ�����
	; ������(208-211 ��),��3 ����ϵͳ���ݶ�������(213-216 ��).ÿ���������ľ���
	; ����μ��б��˵�� 
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