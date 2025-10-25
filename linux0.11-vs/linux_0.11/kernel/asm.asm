; asm.s contains the low-level code for most hardware faults.
; page_exception is handled by the mm, so that isn't here. This
; file also handles (hopefully) fpu-exceptions due to TS-bit, as
; the fpu must be properly saved/resored. This hasn't been tested.

	.686P
	.model flat, c

OPTION	casemap:none

PUBLIC	divide_error
PUBLIC	debug
PUBLIC	nmi
PUBLIC	int3
PUBLIC	overflow
PUBLIC	bounds
PUBLIC	invalid_op
PUBLIC	double_fault
PUBLIC	coprocessor_segment_overrun
PUBLIC	invalid_TSS
PUBLIC	segment_not_present
PUBLIC	stack_segment
PUBLIC	general_protection
PUBLIC	irq13
PUBLIC	reserved
EXTRN	do_divide_error:PROC
EXTRN	do_int3:PROC
EXTRN	do_nmi:PROC
EXTRN	do_overflow:PROC
EXTRN	do_bounds:PROC
EXTRN	do_invalid_op:PROC
EXTRN	do_coprocessor_segment_overrun:PROC
EXTRN	do_reserved:PROC
EXTRN	do_double_fault:PROC
EXTRN	do_invalid_TSS:PROC
EXTRN	do_segment_not_present:PROC
EXTRN	do_stack_segment:PROC
EXTRN	do_general_protection:PROC
EXTRN	coprocessor_error:PROC

.code

divide_error:
	push	do_divide_error
no_error_code:
	xchg	eax, [esp]
	push	ebx
	push	ecx
	push	edx
	push	edi
	push	esi
	push	ebp
	push	ds
	push	es
	push	fs
	push	0				; "error code"
	lea		edx, 44[esp]
	push	edx
	mov		edx, 10h
	mov		ds, dx
	mov		es, dx
	mov		fs, dx
	call	eax
	add		esp, 08h
	pop		fs
	pop		es
	pop		ds
	pop		ebp
	pop		esi
	pop		edi
	pop		edx
	pop		ecx
	pop		ebx
	pop		eax
	iretd
	
debug:
	push	do_int3			; _do_debug
	jmp		no_error_code
	
nmi:
	push	do_nmi
	jmp		no_error_code
	
int3:
	push	do_int3
	jmp		no_error_code

overflow:
	push	do_overflow
	jmp		no_error_code
	
bounds:
	push	do_bounds
	jmp		no_error_code
	
invalid_op:
	push	do_invalid_op
	jmp		no_error_code
	
coprocessor_segment_overrun:
	push	do_coprocessor_segment_overrun
	jmp		no_error_code
	
reserved:
	push	do_reserved
	jmp		no_error_code
	
irq13:
	push	eax
	xor		al, al
	out		0F0h, al
	mov		al, 20h
	out		20h, al
	jmp		$+2
	jmp		$+2
	out		0A0h, al
	pop		eax
	jmp		coprocessor_error
	
double_fault:
	push	do_double_fault
error_code:
	xchg	4[esp], eax		; error code <-> %eax
	xchg	[esp], ebx		; &function <-> %ebx
	push	ecx
	push	edx
	push	edi
	push	esi
	push	ebp
	push	ds
	push	es
	push	fs
	push	eax				; error code
	lea		eax, 44[esp]	; offset
	push	eax
	mov		eax, 10h
	mov 	ds, ax
	mov		es, ax
	mov		fs, ax
	call	ebx
	add		esp, 08h
	pop		fs
	pop		es
	pop		ds
	pop		ebp
	pop		esi
	pop		edi
	pop		edx
	pop		ecx
	pop		ebx
	pop		eax
	iretd
	
invalid_TSS:
	push	do_invalid_TSS
	jmp		error_code
	
segment_not_present:
	push	do_segment_not_present
	jmp		error_code
	
stack_segment:
	push	do_stack_segment
	jmp		error_code

general_protection:
	push	do_general_protection
	jmp		error_code
	
END