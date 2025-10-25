#ifndef _HEAD_H
#define _HEAD_H

#include <sys\types.h>

typedef struct desc_struct
{
	ULONG a;
	ULONG b;
}Desc_Struct;

extern ULONG pg_dir[ 1024 ];
extern Desc_Struct idt[256];
extern Desc_Struct gdt[256];


#define GDT_NUL 0
#define GDT_CODE 1
#define GDT_DATA 2
#define GDT_TMP 3

#define LDT_NUL 0
#define LDT_CODE 1
#define LDT_DATA 2

#endif
