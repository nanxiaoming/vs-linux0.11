; linux/mm/page.s
;
; (C) 1991  Linus Torvalds

; page.s contains the low-level page-exception code.
; the real work is done in mm.c

 	.686P
	.model	flat, c

OPTION	casemap:none

PUBLIC	page_fault
EXTRN	do_no_page:PROC
EXTRN	do_wp_page:PROC

.code

page_fault:
	xchg	[esp], eax
	push	ecx
	push	edx
	push	ds
	push	es
	push	fs
	mov		edx, 10h
	mov		ds, dx
	mov		es, dx
	mov		fs, dx
	mov		edx, cr2
	push	edx
	push	eax
	test	eax, 1
	jne		LN1
	call	do_no_page
	jmp		LN2
LN1:
	call	do_wp_page
LN2:
	add		esp, 08h
	pop		fs
	pop		es
	pop		ds
	pop		edx
	pop		ecx
	pop		eax
	iretd
	
END