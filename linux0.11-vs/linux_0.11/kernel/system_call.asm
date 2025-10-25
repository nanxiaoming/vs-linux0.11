; linux/kernel/system_call.s
;
; (C) 1991  Linus Torvalds

; system_call.s  contains the system-call low-level handling routines.
; This also contains the timer-interrupt handler, as some of the code is
; the same. The hd- and flopppy-interrupts are also here.
;
; NOTE: This code handles signal-recognition, which happens every time
; after a timer-interrupt and after each system call. Ordinary interrupts
; don't handle signal-recognition, as that would clutter them up totally
; unnecessarily.
;
; Stack layout in 'ret_from_system_call':
;
; 	 0(%esp) - %eax
; 	 4(%esp) - %ebx
; 	 8(%esp) - %ecx
; 	 C(%esp) - %edx
; 	10(%esp) - %fs
; 	14(%esp) - %es
; 	18(%esp) - %ds
; 	1C(%esp) - %eip
; 	20(%esp) - %cs
; 	24(%esp) - %eflags
; 	28(%esp) - %oldesp
; 	2C(%esp) - %oldss

	.686P
	.model	flat, c

OPTION	casemap:none

PUBLIC	system_call
PUBLIC	sys_fork
PUBLIC	timer_interrupt
PUBLIC	sys_execve
PUBLIC	hd_interrupt
PUBLIC	floppy_interrupt
PUBLIC	parallel_interrupt
PUBLIC	device_not_available
PUBLIC	coprocessor_error
EXTRN	current:DWORD
EXTRN	task:DWORD
EXTRN	jiffies:DWORD
EXTRN	sys_call_table:DWORD
EXTRN	do_hd:DWORD
EXTRN	do_floppy:DWORD
EXTRN	schedule:PROC
EXTRN	do_timer:PROC
EXTRN	find_empty_process:PROC
EXTRN	copy_process:PROC
EXTRN	do_execve:PROC
EXTRN	do_signal:PROC
EXTRN	unexpected_hd_interrupt:PROC
EXTRN	unexpected_floppy_interrupt:PROC
EXTRN	math_state_restore:PROC
EXTRN	math_error:PROC
EXTRN	math_emulate:PROC

SIG_CHLD	= 17

$EAX		=	00h
$EBX		=	04h
$ECX		=	08h
$EDX		=	0Ch
$FS			=	10h
$ES			=	14h
$DS			=	18h
$EIP		=	1Ch
$CS			=	20h
EFLAGS		=	24h
OLDESP		= 	28h
OLDSS		=	2Ch

state		=	0		; these are offsets into the task-struct.
counter		=	4
priority	=	8
signal		=	12
sigaction	=	16		; MUST be 16 (=len of sigaction)
blocked		=	(33*16)

; offsets within sigaction
sa_handler	= 0
sa_mask		= 4
sa_flags	= 8
sa_restorer	= 12

nr_system_calls	=	72

; Ok, I get parallel printer interrupts while using the floppy for some
; strange reason. Urgel. Now I just ignore them.

.code

ALIGN	DWORD
bad_sys_call:
	mov		eax, -1
	iretd
ALIGN	DWORD
reschedule:
	push	ret_from_sys_call
	jmp		schedule

ALIGN	DWORD
system_call:

	; Stack layout in 'ret_from_system_call':
	;
	; 	 0(%esp) - %eax
	; 	 4(%esp) - %ebx
	; 	 8(%esp) - %ecx
	; 	 C(%esp) - %edx
	; 	10(%esp) - %fs
	; 	14(%esp) - %es
	; 	18(%esp) - %ds
	; 	1C(%esp) - %eip
	; 	20(%esp) - %cs
	; 	24(%esp) - %eflags
	; 	28(%esp) - %oldesp
	; 	2C(%esp) - %oldss

	cmp		eax, nr_system_calls-1
	ja		bad_sys_call
	push	ds
	push	es
	push	fs
	push	edx
	push	ecx			; push %ebx,%ecx,%edx as parameters
	push	ebx			; to the system call
	mov		edx, 10h	; set up ds,es to kernel space
	mov		ds, dx
	mov		es, dx
	mov		edx, 17h	; fs points to local data space
	mov		fs, dx
	call	sys_call_table[ eax*4 ]
	push	eax
	mov		eax, current
	cmp		DWORD PTR state[ eax ], 0		; state
	jne		reschedule
	cmp		DWORD PTR counter[ eax ], 0	; counter
	je		reschedule
ret_from_sys_call:
	mov		eax, current
	cmp		eax, task
	je		@F
	cmp		WORD PTR $CS[ esp ], 0Fh		; was old code segment supervisor ?
	jne		@F
	cmp		WORD PTR OLDSS[ esp ], 17h	; was stack segment = 0x17 ?
	jne		@F
	mov		ebx, signal[ eax ]
	mov		ecx, blocked[ eax ]
	not		ecx
	and		ecx, ebx
	bsf		ecx, ecx
	je		@F
	btr		ebx, ecx
	mov		signal[ eax ], ebx
	inc		ecx
	push	ecx
	call	do_signal
	pop		eax
@@:
	pop		eax
	pop		ebx
	pop		ecx
	pop		edx
	pop		fs
	pop		es
	pop		ds
	iretd

ALIGN	DWORD
coprocessor_error:
	push	ds
	push	es
	push	fs
	push	edx
	push	ecx
	push	ebx
	push	eax
	mov		eax, 10h
	mov		ds, ax
	mov		es, ax
	mov		eax, 17h
	mov		fs, ax
	push	ret_from_sys_call
	jmp		math_error
	
ALIGN	DWORD
device_not_available:
	push	ds
	push	es
	push	fs
	push	edx
	push	ecx
	push	ebx
	push	eax
	mov		eax, 10h
	mov		ds, ax
	mov		es, ax
	mov		eax, 17h
	mov		fs, ax
	push	ret_from_sys_call
	clts				; clear TS so that we can use math
	mov		eax, cr0
	test	eax, 04h	; EM (math emulation bit)
	je		math_state_restore
	push	ebp
	push	esi
	push	edi
	call	math_emulate
	pop		edi
	pop		esi
	pop		ebp
	ret
	
ALIGN	DWORD
timer_interrupt:
	push	ds			; save ds,es and put kernel data space
	push	es			; into them. %fs is used by _system_call
	push	fs
	push	edx			; we save %eax,%ecx,%edx as gcc doesn't
	push	ecx			; save those across function calls. %ebx
	push	ebx			; is saved as we use that in ret_sys_call
	push	eax
	mov		eax, 10h
	mov		ds, ax
	mov		es, ax
	mov		eax, 17h
	mov		fs, ax
	inc		jiffies		; 启动以来的滴答数
	mov		al, 20h		; EOI to interrupt controller #1
	out		20h, al
	mov		eax, $CS[ esp ]
	and		eax, 03h	; %eax is CPL (0 or 3, 0=supervisor)
	push	eax
	call	do_timer	; 'do_timer(LONG CPL)' does everything from
	add		esp, 04h	; task switching to accounting ...
	jmp		ret_from_sys_call

ALIGN	DWORD
sys_execve:
	lea		eax, $EIP[ esp ]
	push	eax
	call	do_execve
	add		esp, 04h
	ret

ALIGN	DWORD
sys_fork:
	call	find_empty_process
	test	eax, eax
	js		@F
	push	gs
	push	esi
	push	edi
	push	ebp
	push	eax
	call	copy_process
	add		esp, 14h
@@:
	ret
	
hd_interrupt:
	push	eax
	push	ecx
	push	edx
	push	ds
	push	es
	push	fs
	mov		eax, 10h
	mov		ds, ax
	mov		es, ax
	mov		eax, 17h
	mov		fs, ax
	mov		al, 20h
	out		0A0h, al
	jmp		$+2
	jmp		$+2
	xor		edx, edx
	xchg	edx, do_hd
	test	edx, edx
	jne		@F
	mov		edx, unexpected_hd_interrupt
@@:
	out		20h, al
	call	edx
	pop		fs
	pop		es
	pop		ds
	pop		edx
	pop		ecx
	pop		eax
	iretd
	
floppy_interrupt:
	push	eax
	push	ecx
	push	edx
	push	ds
	push	es
	push	fs
	
	mov		eax, 10h
	mov		ds, ax
	mov		es, ax
	mov		eax, 17h
	mov		fs, ax
	mov		al, 20h
	out		20h, al		; EOI to interrupt controller #1
	xor		eax, eax
	xchg	eax, do_floppy
	test	eax, eax
	jne		@F
	mov		eax, unexpected_floppy_interrupt
@@:
	call	eax
	pop		fs
	pop		es
	pop		ds
	pop		edx
	pop		ecx
	pop		eax
	iretd
	
parallel_interrupt:
	push	eax
	mov		al, 20h
	out		20h, al
	pop		eax
	iretd

END