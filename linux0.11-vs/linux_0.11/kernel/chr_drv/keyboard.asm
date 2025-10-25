; linux/kernel/keyboard.S
;
; (C) 1991  Linus Torvalds

; Thanks to Alfred Leung for US keyboard patches
; Wolfgang Thiel for German keyboard patches
; Marc Corsini for the French keyboard

	.686P
	.model flat, c

OPTION	casemap:none

PUBLIC	keyboard_interrupt
EXTRN	do_tty_interrupt:PROC
EXTRN	show_stat:PROC
EXTRN	table_list:DWORD

KBD_US		EQU

; these are for the keyboard read functions

$size		=	1024	; must be a power of two ! And MUST be the same
						; as in tty_io.c !!!!
head		=	4
tail		=	8
proc_list	=	12
buf			=	16

.code

mode		BYTE	0	; caps, alt, ctrl and shift mode
leds		BYTE	2	; num-lock, caps, scroll-lock mode (nom-lock on)
e0			BYTE	0

; con_int is the real interrupt routine that reads the
; keyboard scan-code and converts it into the appropriate
; ascii character(s).

keyboard_interrupt:
	push	eax
	push	ebx
	push	ecx
	push	edx
	push	ds
	push	es
	mov		eax, 10h
	mov		ds, ax
	mov		es, ax
	xor		al, al			; %eax is scan code
	in		al, 60h
	cmp		al, 0E0h
	je		set_e0
	cmp		al, 0E1h
	je		set_e1
	call	key_table[ eax*4 ]
	mov		e0, 0
e0_e1:
	in		al, 61h
	jmp		$+2
	jmp		$+2
	or		al, 80h
	jmp		$+2
	jmp		$+2
	out		61h, al
	jmp		$+2
	jmp		$+2
	and		al, 7Fh
	out		61h, al
	mov		al, 20h
	out		20h, al
	push	0
	call	do_tty_interrupt
	add		esp, 04h
	pop		es
	pop		ds
	pop		edx
	pop		ecx
	pop		ebx
	pop		eax
	iretd
set_e0:
	mov		e0, 1
	jmp		e0_e1
set_e1:
	mov		e0, 2
	jmp		e0_e1

; This routine fills the buffer with max 8 bytes, taken from
; %ebx:%eax. (%edx is high). The bytes are written in the
; order %al,%ah,%eal,%eah,%bl,%bh ... until %eax is zero.

put_queue:
	push	ecx
	push	edx
	mov		edx, table_list	; read-queue for console
	mov		ecx, head[ edx ]
LN1:
	mov		buf[ edx+ecx ], al
	inc		ecx
	and		ecx, $size-1
	cmp		ecx, tail[ edx ]	; buffer full - discard everything
	je		LN3
	shrd	eax, ebx, 8
	je		LN2
	shr		ebx, 8
	jmp		LN1
LN2:
	mov		head[ edx ], ecx
	mov		ecx, proc_list[ edx ]
	test	ecx, ecx
	je		LN3
	mov		DWORD PTR [ ecx ], 0
LN3:
	pop		edx
	pop		ecx
	ret
	
ctrl:
	mov		al, 04h
	jmp		@F
alt:
	mov		al, 10h
@@:
	cmp		e0, 0
	je		@F
	and		al, al
@@:
	or		mode, al
	ret
unctrl:
	mov		al, 04h
	jmp		@F
unalt:
	mov		al, 10h
@@:
	cmp		e0, 0
	je		@F
	and		al, al
@@:
	not		al
	and		mode, al
	ret
	
lshift:
	or		mode, 01h
	ret
unlshift:
	and		mode, 0FEh
	ret
rshift:
	or		mode, 02h
	ret
unrshift:
	and		mode, 0FDh
	ret
	
caps:
	test	mode, 80h
	jne		@F
	xor		leds, 4
	xor		mode, 40h
	or		mode, 80h
set_leds:
	call	kb_wait
	mov		al, 0EDh		; set leds command
	out		60h, al
	call	kb_wait
	mov		al, leds
	out		60h, al
	ret
uncaps:
	and		mode, 7Fh
	ret
scroll:
	xor		leds, 1
	jmp		set_leds
num:
	xor		leds, 2
	jmp		set_leds

; curosr-key/numeric keypad cursor keys are handled here.
; checking for numeric keypad etc.

cursor:
	sub		al, 47h
	jb		@F
	cmp		al, 12
	ja		@F
	jne		cur2			; check for ctrl-alt-del
	test	mode, 0Ch
	je		cur2
	test	mode, 30h
	jne		reboot
cur2:
	cmp		e0, 01h			; e0 forces cursor movement
	je		cur
	test	leds, 02h		; not num-lock forces cursor
	je		cur
	test	mode, 03h		; shift forces cursor
	jne		cur
	xor		ebx, ebx
	mov		al, num_table[ eax ]
	jmp		put_queue
@@:
	ret
	
cur:
	mov		al, cur_table[ eax ]
	cmp		al, '9'
	ja		ok_cur
	mov		ah, '~'
ok_cur:
	shl		eax, 16
	mov		ax, 5B1Bh
	xor		ebx, ebx
	jmp		put_queue
	
IFDEF		KBD_FR
num_table	BYTE	'789 456 1230.'
ELSE
num_table	BYTE	'789 456 1230,'
ENDIF
cur_table	BYTE	'HA5 DGC YB623'

; this routine handles function keys

func:
	push	eax
	push	ecx
	push	edx
	call	show_stat
	pop		edx
	pop		ecx
	pop		eax
	sub		al, 3Bh
	jb		end_func
	cmp		al, 9
	jb		ok_func
	sub		al, 18
	cmp		al, 10
	jb		end_func
	cmp		al, 11
	ja		end_func
ok_func:
	cmp		ecx, 4			; check that there is enough room
	jl		end_func
	mov		eax, func_table[ eax*4 ]
	xor		ebx, ebx
	jmp		put_queue
end_func:
	ret

; function keys send F1:'esc [ [ A' F2:'esc [ [ B' etc.

func_table	DWORD	415B5B1Bh,425B5B1Bh,435B5B1Bh,445B5B1Bh
			DWORD	455B5B1Bh,465B5B1Bh,475B5B1Bh,485B5B1Bh
			DWORD	495b5b1bh,4a5b5b1bh,4b5b5b1bh,4c5b5b1bh
	
IFDEF		KBD_FINNISH

key_map		BYTE	0,27
			BYTE	'1234567890+'''
			BYTE	127,9
			BYTE	'qwertyuiop}'
			BYTE	0,13,0
			BYTE	'asdfghjkl|{'
			BYTE	0,0
			BYTE	'''zxcvbnm,.-'
			BYTE	0,'*',0,32		; 36-39
			BYTE	16 DUP (0)		; 3A-49
			BYTE	'-',0,0,0,'+'	; 4A-4E
			BYTE	0,0,0,0,0,0,0	; 4F-55
			BYTE	'<'
			BYTE	10 DUP (0)

shift_map	BYTE	0,27
			BYTE	'!"#$%&/()=?`'
			BYTE	127,9
			BYTE	'QWERTYUIOP ]^'
			BYTE	13,0
			BYTE	'ASDFGHJKL\[ '
			BYTE	0,0
			BYTE	'*ZXCVBNM;:_'
			BYTE	0,'*',0,32		; 36-39
			BYTE	16 DUP (0)		; 3A-49
			BYTE	'-',0,0,0,'+'	; 4A-4E
			BYTE	0,0,0,0,0,0,0	; 4F-55
			BYTE	'>'
			BYTE	10 DUP (0)
			
alt_map		BYTE	0,0
			BYTE	0,'@',0,'$',0,0,'{[]}\',0
			BYTE	0,0
			BYTE	0,0,0,0,0,0,0,0,0,0,0
			BYTE	'~',13,0
			BYTE	0,0,0,0,0,0,0,0,0,0,0
			BYTE	0,0
			BYTE	0,0,0,0,0,0,0,0,0,0,0
			BYTE	0,0,0,0			; 36-39
			BYTE	16 DUP (0)		; 3A-49
			BYTE	0,0,0,0,0		; 4A-4E
			BYTE	0,0,0,0,0,0,0	; 4F-55
			BYTE	'|'
			BYTE	10 DUP (0)

ELSEIFDEF	KBD_US

key_map		BYTE	0,27
			BYTE	'1234567890-='
			BYTE	127,9
			BYTE	'qwertyuiop[]'
			BYTE	13,0
			BYTE	'asdfghjkl;'''
			BYTE	'`',0
			BYTE	'\zxcvbnm,./'
			BYTE	0,'*',0,32		; 36-39
			BYTE	16 DUP (0)		; 3A-49
			BYTE	'-',0,0,0,'+'	; 4A-4E
			BYTE	0,0,0,0,0,0,0	; 4F-55
			BYTE	'<'
			BYTE	10 DUP (0)

shift_map	BYTE	0,27
			BYTE	'!@#$%^&*()_+'
			BYTE	127,9
			BYTE	'QWERTYUIOP{}'
			BYTE	13,0
			BYTE	'ASDFGHJKL:"'
			BYTE	'~',0
			BYTE	'|ZXCVBNM<>?'
			BYTE	0,'*',0,32		; 36-39
			BYTE	16 DUP (0)		; 3A-49
			BYTE	'-',0,0,0,'+'	; 4A-4E
			BYTE	0,0,0,0,0,0,0	; 4F-55
			BYTE	'>'
			BYTE	10 DUP (0)
			
alt_map		BYTE	0,0
			BYTE	0,'@',0,'$',0,0,'{[]}\',0
			BYTE	0,0
			BYTE	0,0,0,0,0,0,0,0,0,0,0
			BYTE	'~',13,0
			BYTE	0,0,0,0,0,0,0,0,0,0,0
			BYTE	0,0
			BYTE	0,0,0,0,0,0,0,0,0,0,0
			BYTE	0,0,0,0			; 36-39
			BYTE	16 DUP (0)		; 3A-49
			BYTE	0,0,0,0,0		; 4A-4E
			BYTE	0,0,0,0,0,0,0	; 4F-55
			BYTE	'|'
			BYTE	10 DUP (0)

ELSEIFDEF	KBD_GR

key_map		BYTE	0,27
			BYTE	'1234567890\'''
			BYTE	127,9
			BYTE	'qwertzuiop@+'
			BYTE	13,0
			BYTE	'asdfghjkl[]^'
			BYTE	0,'#'
			BYTE	'yxcvbnm,.-'
			BYTE	0,'*',0,32		; 36-39
			BYTE	16 DUP (0)		; 3A-49
			BYTE	'-',0,0,0,'+'	; 4A-4E
			BYTE	0,0,0,0,0,0,0	; 4F-55
			BYTE	'<'
			BYTE	10 DUP (0)

shift_map	BYTE	0,27
			BYTE	'!"#$%&/()=?`'
			BYTE	127,9
			BYTE	'QWERTZUIOP\*'
			BYTE	13,0
			BYTE	'ASDFGHJKL{}~'
			BYTE	0,''''
			BYTE	'YXCVBNM;:_'
			BYTE	0,'*',0,32		; 36-39
			BYTE	16 DUP (0)		; 3A-49
			BYTE	'-',0,0,0,'+'	; 4A-4E
			BYTE	0,0,0,0,0,0,0	; 4F-55
			BYTE	'>'
			BYTE	10 DUP (0)
			
alt_map		BYTE	0,0
			BYTE	0,'@',0,'$',0,0,'{[]}\',0
			BYTE	0,0
			BYTE	'@',0,0,0,0,0,0,0,0,0,0
			BYTE	'~',13,0
			BYTE	0,0,0,0,0,0,0,0,0,0,0
			BYTE	0,0
			BYTE	0,0,0,0,0,0,0,0,0,0,0
			BYTE	0,0,0,0			; 36-39
			BYTE	16 DUP (0)		; 3A-49
			BYTE	0,0,0,0,0		; 4A-4E
			BYTE	0,0,0,0,0,0,0	; 4F-55
			BYTE	'|'
			BYTE	10 DUP (0)

ELSEIFDEF	KBD_FR

key_map		BYTE	0,27
			BYTE	'&{"''(-}_/@)='
			BYTE	127,9
			BYTE	'azertyuiop^$'
			BYTE	13,0
			BYTE	'qsdfghjklm|'
			BYTE	'`',0,42		; coin sup gauche, don't know, [ *|mu ]
			BYTE	'wxcvbn,;:!'
			BYTE	0,'*',0,32		; 36-39
			BYTE	16 DUP (0)		; 3A-49
			BYTE	'-',0,0,0,'+'	; 4A-4E
			BYTE	0,0,0,0,0,0,0	; 4F-55
			BYTE	'<'
			BYTE	10 DUP (0)

shift_map	BYTE	0,27
			BYTE	'1234567890 ]+'
			BYTE	127,9
			BYTE	'AZERTYUIOP<>'
			BYTE	13,0
			BYTE	'QSDFGHJKLM%'
			BYTE	'~',0,'#'
			BYTE	'WXCVBN?./\'
			BYTE	0,'*',0,32		; 36-39
			BYTE	16 DUP (0)		; 3A-49
			BYTE	'-',0,0,0,'+'	; 4A-4E
			BYTE	0,0,0,0,0,0,0	; 4F-55
			BYTE	'>'
			BYTE	10 DUP (0)
			
alt_map		BYTE	0,0
			BYTE	0,'~#{[ |`\^@ ]}'
			BYTE	0,0
			BYTE	'@',0,0,0,0,0,0,0,0,0,0
			BYTE	'~',13,0
			BYTE	0,0,0,0,0,0,0,0,0,0,0
			BYTE	0,0
			BYTE	0,0,0,0,0,0,0,0,0,0,0
			BYTE	0,0,0,0			; 36-39
			BYTE	16 DUP (0)		; 3A-49
			BYTE	0,0,0,0,0		; 4A-4E
			BYTE	0,0,0,0,0,0,0	; 4F-55
			BYTE	'|'
			BYTE	10 DUP (0)

ELSE

.err		'KBD-type not defined'

ENDIF

; do_self handles "normal" keys, ie keys that don't change meaning
; and which have just one character returns.

do_self:
	lea		ebx, alt_map
	test	mode, 20h		; alt-gr
	jne		@F
	lea		ebx, shift_map
	test	mode, 03h
	jne		@F
	lea		ebx, key_map
@@:
	mov		al, [ ebx+eax ]
	or		al, al
	je		none
	test	mode, 4Ch		; ctrl or caps
	je		@F
	cmp		al, 'a'
	jb		@F
	cmp		al, '}'
	ja		@F
	sub		al, 32
@@:
	test	mode, 0Ch		; ctrl
	je		@F
	cmp		al, 64
	jb		@F
	cmp		al, 64+32
	jae		@F
	sub		al, 64
@@:
	test	mode, 10h		; left alt
	je		@F
	or		al, 80h
@@:
	and		eax, 0FFh
	xor		ebx, ebx
	call	put_queue
none:
	ret
	
; minus has a routine of it's own, as a 'E0h' before
; the scan code for minus means that the numeric keypad
; slash was pushed.
minus:
	cmp		e0, 1
	jne		do_self
	mov		eax, '/'
	xor		ebx, ebx
	jmp		put_queue

; This table decides which routine to call when a scan-code has been
; gotten. Most routines just call do_self, or none, depending if
; they are make or break.

key_table	DWORD none,do_self,do_self,do_self		; 00-03 s0 esc 1 2
			DWORD do_self,do_self,do_self,do_self	; 04-07 3 4 5 6
			DWORD do_self,do_self,do_self,do_self	; 08-0B 7 8 9 0
			DWORD do_self,do_self,do_self,do_self	; 0C-0F + ' bs tab
			DWORD do_self,do_self,do_self,do_self	; 10-13 q w e r
			DWORD do_self,do_self,do_self,do_self	; 14-17 t y u i
			DWORD do_self,do_self,do_self,do_self	; 18-1B o p } ^
			DWORD do_self,ctrl,do_self,do_self		; 1C-1F enter ctrl a s
			DWORD do_self,do_self,do_self,do_self	; 20-23 d f g h
			DWORD do_self,do_self,do_self,do_self	; 24-27 j k l |
			DWORD do_self,do_self,lshift,do_self	; 28-2B { para lshift ,
			DWORD do_self,do_self,do_self,do_self	; 2C-2F z x c v
			DWORD do_self,do_self,do_self,do_self	; 30-33 b n m ,
			DWORD do_self,minus,rshift,do_self		; 34-37 . - rshift *
			DWORD alt,do_self,caps,func				; 38-3B alt sp caps f1
			DWORD func,func,func,func				; 3C-3F f2 f3 f4 f5
			DWORD func,func,func,func				; 40-43 f6 f7 f8 f9
			DWORD func,num,scroll,cursor			; 44-47 f10 num scr home
			DWORD cursor,cursor,do_self,cursor		; 48-4B up pgup - left
			DWORD cursor,cursor,do_self,cursor		; 4C-4F n5 right + end
			DWORD cursor,cursor,cursor,cursor		; 50-53 dn pgdn ins del
			DWORD none,none,do_self,func			; 54-57 sysreq ? < f11
			DWORD func,none,none,none				; 58-5B f12 ? ? ?
			DWORD none,none,none,none				; 5C-5F ? ? ? ?
			DWORD none,none,none,none				; 60-63 ? ? ? ?
			DWORD none,none,none,none				; 64-67 ? ? ? ?
			DWORD none,none,none,none				; 68-6B ? ? ? ?
			DWORD none,none,none,none				; 6C-6F ? ? ? ?
			DWORD none,none,none,none				; 70-73 ? ? ? ?
			DWORD none,none,none,none				; 74-77 ? ? ? ?
			DWORD none,none,none,none				; 78-7B ? ? ? ?
			DWORD none,none,none,none				; 7C-7F ? ? ? ?
			DWORD none,none,none,none				; 80-83 ? br br br
			DWORD none,none,none,none				; 84-87 br br br br
			DWORD none,none,none,none				; 88-8B br br br br
			DWORD none,none,none,none				; 8C-8F br br br br
			DWORD none,none,none,none				; 90-93 br br br br
			DWORD none,none,none,none				; 94-97 br br br br
			DWORD none,none,none,none				; 98-9B br br br br
			DWORD none,unctrl,none,none				; 9C-9F br unctrl br br
			DWORD none,none,none,none				; A0-A3 br br br br
			DWORD none,none,none,none				; A4-A7 br br br br
			DWORD none,none,unlshift,none			; A8-AB br br unlshift br
			DWORD none,none,none,none				; AC-AF br br br br
			DWORD none,none,none,none				; B0-B3 br br br br
			DWORD none,none,unrshift,none			; B4-B7 br br unrshift br
			DWORD unalt,none,uncaps,none			; B8-BB unalt br uncaps br
			DWORD none,none,none,none				; BC-BF br br br br
			DWORD none,none,none,none				; C0-C3 br br br br
			DWORD none,none,none,none				; C4-C7 br br br br
			DWORD none,none,none,none				; C8-CB br br br br
			DWORD none,none,none,none				; CC-CF br br br br
			DWORD none,none,none,none				; D0-D3 br br br br
			DWORD none,none,none,none				; D4-D7 br br br br
			DWORD none,none,none,none				; D8-DB br ? ? ?
			DWORD none,none,none,none				; DC-DF ? ? ? ?
			DWORD none,none,none,none				; E0-E3 e0 e1 ? ?
			DWORD none,none,none,none				; E4-E7 ? ? ? ?
			DWORD none,none,none,none				; E8-EB ? ? ? ?
			DWORD none,none,none,none				; EC-EF ? ? ? ?
			DWORD none,none,none,none				; F0-F3 ? ? ? ?
			DWORD none,none,none,none				; F4-F7 ? ? ? ?
			DWORD none,none,none,none				; F8-FB ? ? ? ?
			DWORD none,none,none,none				; FC-FF ? ? ? ?
	
; kb_wait waits for the keyboard controller buffer to empty.
; there is no timeout - if the buffer doesn't empty, we hang.

kb_wait:
	push	eax
@@:
	in		al, 64h
	test	al, 02h
	jne		@B
	pop		eax
	ret
	
; This routine reboots the machine by asking the keyboard
; controller to pulse the reset-line low.

reboot:
	call	kb_wait
	mov		WORD PTR ds:[ 472h ], 1234h
	mov		al, 0FCh
	out		64h, al
die:
	jmp		die
	
END