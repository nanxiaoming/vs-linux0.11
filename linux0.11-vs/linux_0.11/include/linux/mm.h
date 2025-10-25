#ifndef _MM_H
#define _MM_H

#include <sys\types.h>

#define PAGE_SIZE 4096

extern ULONG get_free_page();
extern ULONG put_page( ULONG page, ULONG address );
extern VOID free_page( ULONG addr );

#endif
