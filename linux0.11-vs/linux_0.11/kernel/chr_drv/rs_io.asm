; linux/kernel/rs_io.s
;
; (C) 1991  Linus Torvalds

; rs_io.s
;
; This module implements the rs232 io interrupts.

; 
; �ó���ģ��ʵ�� rs232 ��������жϴ������.
; 
	.686P
	.model flat, c

OPTION	casemap:none

PUBLIC	rs1_interrupt
PUBLIC	rs2_interrupt
EXTRN	do_tty_interrupt:PROC
EXTRN	table_list:DWORD

; size �Ƕ�д���л��������ֽڳ���
$size		=	1024	; must be power of two !
						; and must match the value
						; in tty_io.c!!!

; these are the offsets into the read/write buffer structures

; these are the offsets into the read/write buffer structures */
; ������Щ�Ƕ�д����ṹ�е�ƫ���� */
; ��Ӧ������include/linux/tty.h �ļ���tty_queue �ṹ�и�������ƫ����.
rs_addr		=	0		; ���ж˿ں��ֶ�ƫ��(�˿ں���0x3f8 ��0x2f8).
head		=	4		; ��������ͷָ���ֶ�ƫ��.
tail		=	8		; ��������βָ���ֶ�ƫ��.
proc_list	=	12		; �ȴ��û���Ľ����ֶ�ƫ��.
buf			=	16		; �������ֶ�ƫ��.

startup		=	256		; chars left in write queue when we restart it

;��д�����ﻹʣ256 ���ַ��ռ�(WAKEUP_CHARS)ʱ,���ǾͿ���д

.code

;
;��Щ��ʵ�ʵ��жϳ���.�������ȼ���жϵ���Դ,Ȼ��ִ����Ӧ
;�Ĵ���.
;

; These are the actual interrupt routines. They look where
; the interrupt is coming from, and take appropriate action.

ALIGN	DWORD
;���ж˿�1 �жϴ��������ڵ�
rs1_interrupt:
	push	table_list+8	;tty ���ж�Ӧ����1 �Ķ�д����ָ��ĵ�ַ��ջ(tty_io.c,99).
	jmp		rs_int
ALIGN	DWORD
; ���ж˿�2 �жϴ��������ڵ�
rs2_interrupt:
	push	table_list+16	;tty ���ж�Ӧ����2 �Ķ�д�������ָ��ĵ�ַ��ջ
rs_int:
	push	edx
	push	ecx
	push	ebx
	push	eax
	push	es
	push	ds					; as this is an interrupt, we cannot
	push	10h					; know that bs is ok. Load it
	pop		ds					; ��������һ���жϳ���,���ǲ�֪��ds �Ƿ���ȷ���Լ�������(��ds��es ָ���ں����ݶ�
	push	10
	pop		es
	mov		edx, 24[ esp ]		; ���������ָ���ַ����edx �Ĵ�
	mov		edx, [ edx ]			; ȡ������ָ��(��ַ)->edx
	mov		edx, rs_addr[ edx ]	; ȡ����1 �Ķ˿ں�??edx
	add		edx, 2				; interrupt ident. reg.edx ָ���жϱ�ʶ�Ĵ���
rep_int:						; �жϱ�ʶ�Ĵ����˿���0x3fa(0x2fa),�μ��Ͻ��б����Ϣ
	xor		eax, eax			; eax ����.
	in		al, dx				; ȡ�жϱ�ʶ�ֽ�,�����ж��ж���Դ(��4 ���ж����).
	test	al, 1				; �����ж����޴�������ж�(λ0=1 ���ж�;=0 ���ж�).
	jne		$end				; ���޴������ж�,����ת���˳�����end.
	cmp		al, 6				; this shouldn't happen, but ... */ /* �ⲻ�ᷢ��,���ǡ�*/; this shouldn't happen, but ...
	ja		$end				; al ֵ>6? ������ת��end(û������״̬).
	mov		ecx, 24[ esp ]		; ��ȡ�������ָ���ַ??ecx.
	push	edx					; ���˿ں�0x3fa(0x2fa)��ջ.
	sub		edx, 2				; 0x3f8(0x2f8).
	call	jmp_table[ eax*2 ]	; NOTE! not ;*4, bit0 is 0 already */ /* ����4,λ0 ����0*/; NOTE! not *4, bit0 is 0 already
								; ���������ָ,���д������ж�ʱ,al ��λ0=0,λ2-1 ���ж�����,����൱���Ѿ����ж�����
								; ����2,�����ٳ�2,�õ���ת���Ӧ���ж����͵�ַ,����ת������ȥ����Ӧ����.
	pop		edx					; �����жϱ�ʶ�Ĵ����˿ں�0x3fa(��0x2fa)
	jmp		rep_int				; ��ת,�����ж����޴������жϲ���������
$end:
	mov		al, 20h				; ���жϿ��������ͽ����ж�ָ��EOI
	out		20h, al				; EOI
	pop		ds
	pop		es
	pop		eax
	pop		ebx
	pop		ecx
	pop		edx
	add		esp, 4				; jump over _table_list entry �����������ָ���ַ
	iretd

; ���ж����ʹ�������ַ��ת��,����4 ���ж���Դ��
; modem ״̬�仯�ж�,д�ַ��ж�,���ַ��ж�,��·״̬�������ж�
jmp_table	DWORD	modem_status,write_char,read_char,line_status

ALIGN	DWORD
modem_status:
	add		edx, 6				; clear intr by reading modem status reg
	in		al, dx				; ͨ����modem ״̬�Ĵ������и�λ(0x3fe)
	ret
	
ALIGN	DWORD
line_status:
	add		edx, 5				; clear intr by reading line status reg.
	in		al, dx				; ͨ������·״̬�Ĵ������и�λ(0x3fd)
	ret
	
ALIGN	DWORD
read_char:						; ��ȡ�ַ�->al.
	in		al, dx				; ��ǰ���ڻ������ָ���ַ??edx.
	mov		edx, ecx			; �������ָ�����ַ - ��ǰ���ڶ���ָ���ַ??edx,
	sub		edx, table_list		; ��ֵ/8.���ڴ���1 ��1,���ڴ���2 ��2.
	shr		edx, 3				; read-queue # ȡ��������нṹ��ַ??ecx.
	mov		ecx, [ ecx ]		; ȡ�������л���ͷָ��??ebx.
	mov		ebx, head[ ecx ]	; ���ַ����ڻ�������ͷָ����ָ��λ��.
	mov		buf[ ecx+ebx ], al	; ��ͷָ��ǰ��һ�ֽ�.
	inc		ebx					; �û�������С��ͷָ�����ģ����.ָ�벻�ܳ�����������С.
	and		ebx, $size-1		; ������ͷָ����βָ��Ƚ�.
	cmp		ebx, tail[ ecx ]	; �����,��ʾ��������,��ת�����1 ��.
	je		@F					; �����޸Ĺ���ͷָ��.
	mov		head[ ecx ], ebx	; �����ں�ѹ���ջ(1- ����1,2 - ����2),��Ϊ����,
@@:								;
	push	edx					; ����tty �жϴ���C ����(.
	call	do_tty_interrupt	; ������ջ����,������.
	add		esp, 4
	ret

ALIGN	DWORD
write_char:
	mov		ecx, 4[ ecx ]		; write-queue # ȡд������нṹ��ַ??ecx.		
	mov		ebx, head[ ecx ]	; ȡд����ͷָ��??ebx.
	sub		ebx, tail[ ecx ]	; ͷָ�� - βָ�� = �������ַ���.
	and		ebx, $size-1		; nr chars in queue # ��ָ��ȡģ����.	
	je		write_buffer_empty	; ���ͷָ�� = βָ��,˵��д�������ַ�,��ת����.
	cmp		ebx, startup		; �������ַ�������256 ��?
	ja		@F
	mov		ebx, proc_list[ ecx ]	; wake up sleeping process # ���ѵȴ��Ľ���.ȡ�ȴ��ö��еĽ��̵�ָ��,���ж��Ƿ�Ϊ��	
	test	ebx, ebx			; is there any? # �еȴ��Ľ�����?	
	je		@F					; �ǿյ�,����ǰ��ת�����1 ��.
	mov		DWORD PTR [ ebx ], 0	; ���򽫽�����Ϊ������״̬(���ѽ���)..
@@:								
	mov		ebx, tail[ ecx ]		; ȡβָ��.
	mov		al, buf[ ecx+ebx ]	; �ӻ�����βָ�봦ȡһ�ַ�??al.
	out		dx, al				; ��˿�0x3f8(0x2f8)�ͳ������ּĴ�����.
	inc		ebx					; βָ��ǰ��.
	and		ebx, $size-1		; βָ������������ĩ��,���ۻ�.
	mov		tail[ ecx ], ebx		; �������޸Ĺ���βָ��.
	cmp		ebx, head[ ecx ]		; βָ����ͷָ��Ƚ�,
	je		write_buffer_empty	; �����,��ʾ�����ѿ�,����ת.
	ret
	
ALIGN	DWORD
write_buffer_empty:
	mov		ebx, proc_list[ ecx ]	; wake up sleeping process
	test	ebx, ebx			; is there any?  ȡ�ȴ��ö��еĽ��̵�ָ��,���ж��Ƿ�Ϊ��
	je		@F
	mov		DWORD PTR [ ebx ], 0	; ���򽫽�����Ϊ������״̬(���ѽ���)
@@:
	inc		edx					; ָ��˿�0x3f9(0x2f9).
	in		al, dx				; ��ȡ�ж�����Ĵ���.
	jmp		$+2
	jmp		$+2
	and		al, 0Dh				; ���η��ͱ��ּĴ������ж�
	out		dx, al				; д��0x3f9(0x2f9)
	ret
	
END