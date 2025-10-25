#ifndef _SYS_TYPES_H
#define _SYS_TYPES_H

#ifndef _SIZE_T
#define _SIZE_T
typedef unsigned int size_t;
#endif

#ifndef _TIME_T
#define _TIME_T
typedef long time_t;
#endif

#ifndef _PTRDIFF_T
#define _PTRDIFF_T
typedef long ptrdiff_t;
#endif

typedef unsigned char	UCHAR;
typedef unsigned short	USHORT;
typedef unsigned long	ULONG;
typedef int				INT;
typedef long			LONG;
typedef void			VOID;
typedef char			CHAR;

#ifndef NULL
#define NULL ( ( VOID * ) 0 )
#endif

typedef LONG    pid_t;
typedef USHORT	uid_t;
typedef UCHAR	gid_t;
typedef USHORT	dev_t;
typedef USHORT	ino_t;
typedef USHORT	mode_t;
typedef USHORT	umode_t;
typedef UCHAR	nlink_t;
typedef LONG	daddr_t;
typedef ULONG	off_t;
typedef UCHAR	u_char;
typedef USHORT	ushort;

typedef struct { LONG quot, rem; } div_t;
typedef struct { LONG quot, rem; } ldiv_t;

struct ustat 
{
	daddr_t f_tfree;			// Offset=0x0 Size=0x4
	ino_t	f_tinode;			// Offset=0x4 Size=0x2
	CHAR	f_fname[ 6 ];		// Offset=0x6 Size=0x6
	CHAR	f_fpack[ 6 ];		// Offset=0xc Size=0x6
};

#endif
