#include <sys\types.h>
extern __inline UCHAR get_fs_byte( const CHAR *addr )
{
	register UCHAR _v;

	__asm mov	edi, addr
	__asm mov	al, fs :[ edi ];
	__asm mov	_v, al

	return _v;
}

extern __inline USHORT get_fs_word( const USHORT *addr )
{
	USHORT _v;

	__asm mov	edi, addr
	__asm mov	ax, fs :[ edi ];
	__asm mov	_v, ax

	return _v;
}

extern __inline ULONG get_fs_long( const ULONG *addr )
{
	ULONG _v;

	__asm mov	edi, addr
	__asm mov	eax, fs :[ edi ];
	__asm mov	_v, eax

	return _v;
}

extern __inline VOID put_fs_byte( CHAR val, CHAR *addr )
{
	__asm mov	al, val
	__asm mov	edi, addr
	__asm mov	fs : [ edi ], al
}

extern __inline VOID put_fs_word( short val, short * addr )
{
	__asm mov	ax, val
	__asm mov	edi, addr
	__asm mov	fs : [ edi ], ax
}

extern __inline VOID put_fs_long( ULONG val, ULONG * addr )
{
	__asm mov	eax, val
	__asm mov	edi, addr
	__asm mov	fs : [ edi ], eax
}

/*
* Someone who knows GNU asm better than I should double check the followig.
* It seems to work, but I don't know if I'm doing something subtly wrong.
* --- TYT, 11/24/91
* [ nothing wrong here, Linus ]
*/

extern __inline ULONG get_fs()
{
	USHORT _v;

	__asm mov	ax, fs
	__asm mov	_v, ax

	return _v;
}

extern __inline ULONG get_ds()
{
	USHORT _v;

	__asm mov	ax, ds
	__asm mov	_v, ax

	return _v;
}

extern __inline VOID set_fs( ULONG val )
{
	__asm mov	eax, val
	__asm mov	fs, ax
}
