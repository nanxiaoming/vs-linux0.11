#ifndef _STRING_H_
#define _STRING_H_

#include <sys/types.h>

#ifndef NULL
#define NULL ( ( VOID * )0 )
#endif

#ifndef _SIZE_T
#define _SIZE_T
typedef unsigned int size_t;
#endif

extern CHAR * strerror( int errno );

/*
* This string-include defines all string functions as inline
* functions. Use gcc. It also assumes ds=es=data space, this should be
* normal. Most of the string-functions are rather heavily hand-optimized,
* see especially strtok,strstr,str[ c ]spn. They should work, but are not
* very easy to understand. Everything is done entirely within the register
* set, making the functions fast and clean. String instructions have been
* used through-out, making for "slightly" unclear code :- )
*
*		( C ) 1991 Linus Torvalds
*/

extern __inline CHAR *strcpy( CHAR *dest, const CHAR *src )
{
	__asm mov	edi, dest
	__asm mov	esi, src
	__asm cld
LN1 :
	__asm lodsb
	__asm stosb
	__asm test	al, al
	__asm jne	LN1

	return dest;
}

extern __inline CHAR *strncpy( CHAR *dest, const CHAR *src, LONG count )
{
	__asm mov	edi, dest
	__asm mov	esi, src
	__asm mov	ecx, count
	__asm cld
LN1 :
	__asm dec	ecx
	__asm js	LN2
	__asm lodsb
	__asm stosb
	__asm test	al, al
	__asm jne	LN1
	__asm rep	stosb
LN2 :

	return dest;
}

extern __inline LONG strcmp( const CHAR *s1, const CHAR *s2 )
{
	register LONG __res;

	__asm mov	edi, s1
	__asm mov	esi, s2
	__asm cld
LN1 :
	__asm lodsb
	__asm scasb
	__asm jne	LN2
	__asm test	al, al
	__asm jne	LN1
	__asm xor	eax, eax
	__asm jmp	LN3
LN2 :
	__asm mov	eax, 1
	__asm jl	LN3
	__asm neg	eax
LN3 :
	__asm mov	__res, eax

	return __res;
}

extern __inline CHAR *strchr( const CHAR *s, CHAR c )
{
	register CHAR * __res;

	__asm mov	al, c
	__asm mov	esi, s
	__asm cld
	__asm mov	ah, al
LN1 :
	__asm lodsb
	__asm cmp	al, ah
	__asm je	LN2
	__asm test	al, al
	__asm jne	LN1
	__asm mov	esi, 1
LN2 :
	__asm mov	eax, esi
	__asm dec	eax
	__asm mov	__res, eax

	return __res;
}

extern __inline LONG strlen( const CHAR * s )
{
	register LONG __res;
	__asm mov	ecx, -1
	__asm mov	edi, s
	__asm xor	eax, eax
	__asm cld
	__asm repne	scasb
	__asm not	ecx
	__asm dec	ecx
	__asm mov	__res, ecx

	return __res;
}

extern __inline VOID * memset( VOID * s, CHAR c, LONG count )
{
	__asm mov	edi, s
	__asm mov	al, c
	__asm mov	ecx, count
	__asm cld
	__asm rep	stosb

	return s;
}

#endif
