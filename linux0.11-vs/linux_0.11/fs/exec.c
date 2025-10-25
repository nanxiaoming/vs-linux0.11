/*
 *  linux/fs/exec.c
 *
 *  ( C ) 1991  Linus Torvalds
 */

/*
 * #!-checking implemented by tytso.
 */

/*
 * Demand-loading implemented 01.12.91 - no need to read anything but
 * the header into memory. The inode of the executable is put into
 * "current->executable", and page faults do the actual loading. Clean.
 *
 * Once more I can proudly say that linux stood up to being changed: it
 * was less than 2 hours work to get demand-loading completely implemented.
 */

/*
 * ����ʱ��������1991.12.1 ʵ�ֵ�- ֻ�轫ִ���ļ�ͷ���ֶ����ڴ������
 * ������ִ���ļ������ؽ��ڴ�.ִ���ļ���i �ڵ㱻���ڵ�ǰ���̵Ŀ�ִ���ֶ���
 * ( "current-.h>executable" ),��ҳ�쳣�����ִ���ļ���ʵ�ʼ��ز����Լ�������.
 *
 * �ҿ�����һ���Ժ���˵,linux �������޸�:ֻ���˲���2 Сʱ�Ĺ���ʱ�����ȫ
 * ʵ����������ش���.
 */
#include <errno.h>
#include <string.h>
#include <sys\stat.h>
#include <a.out.h>

#include <linux\fs.h>
#include <linux\sched.h>
#include <linux\kernel.h>
#include <linux\mm.h>
#include <asm\segment.h>

extern int sys_exit( LONG exit_code );
extern int sys_close( LONG fd );

/*
 * MAX_ARG_PAGES defines the number of pages allocated for arguments
 * and envelope for the new program. 32 should suffice, this gives
 * a maximum env+arg of 128kB !
 */
/*
 * MAX_ARG_PAGES �������³������������ͻ�������ʹ�õ��ڴ����ҳ��.
 * 32 ҳ�ڴ�Ӧ���㹻��,��ʹ�û����Ͳ���( env+arg )�ռ���ܺϴﵽ128kB!
 */
#define MAX_ARG_PAGES 32

/*
 * create_tables() parses the env- and arg-strings in new user
 * memory and creates the pointer tables from them, and puts their
 * addresses on the "stack", returning the new stack pointer value.
 */
/*
 * create_tables()���������û��ڴ��н������������Ͳ����ַ���,�ɴ�
 * ����ָ���,�������ǵĵ�ַ�ŵ�"��ջ"��,Ȼ�󷵻���ջ��ָ��ֵ.
 */

// �����û���ջ�д��������Ͳ�������ָ���.
// ����:p - �����ݶ�Ϊ���Ĳ����ͻ�����Ϣƫ��ָ��;argc - ��������;envc -����������.
// ����:��ջָ��
static ULONG * create_tables( CHAR * p, LONG argc, LONG envc )
{
	ULONG *argv, *envp;
	ULONG * sp;

	// ��ջָ������4 �ֽ�( 1 �� )Ϊ�߽�Ѱַ��,���������sp Ϊ4 ��������
	sp = ( ULONG * )( 0xfffffffc & ( ULONG )p );

	// sp �����ƶ�,�ճ���������ռ�õĿռ����,���û�������ָ��envp ָ��ô�
	sp -= envc + 1;

	envp = sp;

	// sp �����ƶ�,�ճ������в���ָ��ռ�õĿռ����,����argv ָ��ָ��ô�.
	// ����ָ���1,sp ������ָ�����ֽ�ֵ.
	sp -= argc + 1;
	argv = sp;

	// ����������ָ��envp �������в���ָ���Լ������в�������ѹ���ջ
	put_fs_long( ( ULONG )envp, --sp );
	put_fs_long( ( ULONG )argv, --sp );
	put_fs_long( ( ULONG )argc, --sp );

	// �������и�����ָ�����ǰ��ճ�������Ӧ�ط�,������һ��NULL ָ��
	while ( argc-- > 0 ) 
	{
		put_fs_long( ( ULONG )p, argv++ );
		while ( get_fs_byte( p++ ) ) /* nothing */;
	}
	put_fs_long( 0, argv );
	// ������������ָ�����ǰ��ճ�������Ӧ�ط�,������һ��NULL ָ��
	while ( envc-- > 0 ) 
	{
		put_fs_long( ( ULONG )p, envp++ );
		while ( get_fs_byte( p++ ) ) /* nothing */;
	}

	put_fs_long( 0, envp );

	return sp;		// ���ع���ĵ�ǰ�¶�ջָ��
}

/*
 * count() counts the number of arguments/envelopes
 */

// �����������.
// ����:argv - ����ָ������,���һ��ָ������NULL.
// ����:��������.
static LONG count( CHAR ** argv )
{
	LONG		i = 0;
	CHAR	**	tmp;

	if ( tmp = argv )
	{
		while ( get_fs_long( ( ULONG * )( tmp++ ) ) )
		{
			i++;
		}
	}
	return i;
}

/*
 * 'copy_string()' copies argument/envelope strings from user
 * memory to free pages in kernel mem. These are in a format ready
 * to be put directly into the top of new user memory.
 *
 * Modified by TYT, 11/24/91 to add the from_kmem argument, which specifies
 * whether the string and the string array are from user or kernel segments:
 *
 * from_kmem     argv *        argv **
 *    0          user space    user space
 *    1          kernel space  user space
 *    2          kernel space  kernel space
 *
 * We do this by playing games with the fs segment register.  Since it
 * it is expensive to load a segment register, we try to avoid calling
 * set_fs() unless we absolutely have to.
 */
/*
 * 'copy_string()'�������û��ڴ�ռ俽�������ͻ����ַ������ں˿���ҳ���ڴ���.
 * ��Щ�Ѿ���ֱ�ӷŵ����û��ڴ��еĸ�ʽ.
 *
 * ��TYT( Tytso )�� 1991.12.24 ���޸�,������ from_kmem ����,�ò���ָ�����ַ�����
 * �ַ��������������û��λ����ں˶�.
 * 
 * from_kmem     argv *        argv **
 *    0          �û��ռ�      �û��ռ�
 *    1          �ں˿ռ�      �û��ռ�
 *    2          �ں˿ռ�      �ں˿ռ�
 * 
 * ������ͨ�������fs �μĴ�����������.���ڼ���һ���μĴ�������̫��,����
 * ���Ǿ����������set_fs(),����ʵ�ڱ�Ҫ.
 */
//// ����ָ�������Ĳ����ַ����������ͻ����ռ�.
// ����:argc - ����ӵĲ�������;argv - ����ָ������;page - �����ͻ����ռ�ҳ��ָ������.
//       p -�ڲ�����ռ��е�ƫ��ָ��,ʼ��ָ���Ѹ��ƴ���ͷ��;from_kmem - �ַ�����Դ��־.
// ��do_execve()������,p ��ʼ��Ϊָ�������( 128kB )�ռ�����һ�����ִ�,�����ַ���
// ���Զ�ջ������ʽ���������и��ƴ�ŵ�,���p ָ���ʼ��ָ������ַ�����ͷ��.
// ����:�����ͻ����ռ䵱ǰͷ��ָ��.
static ULONG 
copy_strings( 
	LONG		argc, 
	CHAR	**	argv, 
	ULONG	*	page,
	ULONG		p, 
	LONG		from_kmem )
{
	CHAR *		tmp, *pag;
	LONG		len, offset = 0;
	ULONG		old_fs, new_fs;

	if ( !p )
	{
		return 0;	/* bullet-proofing */
	}

	// ȡds �Ĵ���ֵ��new_fs,������ԭfs �Ĵ���ֵ��old_fs.

	new_fs = get_ds();
	old_fs = get_fs();

	// ����ַ������ַ������������ں˿ռ�,������fs �μĴ���ָ���ں����ݶ�( ds )
	if ( from_kmem == 2 )
	{
		set_fs( new_fs );
	}

	// ѭ�������������,�����һ����������ʼ����,���Ƶ�ָ��ƫ�Ƶ�ַ��
	while ( argc-- > 0 )
	{
		// ����ַ������û��ռ���ַ����������ں˿ռ�,������fs �μĴ���ָ���ں����ݶ�( ds ).
		if ( from_kmem == 1 )
		{
			set_fs( new_fs );
		}
		// �����һ��������ʼ�������,ȡfs �������һ����ָ�뵽tmp,���Ϊ��,���������.
		if ( !( tmp = ( CHAR * )get_fs_long( ( ( ULONG * )argv ) + argc ) ) )
		{
			panic( "argc is wrong" );
		}
		// ����ַ������û��ռ���ַ����������ں˿ռ�,��ָ�fs �μĴ���ԭֵ
		if ( from_kmem == 1 )
		{
			set_fs( old_fs );
		}

		// ����ò����ַ�������len,��ʹtmp ָ��ò����ַ���ĩ��
		len = 0;		/* remember zero-padding */

		do 
		{
			len++;

		} while ( get_fs_byte( tmp++ ) );

		// ������ַ������ȳ�����ʱ�����ͻ����ռ��л�ʣ��Ŀ��г���,��ָ� fs �μĴ���������0

		if ( p - len < 0 ) 
		{	/* this shouldn't happen - 128kB ���ᷢ��-��Ϊ��128kB �Ŀռ�*/
			set_fs( old_fs );
			return 0;
		}

		// ����fs ���е�ǰָ���Ĳ����ַ���,�ǴӸ��ַ���β����ʼ����
		while ( len ) 
		{
			--p; --tmp; --len;
			// �����տ�ʼִ��ʱ,ƫ�Ʊ���offset ����ʼ��Ϊ0,�����offset-1<0,˵�����״θ����ַ���,
			// ���������p ָ����ҳ���ڵ�ƫ��ֵ,���������ҳ��
			if ( --offset < 0 ) 
			{
				offset = p % PAGE_SIZE;
				// ����ַ������ַ����������ں˿ռ�,��ָ�fs �μĴ���ԭֵ
				if ( from_kmem == 2 )
				{
					set_fs( old_fs );
				}
				// �����ǰƫ��ֵp ���ڵĴ��ռ�ҳ��ָ��������page[ p/PAGE_SIZE ]==0,��ʾ��Ӧҳ�滹������,
				// ���������µ��ڴ����ҳ��,����ҳ��ָ������ָ������,����Ҳʹpag ָ�����ҳ��,�����벻
				// ������ҳ���򷵻�0
				if ( !( pag = ( CHAR * )page[ p / PAGE_SIZE ] ) &&
					 !( pag = ( CHAR * )page[ p / PAGE_SIZE ] = ( CHAR * )get_free_page() ) 
					)
				{
					return 0;
				}
				// ����ַ������ַ������������ں˿ռ�,������fs �μĴ���ָ���ں����ݶ�( ds )
				if ( from_kmem == 2 )
				{
					set_fs( new_fs );
				}
			}
			// ��fs ���и��Ʋ����ַ�����һ�ֽڵ�pag+offset ��
			*( pag + offset ) = get_fs_byte( tmp );
		}
	}

	// ����ַ������ַ����������ں˿ռ�,��ָ� fs �μĴ���ԭֵ
	if ( from_kmem == 2 )
	{
		set_fs( old_fs );
	}

	// ���,���ز����ͻ����ռ����Ѹ��Ʋ�����Ϣ��ͷ��ƫ��ֵ
	return p;
}

// �޸ľֲ����������е���������ַ�Ͷ��޳�,���������ͻ����ռ�ҳ����������ݶ�ĩ��.
// ����:text_size - ִ���ļ�ͷ����a_text �ֶθ����Ĵ���γ���ֵ;
//       page - �����ͻ����ռ�ҳ��ָ������.
// ����:���ݶ��޳�ֵ( 64MB ).
static ULONG change_ldt( ULONG text_size, ULONG * page )
{
	ULONG code_limit, data_limit, code_base, data_base;
	LONG i;

	// ����ִ���ļ�ͷ��a_text ֵ,������ҳ�泤��Ϊ�߽�Ĵ�����޳�.���������ݶγ���Ϊ64MB.
	code_limit	= text_size + PAGE_SIZE - 1;
	code_limit &= 0xFFFFF000;
	data_limit	= 0x4000000;

	// ȡ��ǰ�����оֲ��������������������д���λ�ַ,����λ�ַ�����ݶλ�ַ��ͬ
	code_base = get_base( current->ldt[ 1 ] );
	data_base = code_base;

	// �������þֲ����д���κ����ݶ��������Ļ�ַ�Ͷ��޳�
	set_base	( current->ldt[ 1 ], code_base	);
	set_limit	( current->ldt[ 1 ], code_limit );
	set_base	( current->ldt[ 2 ], data_base	);
	set_limit	( current->ldt[ 2 ], data_limit );
	/* make sure fs points to the NEW data segment */
	//Ҫȷ��fs �μĴ�����ָ���µ����ݶ� */
	// fs �μĴ����з���ֲ������ݶ���������ѡ���( 0x17 ).

	__asm push	0x17
	__asm pop	fs

	data_base += data_limit;

	for ( i = MAX_ARG_PAGES - 1; i >= 0; i-- ) 
	{
		data_base -= PAGE_SIZE;

		if ( page[ i ] )						// �����ҳ�����,
		{
			put_page( page[ i ], data_base );	// �ͷ��ø�ҳ��.
		}
	}
	return data_limit;						// ��󷵻����ݶ��޳�( 64MB )		
}

LONG 
do_execve( 
	ULONG	*	eip		,
	LONG		tmp		,
	CHAR	*	filename,
	CHAR	**	argv	,
	CHAR	**	envp	)
/*++

Routine Description:

	execve()ϵͳ�жϵ��ú���.���ز�ִ���ӽ���( �������� )

	���õ��˴���ջ����ͼ

	  Stack layout in 'ret_from_system_call':


	  | STACK 0x00ffbfcc[0x00ffbfec] (<unknown>)			--> ʵ������ջ�е�eip�ĵ�ַ	,Ϊ�ú����ĵ�һ������
	  | STACK 0x00ffbfd0[0x000065da](system_call + 26)		--> sys_call ������ call	sys_call_table[ eax*4 ] ������õ���һ��ָ���ַ,�˴���tmp����,������
	  | STACK 0x00ffbfd4[0x00012608](write_verify + 1487)	--> Ӧ�ò����úõļĴ��� filename
	  | STACK 0x00ffbfd8[0x00014048](argv_rc + 0)			--> Ӧ�ò����úõļĴ��� argv_rc �ĵ�ַ
	  | STACK 0x00ffbfdc[0x00014050](envp_rc + 0)			-->	Ӧ�ò����úõļĴ��� envp_rc �ĵ�ַ
	  | STACK 0x00ffbfe0[0x00000017](<unknown>)				--> %fs
	  | STACK 0x00ffbfe4[0x00010017](die + 37)				-->	%es
	  | STACK 0x00ffbfe8[0x00000017](<unknown>)				-->	%ds
	  | STACK 0x00ffbfec[0x00010a58](execve + 15)			-->	%eip
	  | STACK 0x00ffbff0[0x0000000f](<unknown>)				-->	%cs
	  | STACK 0x00ffbff4[0x00000246](<unknown>)				-->	%eflags
	  | STACK 0x00ffbff8[0x0001ad6c](user_stack + fac)		-->	%oldesp
	  | STACK 0x00ffbffc[0x00000017](<unknown>)				-->	%oldss
	  | STACK 0x00ffc000[0x00000000](<unknown>)				-->


	���º궨�幹������ͼ�Ĳ����Ĵ�����Ϣ,ע��ջ�Ƿ����,ʹ�üĴ������ݵĲ���
	#define _syscall3( type,name,atype,a,btype,b,ctype,c ) \
			type name( atype a, btype b, ctype c ) \
			{ \
			int __res; \
			{ \
			__asm mov	eax, __NR_##name \	--> ���������� 0xb
			__asm mov	ebx, a \			--> filename
			__asm mov	ecx, b \			--> argv_rc
			__asm mov	edx, c \			-->	envp_rc
			__asm int	0x80 \
			__asm mov	__res, eax \
			} \
			if ( __res >= 0 ) \
			return ( type )__res; \
			errno = -__res; \
			return -1; \
			}

Arguments:

	eip			- ջ�ϵ�Ӧ�ò�eip��ַ,��ֵ��cpu�Զ���ջ,��sys_call��ջ�еĵ�ַȡ��������ú���
	tmp			- ������,�� call do_execve ����һ��ָ���ַ
	filename	- Ҫִ�еĳ���,��һ�ֿ�ִ�е��ļ���ʽ,ͷ����Ϣ��a.out.h�ж���
	argv		- �����в���ָ������;
	envp		- ��������ָ������.

Return Value:

	�ɹ�,�򷵻�Ӧ�ò��oepִ��
	ʧ�����ó����,������-1.

--*/
{
	M_Inode			*	inode;						// �ڴ���I �ڵ�ָ��ṹ����.
	Buffer_Head		*	bh;							// ���ٻ����ͷָ��.
	struct exec			ex;							// ִ���ļ�ͷ�����ݽṹ����.
	ULONG				page[ MAX_ARG_PAGES ];		// �����ͻ����ַ����ռ��ҳ��ָ������.
	LONG				i, argc, envc;
	LONG				e_uid, e_gid;				// ��Ч�û�id ����Ч��id.
	LONG				retval;						// ����ֵ.
	LONG				sh_bang = 0;				// �����Ƿ���Ҫִ�нű��������.

	// �����ͻ����ַ����ռ��е�ƫ��ָ��,��ʼ��Ϊָ��ÿռ�����һ�����ִ�

	ULONG				p = PAGE_SIZE*MAX_ARG_PAGES - 4;

	// eip[ 1 ]����ԭ����μĴ���cs,���е�ѡ������������ں˶�ѡ���,Ҳ���ں˲��ܵ��ñ�����

	if ( ( 0xffff & eip[ 1 ] ) != 0x000f )
	{
		panic( "execve called from supervisor mode" );
	}

	// ��ʼ�������ͻ������ռ��ҳ��ָ������( �� )
	for ( i = 0; i < MAX_ARG_PAGES; i++ )	/* clear page-table */
	{
		page[ i ] = 0;
	}

	// ȡ��ִ���ļ��Ķ�Ӧ i �ڵ��
	if ( !( inode = namei( filename ) ) )		/* get executables inode */
	{
		return -ENOENT;
	}

	// ������������ͻ�����������
	argc = count( argv );
	envc = count( envp );

	// ִ���ļ������ǳ����ļ�.�����ǳ����ļ����ó�������,��ת��exec_error2( ��347 �� )
restart_interp:

	if ( !S_ISREG( inode->i_mode ) ) /* must be regular file */
	{	
		retval = -EACCES;
		goto exec_error2;
	}

	// ��鱻ִ���ļ���ִ��Ȩ��.����������( ��Ӧ i �ڵ�� uid �� gid ),���������Ƿ���Ȩִ����.
	i = inode->i_mode;

	e_uid = ( i & S_ISUID ) ? inode->i_uid : current->euid;
	e_gid = ( i & S_ISGID ) ? inode->i_gid : current->egid;

	if ( current->euid == inode->i_uid )
	{
		i >>= 6;
	}
	else if ( current->egid == inode->i_gid )
	{
		i >>= 3;
	}
	if ( !( i & 1 ) && !( ( inode->i_mode & 0111 ) && suser() ) ) 
	{	
		retval = -ENOEXEC;
		goto exec_error2;
	}

	// ��ȡִ���ļ��ĵ�һ�����ݵ����ٻ�����,���������ó�����,��ת��exec_error2 ��ȥ����.

	if ( !( bh = bread( inode->i_dev, inode->i_zone[ 0 ] ) ) ) 
	{
		retval = -EACCES;
		goto exec_error2;
	}

	// �����ִ���ļ���ͷ�ṹ���ݽ��д��� , ������ ex ָ��ִ��ͷ���ֵ����ݽṹ
	ex = *( ( struct exec * ) bh->b_data );	/* read exec-header */

	// ���ִ���ļ���ʼ�������ֽ�Ϊ'#!',���� sh_bang ��־û����λ,����ű��ļ���ִ��
	if ( ( bh->b_data[ 0 ] == '#' ) && ( bh->b_data[ 1 ] == '!' ) && ( !sh_bang ) ) 
	{
		/*
		 * This section does the #! interpretation.
		 * Sorta complicated, but hopefully it will work.  -TYT
		 */
		/*
		 * �ⲿ�ִ����'#!'�Ľ���,��Щ����,��ϣ���ܹ���.-TYT
		 */
		CHAR buf[ 1023 ], *cp, *interp, *i_name, *i_arg;
		ULONG old_fs;

		// ����ִ�г���ͷһ���ַ�'#!'������ַ�����buf ��,���к��нű����������
		strncpy( buf, bh->b_data + 2, 1022 );
		// �ͷŸ��ٻ����͸�ִ���ļ�i �ڵ�
		brelse( bh );
		iput( inode );

		// ȡ��һ������,��ɾ����ʼ�Ŀո��Ʊ��
		buf[ 1022 ] = '\0';

		if ( cp = strchr( buf, '\n' ) ) 
		{
			*cp = '\0';
			for ( cp = buf; ( *cp == ' ' ) || ( *cp == '\t' ); cp++ );
		}

		// ������û����������,�����.�ó�����,��ת��exec_error1 ��

		if ( !cp || *cp == '\0' ) 
		{
			retval = -ENOEXEC; /* No interpreter name found */
			goto exec_error1;
		}

		// ����͵õ��˿�ͷ�ǽű�����ִ�г������Ƶ�һ������
		interp = i_name = cp;

		// �����������.����ȡ��һ���ַ���,��Ӧ���ǽű����ͳ�����,iname ָ�������
		i_arg = 0;

		for ( ; *cp && ( *cp != ' ' ) && ( *cp != '\t' ); cp++ ) 
		{
			if ( *cp == '/' )
			{
				i_name = cp + 1;
			}
		}

		// ���ļ��������ַ�,��Ӧ���ǲ�����,��i_arg ָ��ô�
		if ( *cp ) 
		{
			*cp++ = '\0';
			i_arg = cp;
		}

		/*
		 * OK, we've parsed out the interpreter name and
		 * ( optional ) argument.
		 */

		/*
		 * OK,�����Ѿ����������ͳ�����ļ����Լ�( ��ѡ�� )����.
		 */

		// �� sh_bang ��־û������,��������,������ָ�������Ļ����������Ͳ������������ͻ����ռ���
		if ( sh_bang++ == 0 ) 
		{
			p = copy_strings(	envc, envp		, page, p , 0 );
			p = copy_strings( --argc, argv + 1	, page, p , 0 );
		}

		/*
		 * Splice in ( 1 ) the interpreter's name for argv[ 0 ]
		 *           ( 2 ) ( optional ) argument to interpreter
		 *           ( 3 ) filename of shell script
		 *
		 * This is done in reverse order, because of how the
		 * user environment and arguments are stored.
		 */

		/*
		 * ƴ�� 
		 *		( 1 ) argv[ 0 ]�зŽ��ͳ��������
		 *      ( 2 ) ( ��ѡ�� )���ͳ���Ĳ���
		 *      ( 3 ) �ű����������
		 *
		 * ������������д����,�������û������Ͳ����Ĵ�ŷ�ʽ��ɵ�.
		 */

		// ���ƽű������ļ����������ͻ����ռ���.

		p = copy_strings( 1, &filename, page, p, 1 );

		// ���ƽ��ͳ���Ĳ����������ͻ����ռ���
		argc++;

		if ( i_arg ) 
		{
			p = copy_strings( 1, &i_arg, page, p, 2 );
			argc++;
		}

		// ���ƽ��ͳ����ļ����������ͻ����ռ���.������,���ó�����,��ת��exec_error1

		p = copy_strings( 1, &i_name, page, p, 2 );

		argc++;

		if ( !p ) 
		{
			retval = -ENOMEM;
			goto exec_error1;
		}

		/*
		 * OK, now restart the process with the interpreter's inode.
		 */

		/*
		 * OK,����ʹ�ý��ͳ����i �ڵ���������.
		 */
		// ����ԭfs �μĴ���( ԭָ���û����ݶ� ),������ָ���ں����ݶ�

		old_fs = get_fs();

		set_fs( get_ds() );

		// ȡ���ͳ����i �ڵ�,����ת��restart_interp �����´���

		if ( !( inode = namei( interp ) ) )  /* get executables inode */
		{ 
			set_fs( old_fs );
			retval = -ENOENT;
			goto exec_error1;
		}

		set_fs( old_fs );

		goto restart_interp;
	}

	brelse( bh );

	// �����ִ��ͷ��Ϣ���д���.
	// �����������,����ִ�г���:���ִ���ļ���������ҳ��ִ���ļ�( ZMAGIC )�����ߴ����ض�λ����
	// ����a_trsize ������0�����������ض�λ��Ϣ���Ȳ�����0�����ߴ����+���ݶ�+�Ѷγ��ȳ���50MB��
	// ����i �ڵ�����ĸ�ִ���ļ�����С�ڴ����+���ݶ�+���ű���+ִ��ͷ���ֳ��ȵ��ܺ�.

	if (
		N_MAGIC( ex ) != ZMAGIC || 
		ex.a_trsize || 
		ex.a_drsize ||
		ex.a_text + ex.a_data + ex.a_bss > 0x3000000 ||
		inode->i_size < ex.a_text + ex.a_data + ex.a_syms + N_TXTOFF( ex ) 
		) 
	{
		retval = -ENOEXEC;
		goto exec_error2;
	}

	// ���ִ���ļ�ִ��ͷ���ֳ��Ȳ�����һ���ڴ���С( 1024 �ֽ� ),Ҳ����ִ��.תexec_error2.
	if ( N_TXTOFF( ex ) != BLOCK_SIZE ) 
	{
		printk( "%s: N_TXTOFF != BLOCK_SIZE. See a.out.h.", filename );
		retval = -ENOEXEC;
		goto exec_error2;
	}
	// ��� sh_bang ��־û������,����ָ�������Ļ��������ַ����Ͳ����������ͻ����ռ���.
	// �� sh_bang ��־�Ѿ�����,������ǽ����нű�����,��ʱ��������ҳ���Ѿ�����,�����ٸ���.

	if ( !sh_bang ) 
	{
		p = copy_strings( envc, envp, page, p, 0 );
		p = copy_strings( argc, argv, page, p, 0 );

		// ���p=0,���ʾ��������������ռ�ҳ���Ѿ���ռ��,���ɲ�����.ת��������

		if ( !p ) 
		{
			retval = -ENOMEM;
			goto exec_error2;
		}
	}

	/* OK, This is the point of no return */
	/* OK,���濪ʼ��û�з��صĵط��� */
	// ���ԭ����Ҳ��һ��ִ�г���,���ͷ���i �ڵ�,���ý���executable �ֶ�ָ���³���i �ڵ�.

	if ( current->executable )
	{
		iput( current->executable );
	}

	current->executable = inode;

	// �帴λ�����źŴ�����.������SIG_IGN ������ܸ�λ,�����322 ��323 ��֮�������һ��
	// if ���:if ( current->sa[ I ].sa_handler != SIG_IGN ).����Դ�����е�һ��bug.

	for ( i = 0; i < 32; i++ )
	{
		current->sigaction[ i ].sa_handler = NULL;
	}

	// ����ִ��ʱ�ر�( close_on_exec )�ļ����λͼ��־,�ر�ָ���Ĵ��ļ�,����λ�ñ�־.
	for ( i = 0; i < NR_OPEN; i++ )
	{
		if ( ( current->close_on_exec >> i ) & 1 )
		{
			sys_close( i );
		}
	}

	current->close_on_exec = 0;

	// ����ָ���Ļ���ַ���޳�,�ͷ�ԭ���������κ����ݶ�����Ӧ���ڴ�ҳ��ָ�����ڴ�鼰ҳ����

	free_page_tables( get_base( current->ldt[ 1 ] ), get_limit( 0x0f ) );
	free_page_tables( get_base( current->ldt[ 2 ] ), get_limit( 0x17 ) );

	// ���"�ϴ�����ʹ����Э������"ָ����ǵ�ǰ����,�����ÿ�,����λʹ����Э�������ı�־.
	if ( last_task_used_math == current )
	{
		last_task_used_math = NULL;
	}

	current->used_math = 0;

	// ����a_text �޸ľֲ�������������ַ�Ͷ��޳�,���������ͻ����ռ�ҳ����������ݶ�ĩ��.
	// ִ���������֮��,p ��ʱ�������ݶ���ʼ��Ϊԭ���ƫ��ֵ,��ָ������ͻ����ռ����ݿ�ʼ��,
	// Ҳ��ת����Ϊ��ջ��ָ��.

	p += change_ldt( ex.a_text, page ) - MAX_ARG_PAGES*PAGE_SIZE;

	// create_tables()�����û���ջ�д��������Ͳ�������ָ���,�����ظö�ջָ��

	p = ( ULONG )create_tables( ( CHAR * )p, argc, envc );

	//
	// �޸ĵ�ǰ���̸��ֶ�Ϊ��ִ�г������Ϣ.
	// end_code = a_text;				���̴����βֵ�ֶ�
	// end_data = a_data + a_text	    �������ݶ�β�ֶ� 
	// brk = a_text + a_data + a_bss.   ���̶ѽ�β�ֶ�   
	//

	current->brk = ex.a_bss +
					( current->end_data = ex.a_data + ( current->end_code = ex.a_text ) );
							
	// ���ý��̶�ջ��ʼ�ֶ�Ϊ��ջָ�����ڵ�ҳ��,���������ý��̵��û�id ����id

	current->start_stack	= p & 0xfffff000;
	current->euid			= (USHORT)e_uid;
	current->egid			= (USHORT)e_gid;
	i						= ex.a_text + ex.a_data;

	while ( i & 0xfff )
	{
		put_fs_byte( 0, ( CHAR * )( i++ ) );
	}

	// ��ԭ����ϵͳ�жϵĳ����ڶ�ջ�ϵĴ���ָ���滻Ϊָ����ִ�г������ڵ�,������ջָ���滻
	// Ϊ��ִ�г���Ķ�ջָ��.����ָ�������Щ��ջ���ݲ�ʹ��CPU ȥִ���µ�ִ�г���,��˲���
	// ���ص�ԭ����ϵͳ�жϵĳ�����ȥ��.
	eip[ 0 ] = ex.a_entry;			/* eip, magic happens :- ) eip,ħ���������� :- )*/
	eip[ 3 ] = p;					/* stack pointer esp,��ջָ�� */
	return 0;

exec_error2:

	iput( inode );

exec_error1:

	for ( i = 0; i < MAX_ARG_PAGES; i++ )
	{
		free_page( page[ i ] );
	}
	return( retval );
}
