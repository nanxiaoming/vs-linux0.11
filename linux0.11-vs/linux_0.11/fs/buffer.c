/*
*  linux/fs/buffer.c
*
*  ( C ) 1991  Linus Torvalds
*/

/*
 * 'buffer.c' implements the buffer-cache functions. Race-conditions have
 * been avoided by NEVER letting a interrupt change a buffer ( except for the
 * data, of course ), but instead letting the caller do it. NOTE! As interrupts
 * can wake up a caller, some cli-sti sequences are needed to check for
 * sleep-on-calls. These should be extremely quick, though ( I hope ).
 *
 *
 * NOTE! There is one discordant note here: checking floppies for
 * disk change. This is where it fits best, I think, as it should
 * invalidate changed floppy-disk-caches.
 * 
 * 'buffer.c'����ʵ�ֻ��������ٻ��湦��.ͨ�������жϹ��̸ı仺����,�����õ�����
 * ��ִ��,�����˾�������( ��Ȼ���ı��������� ).ע�⣡�����жϿ��Ի���һ��������,
 * ��˾���Ҫ�����ж�ָ��( cli-sti )���������ȴ����÷���.����Ҫ�ǳ��ؿ�( ϣ�������� ).
 * 
 * ע�⣡������һ������Ӧ����������:��������Ƿ����.������������
 * ���øó�����õĵط���,��Ϊ����Ҫʹ�Ѹ������̻���ʧЧ.
 * 
 */

#include <stdarg.h>

#include <linux\config.h>
#include <linux\sched.h>
#include <linux\kernel.h>
#include <asm\system.h>
#include <asm\io.h>

extern	LONG			_end;	//�����ӳ��� ld ���ɵ�λ�ڳ���ĩ�˵ı���
		Buffer_Head *	start_buffer = ( Buffer_Head * ) &_end;
		Buffer_Head *	hash_table[ NR_HASH ];
static	Buffer_Head *	free_list;
static	Task_Struct *	buffer_wait = NULL;
		LONG			NR_BUFFERS = 0;

// �ȴ�ָ������������
static __inline VOID wait_on_buffer( Buffer_Head * bh )
{
	cli();

	while ( bh->b_lock )
	{
		sleep_on( &bh->b_wait );
	}

	sti();
}

LONG sys_sync()
{
	LONG			i;
	Buffer_Head *	bh;

	sync_inodes();

	// �� i �ڵ�д����ٻ���
	// ɨ�����и��ٻ�����,�����ѱ��޸ĵĻ�������д������,���������������豸��ͬ��

	bh = start_buffer;

	for ( i = 0; i < NR_BUFFERS; i++, bh++ )
	{
		wait_on_buffer( bh );

		if ( bh->b_dirt )
		{
			ll_rw_block( WRITE, bh );
		}
	}
	return 0;
}

LONG sync_dev( LONG dev )
/*++

Routine Description:

	��ָ���豸���и��ٻ����������豸�����ݵ�ͬ������

Arguments:

	dev - �豸��

Return Value:
	
	0 - �ɹ�

--*/
{
	LONG			i;
	Buffer_Head *	bh;

	bh = start_buffer;

	for ( i = 0; i < NR_BUFFERS; i++, bh++ )
	{
		if ( bh->b_dev != dev )
		{
			continue;
		}

		if ( bh->b_dev == dev && bh->b_dirt )
		{
			ll_rw_block( WRITE, bh );
		}

		if ( bh->b_dev == dev && bh->b_dirt )
		{
			ll_rw_block( WRITE, bh );
		}
	}

	sync_inodes();

	bh = start_buffer;

	for ( i = 0; i < NR_BUFFERS; i++, bh++ ) 
	{
		if ( bh->b_dev != dev )
		{
			continue;
		}

		wait_on_buffer( bh );

		if ( bh->b_dev == dev && bh->b_dirt )
		{
			ll_rw_block( WRITE, bh );
		}
	}
	return 0;
}

VOID __inline invalidate_buffers( LONG dev )
/*++

Routine Description:

	ʹָ���豸�ڸ��ٻ������е�������Ч
	ɨ����ٻ����е����л����,����ָ���豸�Ļ�����,��λ����Ч( ���� )��־�����޸ı�־.

Arguments:

	dev - �豸��

Return Value:

	VOID

--*/
{
	LONG			i;
	Buffer_Head *	bh;

	bh = start_buffer;

	for ( i = 0; i < NR_BUFFERS; i++, bh++ ) 
	{
		if ( bh->b_dev != dev )
		{
			continue;
		}

		wait_on_buffer( bh );

		if ( bh->b_dev == dev )
		{
			bh->b_uptodate = bh->b_dirt = 0;
		}
	}
}

extern VOID put_super		 ( LONG dev );
extern VOID invalidate_inodes( LONG dev );

VOID check_disk_change( LONG dev )
/*++

Routine Description:

	�������Ƿ����,����Ѹ�����ʹ��Ӧ���ٻ�������Ч.

	ԭʼע��

	This routine checks whether a floppy has been changed, and
	invalidates all buffer-cache-entries in that case. This
	is a relatively slow routine, so we have to try to minimize using
	it. Thus it is called only upon a 'mount' or 'open'. This
	is the best way of combining speed and utility, I think.
	People changing diskettes in the middle of an operation deserve
	to loose :- )

	NOTE! Although currently this is only for floppies, the idea is
	that any additional removable block-device will use this routine,
	and that mount/open needn't know that floppies/whatever are
	special.

	���ӳ�����һ�������Ƿ��Ѿ�������,����Ѿ�������ʹ���ٻ������������
	��Ӧ�����л�������Ч.���ӳ��������˵����,��������Ҫ������ʹ����.
	���Խ���ִ��'mount'��'open'ʱ�ŵ�����.�������ǽ��ٶȺ�ʵ�������ϵ�
	��÷���.���ڲ������̵��и�������,�ᵼ�����ݵĶ�ʧ,���Ǿ�����ȡ :- )

	ע�⣡����Ŀǰ���ӳ������������,�Ժ��κο��ƶ����ʵĿ��豸����ʹ�ø�
	����,mount/open �����ǲ���Ҫ֪���Ƿ������̻�����ʲô������ʵ�.

Arguments:

	dev - �豸��

Return Value:

	-

--*/
{
	LONG i;

	if ( MAJOR( dev ) != 2 )
	{
		return;
	}

	if ( !floppy_change( dev & 0x03 ) )
	{
		return;
	}

	// �����Ѿ�����,�����ͷŶ�Ӧ�豸�� i �ڵ�λͼ���߼���λͼ��ռ�ĸ��ٻ�����;
	// ��ʹ���豸�� i �ڵ�����ݿ���Ϣ��ռ�ĸ��ٻ�������Ч
	for ( i = 0; i < NR_SUPER; i++ )
	{
		if ( super_block[ i ].s_dev == dev )
		{
			put_super( super_block[ i ].s_dev );
		}
	}

	invalidate_inodes	( dev );
	invalidate_buffers	( dev );
}

// hash ������ hash ����ļ���궨��

#define _hashfn( dev,block ) ( ( ( unsigned )( dev^block ) )%NR_HASH )
#define hash( dev,block ) hash_table[ _hashfn( dev,block ) ]

static __inline VOID remove_from_queues( Buffer_Head * bh )
/*++

Routine Description:

	�� hash ���кͿ��л������������ָ���Ļ����

Arguments:

	bh - Buffser_Head ͷ

Return Value:

	VOID

--*/
{
	/* ˫�������Ƴ����� */
	if ( bh->b_next )
	{
		bh->b_next->b_prev = bh->b_prev;
	}

	if ( bh->b_prev )
	{
		bh->b_prev->b_next = bh->b_next;
	}

	// ����û������Ǹö��е�ͷһ����,���� hash ��Ķ�Ӧ��ָ�򱾶����е���һ��������
	if ( hash( bh->b_dev, bh->b_blocknr ) == bh )
	{
		hash( bh->b_dev, bh->b_blocknr ) = bh->b_next;
	}

	/* remove from free list �ӿ��л����������Ƴ������  */
	if ( !( bh->b_prev_free ) || !( bh->b_next_free ) )
	{
		panic( "Free block list corrupted" );
	}

	bh->b_prev_free->b_next_free = bh->b_next_free;
	bh->b_next_free->b_prev_free = bh->b_prev_free;

	// �����������ͷָ�򱾻�����,������ָ����һ������
	if ( free_list == bh )
	{
		free_list = bh->b_next_free;
	}
}

static __inline VOID insert_into_queues( Buffer_Head * bh )
/*++

Routine Description:

	��ָ�������������������β������ hash ������

Arguments:

	bh - Buffer_Head

Return Value:

	VOID

--*/
{
	bh->b_next_free						= free_list;
	bh->b_prev_free						= free_list->b_prev_free;
	free_list->b_prev_free->b_next_free = bh;
	free_list->b_prev_free				= bh;

	/* put the buffer in new hash-queue if it has a device */
	//����û�����Ӧһ���豸,���������hash ������
	bh->b_prev = NULL;
	bh->b_next = NULL;

	if ( !bh->b_dev )
	{
		return;
	}

	bh->b_next							= hash( bh->b_dev, bh->b_blocknr );
	hash( bh->b_dev, bh->b_blocknr )	= bh;
	bh->b_next->b_prev					= bh;
}

static Buffer_Head * find_buffer( LONG dev, LONG block )
/*++

Routine Description:

	�ڸ��ٻ�����Ѱ�Ҹ����豸��ָ����Ļ�������.
	����ҵ��򷵻ػ��������ָ��.

Arguments:

	dev		- �豸��
	block	- ���

Return Value:

	�ɹ�����Buffer_Head,ʧ�ܷ���NULL 

--*/
{
	Buffer_Head * tmp;

	for ( tmp = hash( dev, block ); tmp != NULL; tmp = tmp->b_next )
	{
		if ( tmp->b_dev == dev && tmp->b_blocknr == block )
		{
			return tmp;
		}
	}
	return NULL;
}


Buffer_Head * get_hash_table( LONG dev, LONG block )
/*++

Routine Description:

	get_hash_table ��HashTable�л�ȡһ��Buffer_Head , ���������豸�Ϳ��

	ԭʼע��
	Why like this, I hear you say... The reason is race-conditions.
	As we don't lock buffers ( unless we are readint them, that is ),
	something might happen to it while we sleep ( ie a read-error
	will force it bad ). This shouldn't really happen currently, but
	the code is ready.

Arguments:

	dev		- �豸��
	block	- ���

Return Value:

	NULL - ʧ��

--*/
{
	Buffer_Head * bh;

	for ( ;; ) 
	{
		// �ڸ��ٻ�����Ѱ�Ҹ����豸��ָ����Ļ�����,���û���ҵ��򷵻�NULL,�˳�
		if ( !( bh = find_buffer( dev, block ) ) )
		{
			return NULL;
		}

		// �Ըû������������ü���,���ȴ��û���������( ����ѱ����� )
		bh->b_count++;

		wait_on_buffer( bh );

		// ���ھ�����˯��״̬,����б�Ҫ����֤�û����������ȷ��,�����ػ�����ͷָ��
		if ( bh->b_dev == dev && bh->b_blocknr == block )
		{
			return bh;
		}
		// ����û������������豸�Ż�����˯��ʱ�����˸ı�,�������������ü���,����Ѱ��
		bh->b_count--;
	}
}

#define BADNESS( bh ) ( ( ( bh )->b_dirt<<1 )+( bh )->b_lock )

Buffer_Head * getblk( LONG dev, LONG block )
/*++

Routine Description:

	getblk
	ȡ���ٻ�����ָ���Ļ�����.
	�����ָ���Ļ������Ƿ��Ѿ��ڸ��ٻ�����,�������,����Ҫ�ڸ��ٻ����н���һ����Ӧ������.
	������Ӧ������ͷָ��.

Arguments:

	dev	  - �豸��
	block - ���

Return Value:

	Buffer_Head ָ��

--*/
{
	Buffer_Head * tmp, *bh;

repeat:

	// ���� hash ��,���ָ�����Ѿ��ڸ��ٻ�����,�򷵻ض�Ӧ������ͷָ��,�˳�.
	if ( bh = get_hash_table( dev, block ) )
	{
		return bh;
	}

	// ɨ��������ݿ�����,Ѱ�ҿ��л�����.
	// ������tmp ָ���������ĵ�һ�����л�����ͷ.

	tmp = free_list;

	do 
	{
		// ����û���������ʹ��( ���ü���������0 ),�����ɨ����һ��
		if ( tmp->b_count )
		{
			continue;
		}

		// �������ͷָ�� bh Ϊ��,���� tmp ��ָ����ͷ�ı�־( �޸ġ����� )Ȩ��С�� bh ͷ��־��Ȩ��,
		// ���� bh ָ��� tmp ������ͷ.����� tmp ������ͷ������������û���޸�Ҳû��������־��λ,
		// ��˵����Ϊָ���豸�ϵĿ�ȡ�ö�Ӧ�ĸ��ٻ�����,���˳�ѭ��

		if ( !bh || BADNESS( tmp ) < BADNESS( bh ) ) 
		{
			bh = tmp;
			
			if ( !BADNESS( tmp ) )
			{
				break;
			}
		}
		//�ظ�����ֱ���ҵ��ʺϵĻ�����
		/* and repeat until we find something good */
	} while ( ( tmp = tmp->b_next_free ) != free_list );

	// ������л�����������ʹ��( ���л�������ͷ�����ü�����>0 ),
	// ��˯��,�ȴ��п��еĻ���������
	if ( !bh ) 
	{
		sleep_on( &buffer_wait );
		goto repeat;
	}

	// �ȴ��û���������( ����ѱ������Ļ� )
	wait_on_buffer( bh );

	// ����û������ֱ���������ʹ�õĻ�,ֻ���ظ���������
	if ( bh->b_count )
	{
		goto repeat;
	}
	// ����û������ѱ��޸�,������д��,���ٴεȴ�����������.����û������ֱ���������ʹ��
	// �Ļ�,ֻ�����ظ���������
	while ( bh->b_dirt ) 
	{
		sync_dev( bh->b_dev );
		wait_on_buffer( bh );

		if ( bh->b_count )
		{
			goto repeat;
		}
	}

	// ע�⣡��������Ϊ�˵ȴ��û�����˯��ʱ,�������̿����Ѿ����û���� 
	// ��������ٻ�����,����Ҫ�Դ˽��м��. 
	// �ڸ��ٻ���hash ���м��ָ���豸�Ϳ�Ļ������Ƿ��Ѿ��������ȥ.����ǵĻ�,���ٴ��ظ�
	// ��������.
	/* NOTE!! While we slept waiting for this block, somebody else might */
	/* already have added "this" block to the cache. check it */

	if ( find_buffer( dev, block ) )
	{
		goto repeat;
	}

	/* OK, FINALLY we know that this buffer is the only one of it's kind, */
	/* and that it's unused ( b_count=0 ), unlocked ( b_lock=0 ), and clean */
	// OK,��������֪���û�������ָ��������Ψһһ��, */
	// ���һ�û�б�ʹ��( b_count=0 ),δ������( b_lock=0 ),�����Ǹɾ���( δ���޸ĵ� ) */
	// ����������ռ�ô˻�����.�����ü���Ϊ1,��λ�޸ı�־����Ч( ���� )��־.

	bh->b_count		= 1;
	bh->b_dirt		= 0;
	bh->b_uptodate	= 0;

	// �� hash ���кͿ��п��������Ƴ��û�����ͷ,�øû���������ָ���豸�����ϵ�ָ����.
	remove_from_queues( bh );

	bh->b_dev		= (USHORT)dev;
	bh->b_blocknr	= block;

	// Ȼ����ݴ��µ��豸�źͿ�����²����������� hash ������λ�ô�.�����շ��ػ���ͷָ��
	insert_into_queues( bh );

	return bh;
}

// �ͷ�ָ���Ļ�����.
// �ȴ��û���������.���ü����ݼ�1.���ѵȴ����л������Ľ���

VOID brelse( Buffer_Head * buf )
{
	if ( !buf )		// �������ͷָ����Ч�򷵻�
	{
		return;
	}

	wait_on_buffer( buf );

	if ( !( buf->b_count-- ) )
	{
		panic( "Trying to free free buffer" );
	}

	wake_up( &buffer_wait );
}

/*
 * bread() reads a specified block and returns the buffer that contains
 * it. It returns NULL if the block was unreadable.
 */


Buffer_Head * bread( LONG dev, LONG block )
/*++

Routine Description:

	���豸�϶�ȡָ�������ݿ鲢���غ������ݵĻ�����.���ָ���Ŀ鲻����
	�򷵻�NULL.

Arguments:

	dev		- �豸��
	block	- ���

Return Value:

	NULL - ʧ��,���򷵻�Buffer_Head�ṹ

--*/
{
	Buffer_Head * bh;

	// �ڸ��ٻ���������һ�黺����.�������ֵ�� NULL ָ��,��ʾ�ں˳���,����.
	if ( !( bh = getblk( dev, block ) ) )
	{
		panic( "bread: getblk returned NULL\n" );
	}

	// ����û������е���������Ч��( �Ѹ��µ� )����ֱ��ʹ��,�򷵻�
	if ( bh->b_uptodate )
	{
		return bh;
	}

	// �������ll_rw_block()����,�������豸������.���ȴ�����������
	ll_rw_block( READ, bh );

	wait_on_buffer( bh );

	// ����û������Ѹ���,�򷵻ػ�����ͷָ��,�˳�
	if ( bh->b_uptodate )
	{
		return bh;
	}
	// ����������豸����ʧ��,�ͷŸû�����,����NULL ָ��,�˳�
	brelse( bh );

	return NULL;
}
// �����ڴ��.
// �� from ��ַ����һ�����ݵ� to λ��
static __inline VOID COPYBLK( ULONG from, ULONG to )
{
	__asm mov	ecx, BLOCK_SIZE / 4
	__asm mov	esi, from
	__asm mov	edi, to
	__asm cld
	__asm rep	movsd
}

VOID bread_page( ULONG address, LONG dev, LONG b[ 4 ] )
/*++

Routine Description:

	bread_page 

	һ�ζ��ĸ���������ݶ����ڴ�ָ���ĵ�ַ,һ��ҳ���С.����һ�������ĺ���,
	��Ϊͬʱ��ȡ�Ŀ���Ի���ٶ��ϵĺô�,���õ��Ŷ�һ��,�ٶ�һ����.

Arguments:

	address - ���ݴ洢�ڸõ�ַ
	dev		- �豸��
	b		- ��������,Buffer_Headͷ

Return Value:

	VOID -

--*/
{
	Buffer_Head *	bh[ 4 ];
	LONG			i;

	// ѭ��ִ��4 ��,��һҳ����
	for ( i = 0; i < 4; i++ )
	{
		if ( b[ i ] ) 
		{
			// ȡ���ٻ�����ָ���豸�Ϳ�ŵĻ�����,����û�����������Ч��������豸����
			if ( bh[ i ] = getblk( dev, b[ i ] ) )
			{
				if ( !bh[ i ]->b_uptodate )
				{
					ll_rw_block( READ, bh[ i ] );
				}
			}
		}
		else
		{
			bh[ i ] = NULL;
		}
	}

	// ��4 �黺�����ϵ�����˳���Ƶ�ָ����ַ��
	for ( i = 0; i < 4; i++, address += BLOCK_SIZE )
	{
		if ( bh[ i ] ) 
		{
			wait_on_buffer( bh[ i ] ); // �ȴ�����������( ����ѱ������Ļ� ).

			if ( bh[ i ]->b_uptodate ) // ����û�������������Ч�Ļ�,����
			{
				COPYBLK( ( ULONG )bh[ i ]->b_data, address );
			}
			brelse( bh[ i ] );
		}
	}
}


Buffer_Head * breada( LONG dev, LONG first, ... )
/*++

Routine Description:

	breada
	bread һ��ʹ��.aΪ�ɱ����

Arguments:

	dev		- �豸��
	first	- ���

Return Value:

	 1 - �ɹ�ʱ���صڿ�Ļ�����ͷָ��
	 0 - ʧ��

--*/
{
	va_list				args;
	Buffer_Head		*	bh, *tmp;

	// ȡ�ɱ�������е� 1 ������( ��� )
	va_start( args, first );

	// ȡ���ٻ�����ָ���豸�Ϳ�ŵĻ�����.����û�����������Ч,�򷢳����豸���ݿ�����
	if ( !( bh = getblk( dev, first ) ) )
	{
		panic( "bread: getblk returned NULL\n" );
	}

	if ( !bh->b_uptodate )
	{
		ll_rw_block( READ, bh );
	}

	// Ȼ��˳��ȡ�ɱ������������Ԥ�����,����������ͬ������,��������
	while ( ( first = va_arg( args, LONG ) ) >= 0 )
	{
		tmp = getblk( dev, first );

		if ( tmp ) 
		{
			if ( !tmp->b_uptodate )
			{
				ll_rw_block( READA , bh );
			}
			tmp->b_count--;
		}
	}
	// �ɱ�����������в����������.�ȴ��� 1 ������������( ����ѱ����� )
	va_end( args );
	wait_on_buffer( bh );

	// �����������������Ч,�򷵻ػ�����ͷָ��,�˳�.
	if ( bh->b_uptodate )
	{
		return bh;
	}

	brelse( bh );
	return NULL;
}

// 
// 
// 

VOID buffer_init( LONG end_buffer )
/*++

Routine Description:

	��������ʼ������.

	�ú����ǽ�ÿһ�� 'ͷ��' (�洢��HASH_TAB ) ��ʼ��,��ָ��ÿһ��Body.

	����ڴ濪ʼ��kernel��β(_end)��2M����.

	�����ж��ٸ�Body? �� ' ( b -= BLOCK_SIZE ) >= ( ( CHAR* )( h + 1 ) ) ' ����.

	���� end_buffer ��ָ���Ļ������ڴ��ĩ��.����ϵͳ��16MB �ڴ�,�򻺳���ĩ������Ϊ4MB.
	����ϵͳ��8MB �ڴ�,������ĩ������Ϊ2MB

Arguments:

	end_buffer - �����ĩβ

Return Value:

	VOID

--*/
{
	Buffer_Head		*	h = start_buffer;
	CHAR			*	b;
	LONG				i;

	// ����������߶˵���1Mb,�����ڴ�640KB-1MB ����ʾ�ڴ��BIOS ռ��,���ʵ�ʿ��û������ڴ�
	// �߶�(ʵ�ʵ�end_buffer)Ӧ����640KB.�����ڴ�߶�һ������1MB

	if ( end_buffer == 1 << 20 )
	{
		b = ( VOID * )( 640 * 1024 );	
	}
	else
	{
		b = ( VOID * )end_buffer;
	}
	// ��δ������ڳ�ʼ��������,�������л�����������,����ȡϵͳ�л�������Ŀ.
	// �����Ĺ����Ǵӻ������߶˿�ʼ���� 1K ��С�Ļ����,���ͬʱ�ڻ������Ͷ˽��������û����
	// �Ľṹ buffer_head,������Щ buffer_head ���˫������.
	// h ��ָ�򻺳�ͷ�ṹ��ָ��,�� h+1 ��ָ���ڴ��ַ��������һ������ͷ��ַ,Ҳ����˵��ָ��h
	// ����ͷ��ĩ����.Ϊ�˱�֤���㹻���ȵ��ڴ����洢һ������ͷ�ṹ,��Ҫ b ��ָ����ڴ��
	// ��ַ >= h ����ͷ��ĩ��,Ҳ��Ҫ >=h+1

	while ( ( b -= BLOCK_SIZE ) >= ( ( CHAR* )( h + 1 ) ) )
	{
		h->b_dev		= 0;				// ʹ�øû��������豸��.
		h->b_dirt		= 0;				// ���־,Ҳ���������޸ı�־.
		h->b_count		= 0;				// �û��������ü���.
		h->b_lock		= 0;				// ������������־.
		h->b_uptodate	= 0;				// ���������±�־( ���������Ч��־ ).
		h->b_wait		= NULL;				// ָ��ȴ��û����������Ľ���.
		h->b_next		= NULL;				// ָ�������ͬ hash ֵ����һ������ͷ.
		h->b_prev		= NULL;				// ָ�������ͬ hash ֵ��ǰһ������ͷ.
		h->b_data		= ( CHAR* )b;		// ָ���Ӧ���������ݿ�( 1024 �ֽ� ).
		h->b_prev_free	= h - 1;			// ָ��������ǰһ��.
		h->b_next_free	= h + 1;			// ָ����������һ��.

		h++;								// h ָ����һ�»���ͷλ��.

		NR_BUFFERS++;						// �����������ۼ�.

		if ( b == ( VOID * )0x100000 )		// �����ַb �ݼ�������1MB,������384KB,
		{
			b = ( VOID * )0xA0000;			// �� b ָ���ַ0xA0000( 640KB )��.
		}
	}

	h--;									// ��h ָ�����һ����Ч����ͷ
	
	free_list				= start_buffer;	// �ÿ�������ͷָ��ͷһ��������ͷ.
	free_list->b_prev_free	= h;			// ����ͷ�� b_prev_free ָ��ǰһ��( �����һ�� )
	h->b_next_free			= free_list;	// h ����һ��ָ��ָ���һ��,�γ�һ������.
	
	for ( i = 0; i < NR_HASH; i++ )
	{
		hash_table[ i ] = NULL;
	}
}
