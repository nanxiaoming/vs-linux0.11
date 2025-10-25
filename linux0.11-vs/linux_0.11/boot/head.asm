; linux/boot/head.s
; 
; (C) 1991  Linus Torvalds

; head.s contains the 32-bit startup code.
;
; NOTE!!! Startup happens at absolute address 0x00000000, which is also where
; the page directory will exist. The startup code will be overwritten by
; the page directory.

	.686P
	.model	flat, c

OPTION	casemap:none

PUBLIC	main
PUBLIC	idt
PUBLIC	gdt
PUBLIC	pg_dir
PUBLIC	tmp_floppy_area
PUBLIC	_end
EXTRN	stack_start:DWORD
EXTRN	os_entry:PROC
EXTRN	printk:PROC

pg_dir		=	0			; ҳĿ¼������������
_end		=	30000h

.code

; I put the kernel page tables right after the page directory,
; using 4 of them to span 16 Mb of physical memory. People with
; more than 16MB will have to expand this.

	ORG	0000h
pg0:

main:
	; �ٴ�ע��!!! �����Ѿ�����32 λ����ģʽ,��������$0x10 �����ǰѵ�ַ0x10 װ���
	; ���μĴ���,��������ʵ��ȫ�ֶ����������е�ƫ��ֵ,���߸���ȷ��˵��һ����������
	; ���ѡ���.�й�ѡ�����˵����μ�setup.s �е�˵��.����$0x10 �ĺ�����������Ȩ
	; ��0(λ0-1=0),ѡ��ȫ����������(λ2=0),ѡ����е�2 ��(λ3-15=2).������ָ�����
	; �����ݶ���������.(�������ľ�����ֵ�μ�ǰ��setup.s).�������ĺ�����:
	; ��ds,es,fs,gs �е�ѡ���Ϊsetup.s �й�������ݶ�(ȫ�ֶ���������ĵ�2 ��)=0x10,
	; ������ջ���������ݶ��е�_stack_start ������,Ȼ��ʹ���µ��ж����������ȫ�ֶ�
	; ������.�µ�ȫ�ֶ��������г�ʼ������setup.s �е���ȫһ��
	mov		eax, 10h
	mov		ds, ax
	mov		es, ax
	mov		fs, ax
	mov		gs, ax
	lss		esp, FWORD PTR stack_start	; ��ʾ_stack_start -> ss:esp,����ϵͳ��ջ 
										; stack_start ������kernel/sched.c,69�� 
	call	setup_idt
	call	setup_gdt
	mov		eax, 10h					; reload all the segment registers
	mov		ds, ax						; after changing gdt. CS was already
	mov		es, ax						; reloaded in 'setup_gdt'
	mov		fs, ax						; ��Ϊ�޸���gdt,������Ҫ����װ�����еĶμĴ���
	mov		gs, ax						; CS ����μĴ����Ѿ���setup_gdt�����¼��ع���
	lss		esp, FWORD PTR stack_start

	; ����5�����ڲ���A20 ��ַ���Ƿ��Ѿ�����.���õķ��������ڴ��ַ0x000000 ��д������
	; һ����ֵ,Ȼ���ڴ��ַ0x100000(1M)���Ƿ�Ҳ�������ֵ.���һֱ��ͬ�Ļ�,��һֱ
	; �Ƚ���ȥ,Ҳ����ѭ��,����.��ʾ��ַA20 ��û��ѡͨ,����ں˾Ͳ���ʹ��1M �����ڴ�.

	xor		eax, eax
@@:
	inc		eax						; check that A20 really IS enabled
	mov		ds:[ 00000000h ], eax		; loop forever if it isn't
	cmp		eax, [ 00100000h ]
	je		@B

	; NOTE! 486 should set bit 16, to check for write-protect in supervisor
	; mode. Then it would be unnecessary with the "verify_area()"-calls.
	; 486 users probably want to set the NE (#5) bit also, so as to use
	; int 16 for math errors.

	; 
	; ע��! ��������γ�����,486 Ӧ�ý�λ16 ��λ,�Լ���ڳ����û�ģʽ�µ�д����,
	; �˺�"verify_area()"�����оͲ���Ҫ��.486 ���û�ͨ��Ҳ���뽫NE(;//5)��λ,�Ա�
	; ����ѧЭ�������ĳ���ʹ�� int 16.
	; 

	; ������γ������ڼ����ѧЭ������оƬ�Ƿ����.�������޸Ŀ��ƼĴ���CR0,�ڼ���
	; ����Э�������������ִ��һ��Э������ָ��,�������Ļ���˵��Э������оƬ����
	; ��,��Ҫ����CR0 �е�Э����������λEM(λ2),����λЭ���������ڱ�־MP(λ1).
	
	mov		eax, cr0			; check math chip
	and		eax, 80000011h		; Save PG,PE,ET
	; "orl $0x10020,%eax" here for 486 might be good
	or		eax, 02h			; set MP
	mov		cr0, eax
	call	check_x87
	jmp		after_page_tables
	
	; We depend on ET to be correct. This checks for 287/387.

	; 
	; ����������ET ��־����ȷ�������287/387 �������.
	; 

check_x87:
	fninit
	fstsw	ax
	cmp		ax	, 0
	je		@F				; no coprocessor: have to set bits
	mov		eax	, cr0		; ������ڵ�����ǰ��ת�����1 ��,�����дcr0.
	xor		eax	, 06h		; reset MP, set EM
	mov		cr0	, eax
	ret
@@:
	fsetpm					; 287 Э�������� 
	ret
	
	; setup_idt
	;
	; sets up a idt with 256 entries pointing to
	; ignore_int, interrupt gates. It then loads
	; idt. Everything that wants to install itself
	; in the idt-table may do so themselves. Interrupts
	; are enabled elsewhere, when we can be relatively
	; sure everything is ok. This routine will be over-
	; written by the page tables.

	; 
	; ��������������ж����������ӳ���setup_idt
	; 
	; ���ж���������idt ���óɾ���256 ����,����ָ��ignore_int �ж���.Ȼ�����
	; �ж���������Ĵ���(��lidt ָ��).����ʵ�õ��ж����Ժ��ٰ�װ.������������
	; �ط���Ϊһ�ж�����ʱ�ٿ����ж�.���ӳ��򽫻ᱻҳ���ǵ�.
	; 
setup_idt:

	lea		edx, ignore_int			; ��ignore_int ����Ч��ַ(ƫ��ֵ)ֵ edx �Ĵ���
	mov		eax, 00080000h			; ��ѡ���0x0008 ����eax �ĸ�16 λ��.
	mov		ax, dx					; selector = 0x0008 = cs
									; ƫ��ֵ�ĵ�16 λ����eax �ĵ�16 λ��.��ʱeax ��
									; ������������4 �ֽڵ�ֵ.

	mov		dx, 8E00h				; interrupt gate - dpl=0, present
									; ��ʱedx ��������������4 �ֽڵ�ֵ
	lea		edi, idt
	mov		ecx, 256
rp_sidt:
	mov		[ edi ]	, eax			; �����ж����������������
	mov		[ edi+4 ]	, edx			  
	add		edi		, 8				; edi ָ�������һ��.
	dec		ecx
	jne		rp_sidt
	lidt	FWORD PTR idt_descr		; �����ж���������Ĵ���ֵ.
	ret
	
; setup_gdt
;
; This routines sets up a new gdt and loads it.
; Only two entries are currently built, the same
; ones that were built in init.s. The routine
; is VERY complicated at two whole lines, so this
; rather long comment is certainly needed :-).
; This routine will beoverwritten by the page tables.

; 
; �������������ȫ������������setup_gdt
; 
; ����ӳ�������һ���µ�ȫ����������gdt,������.��ʱ����������������,��ǰ
; ���һ��.���ӳ���ֻ������,"�ǳ���"����,���Ե�Ȼ��Ҫ��ô����ע����:).
; 
setup_gdt:
	lgdt	FWORD PTR gdt_descr
	ret
	
	; I put the kernel page tables right after the page directory,
	; using 4 of them to span 16 Mb of physical memory. People with
	; more than 16MB will have to expand this.

	; 
	; Linus ���ں˵��ڴ�ҳ��ֱ�ӷ���ҳĿ¼֮��,ʹ����4 ������Ѱַ16 Mb �������ڴ�.
	; ������ж���16 Mb ���ڴ�,����Ҫ��������������޸�.
	; 
	; ÿ��ҳ��Ϊ4 Kb �ֽ�,��ÿ��ҳ������Ҫ4 ���ֽ�,���һ��ҳ�����Դ��1000 ��,
	; �������һ������Ѱַ4 Kb �ĵ�ַ�ռ�,��һ��ҳ��Ϳ���Ѱַ4 Mb �������ڴ�.ҳ����
	; �ĸ�ʽΪ:���ǰ0-11 λ���һЩ��־,���Ƿ����ڴ���(P λ0),��д���(R/W λ1),
	; ��ͨ�û����ǳ����û�ʹ��(U/S λ2),�Ƿ��޸Ĺ�(�Ƿ�����)(D λ6)��;�����λ12-31 
	; ��ҳ���ַ,����ָ��һҳ�ڴ��������ʼ��ַ.

	ORG	1000h	; ��ƫ��0x1000 ����ʼ�ǵ�1 ��ҳ��(ƫ��0 ��ʼ�������ҳ��Ŀ¼).
pg1:

	ORG	2000h
pg2:

	ORG	3000h
pg3:

	ORG	4000h

	; tmp_floppy_area is used by the floppy-driver when DMA cannot
	; reach to a buffer-block. It needs to be aligned, so that it isn't
	; on a 64kB border.

	; 
	; ��DMA(ֱ�Ӵ洢������)���ܷ��ʻ����ʱ,�����tmp_floppy_area �ڴ��
	; �Ϳɹ�������������ʹ��.���ַ��Ҫ�������,�����Ͳ����Խ64kB �߽�.
	; 

tmp_floppy_area	BYTE 1024 DUP (0)

	; �����⼸����ջ����(pushl)����Ϊ����/init/main.c ����ͷ�����׼��.
	; ǰ��3 ����ջָ�֪����ʲô�õ�,Ҳ����Linus �����ڵ���ʱ�ܿ���������õ�..
	; 139 �е���ջ������ģ�����main.c ����ʱ���Ƚ����ص�ַ��ջ�Ĳ���,�������
	; main.c ��������˳�ʱ,�ͻ᷵�ص�����ı��L6 ������ִ����ȥ,Ҳ����ѭ��.
	; 140 �н�main.c �ĵ�ַѹ���ջ,����,�����÷�ҳ����(setup_paging)������
	; ִ��'ret'����ָ��ʱ�ͻὫmain.c ����ĵ�ַ������ջ,��ȥִ��main.c ����ȥ��.

after_page_tables:
	push	0			; These are the parameters to main :-)
	push	0
	push	0
	push	L6			; return address for main, if it decides to.
	push	os_entry
	jmp		setup_paging
L6:
	jmp		L6			; main should never return here, but
						; just in case, we know what happens.

; This is the default interrupt "handler" :-)
int_msg		BYTE	'Unknown interrupt',0Dh,0Ah,0
ALIGN	DWORD
ignore_int:
	push	eax
	push	ecx
	push	edx
	push	ds
	push	es
	push	fs
	mov		eax, 10h	; �ö�ѡ���(ʹds,es,fs ָ��gdt ���е����ݶ�).
	mov		ds, ax
	mov		es, ax
	mov		fs, ax
	push	OFFSET int_msg	; �ѵ���printk �����Ĳ���ָ��(��ַ)��ջ.
	call	printk			; �ú�����/kernel/printk.c ��.
							; '_printk'��printk �����ģ���е��ڲ���ʾ��.
	pop		eax				
	pop		fs
	pop		es
	pop		ds
	pop		edx
	pop		ecx
	pop		eax
	iretd					; �жϷ���(���жϵ���ʱѹ��ջ��CPU ��־�Ĵ���(32 λ)ֵҲ����).

	; setup_paging
	;
	; This routine sets up paging by setting the page bit
	; in cr0. The page tables are set up, identity-mapping
	; the first 16MB. The pager assumes that no illegal
	; addresses are produced (ie >4Mb on a 4Mb machine).
	;
	; NOTE! Although all physical memory should be identity
	; mapped by this routine, only the kernel page functions
	; use the >1Mb addresses directly. All "normal" functions
	; use just the lower 1Mb, or the local data space, which
	; will be mapped to some other place - mm keeps track of
	; that.
	;
	; For those with more memory than 16 Mb - tough luck. I've
	; not got it, why should you :-) The source is here. Change
	; it. (Seriously - it shouldn't be too difficult. Mostly
	; change some constants etc. I left it at 16Mb, as my machine
	; even cannot be extended past that (ok, but it was cheap :-)
	; I've tried to show which constants to change by having
	; some kind of marker at them (search for "16Mb"), but I
	; won't guarantee that's all :-( )

	; 
	; ����ӳ���ͨ�����ÿ��ƼĴ���cr0 �ı�־(PG λ31)���������ڴ�ķ�ҳ����
	; ����,�����ø���ҳ���������,�Ժ��ӳ��ǰ16 MB �������ڴ�.��ҳ���ٶ�
	; ��������Ƿ��ĵ�ַӳ��(Ҳ����ֻ��4Mb �Ļ��������ó�����4Mb ���ڴ��ַ).
	; 
	; ע�⣡�������е������ַ��Ӧ��������ӳ�����к��ӳ��,��ֻ���ں�ҳ���
	; ������ֱ��ʹ��>1Mb �ĵ�ַ.����"һ��"������ʹ�õ���1Mb �ĵ�ַ�ռ�,��
	; ����ʹ�þֲ����ݿռ�,��ַ�ռ佫��ӳ�䵽����һЩ�ط�ȥ-- mm(�ڴ�������)
	; �������Щ�µ�.
	; 
	; ������Щ�ж���16Mb �ڴ�ļһ�- ̫������,�һ�û��,Ϊʲô�����:-).�����
	; ������,���������޸İ�.(ʵ����,�Ⲣ��̫���ѵ�.ͨ��ֻ���޸�һЩ������.
	; �Ұ�������Ϊ16Mb,��Ϊ�ҵĻ�������ô�����������ܳ����������(��Ȼ,�ҵĻ� 
	; ���ܱ��˵�:-)).���Ѿ�ͨ������ĳ���־��������Ҫ�Ķ��ĵط�(����"16Mb"),
	; ���Ҳ��ܱ�֤����Щ�Ķ������� :-( )
	;
ALIGN	DWORD
setup_paging:

	mov		ecx, 1024*5		; 5 pages - pg_dir+4 page tables
	xor		eax, eax
	xor		edi, edi		; pg_dir is at 0x000
	cld						
	rep		stosd

	; ����4 ������ҳĿ¼�е���,���ǹ��� 4 ��ҳ������ֻ������ 4 ��.
	; ҳĿ¼��Ľṹ��ҳ������Ľṹһ��, 4 ���ֽ�Ϊ 1 ��.�μ������˵��.
	; "$pg0+7"��ʾ:0x00001007,��ҳĿ¼���еĵ�1 ��.
	; ��� 1 ��ҳ�����ڵĵ�ַ= 0x00001007 & 0xfffff000 = 0x1000;�� 1 ��ҳ��
	; �����Ա�־= 0x00001007 & 0x00000fff = 0x07,��ʾ��ҳ����,�û��ɶ�д.

	mov		ds:[ 00h ], pg0+07h	; set present bit/user r/w
	mov		ds:[ 04h ], pg1+07h	; --------- " " ---------
	mov		ds:[ 08h ], pg2+07h	; --------- " " ---------
	mov		ds:[ 0Ch ], pg3+07h	; --------- " " ---------

	; ���� 6 ����д 4 ��ҳ���������������,����:4(ҳ��)*1024(��/ҳ��)=4096 ��(0 - 0xfff),
	; Ҳ����ӳ�������ڴ�4096*4Kb = 16Mb.
	; ÿ���������:��ǰ����ӳ��������ڴ��ַ+ ��ҳ�ı�־(�����Ϊ7).
	; ʹ�õķ����Ǵ����һ��ҳ������һ�ʼ������˳����д.һ��ҳ������һ��
	; ��ҳ���е�λ����1023*4 = 4092.������һҳ�����һ���λ�þ��� $pg3+4092.
	mov		edi, pg3+0FFCh
	mov		eax, 00FFF007h		; 16Mb - 4096 + 7 (r/w user,p)
	std							; ����λ��λ,edi ֵ�ݼ�(4 �ֽ�).
@@:
	stosd						; fill pages backwards - more efficient :-) */
	sub		eax, 1000h			; ÿ��д��һ��,�����ֵַ��0x1000.
	jge		@B					; ���С��0 ��˵��ȫ��д����.
	xor		eax, eax			; pg_dir is at 0x0000
	mov		cr3, eax			; cr3 - page directory start 
	mov		eax, cr0			
	or		eax, 80000000h		; ���÷�ҳ
	mov		cr0, eax			; this also flushes prefetch-queue

	; �ڸı��ҳ�����־��Ҫ��ʹ��ת��ָ��ˢ��Ԥȡָ�����,�����õ��Ƿ���ָ��ret.
	; �÷���ָ�����һ�������ǽ���ջ�е�main����ĵ�ַ����,����ʼ����/init/main.c 
	; ����.�����򵽴����������� 
	ret
				
ALIGN DWORD

			WORD	0
idt_descr	WORD	256*8-1			; idt contains 256 entries
			DWORD	idt

ALIGN  DWORD
			WORD	0
gdt_descr	WORD	256*8-1			; so does gdt (not that that's any
			DWORD	gdt				; magic number, but it works for me :^)
					
ALIGN QWORD
idt			QWORD	256 DUP (0)

	; ȫ�ֱ�.ǰ4 ��ֱ��ǿ���(����),�����������,���ݶ�������,ϵͳ��������,
	; ����ϵͳ��������linux û�����ô�.���滹Ԥ����252 ��Ŀռ�,���ڷ���������
	; ����ľֲ�������(LDT)�Ͷ�Ӧ������״̬��TSS ��������.
	; (0-nul, 1-cs, 2-ds, 3-sys, 4-TSS0, 5-LDT0, 6-TSS1, 7-LDT1, 8-TSS2 etc...)

gdt			QWORD	0000000000000000h	; NULL descriptor
			QWORD	00C09A0000000FFFh	; 16Mb
			QWORD	00C0920000000FFFh	; 16Mb
			QWORD	0000000000000000h	; TEMPORARY - don't use
			QWORD	252 DUP (0)			; space for LDT's and TSS's etc

END
