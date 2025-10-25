static __inline VOID outb( UCHAR value, USHORT port )
{
	__asm mov	al, value
	__asm mov	dx, port
	__asm out	dx, al
}

static __inline UCHAR inb( USHORT port )
{
	UCHAR _v;

	__asm mov	dx, port
	__asm in	al, dx
	__asm mov	_v, al

	return _v;
}

static __inline VOID outb_p( UCHAR value, USHORT port )
{
	__asm mov	al, value
	__asm mov	dx, port
	__asm out	dx, al
	__asm jmp	LN1
LN1 :
	__asm jmp	LN2
LN2 :
	;
}

static __inline UCHAR inb_p( USHORT port )
{
	UCHAR _v;

	__asm mov	dx, port
	__asm in	al, dx
	__asm jmp	LN1
LN1 :
	__asm jmp	LN2
LN2 :
	__asm mov	_v, al

	return _v;
}