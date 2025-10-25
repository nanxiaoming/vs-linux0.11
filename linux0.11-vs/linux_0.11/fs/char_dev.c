/*
 *  linux/fs/char_dev.c
 *
 *  ( C ) 1991  Linus Torvalds
 */

#include <errno.h>
#include <sys\types.h>

#include <linux\sched.h>
#include <linux\kernel.h>

#include <asm\segment.h>
#include <asm\io.h>

extern LONG tty_read ( unsigned minor, CHAR * buf, LONG count ); // �ն˶�
extern LONG tty_write( unsigned minor, CHAR * buf, LONG count ); // �ն�д

// �����ַ��豸��д����ָ������
typedef ( *crw_ptr )( LONG rw, unsigned minor, CHAR * buf, LONG count, off_t * pos );

// �����ն˶�д��������.
// ����:rw - ��д����;minor - �ն����豸��;buf - ������;cout - ��д�ֽ���;
//       pos - ��д������ǰָ��,�����ն˲���,��ָ������.
// ����:ʵ�ʶ�д���ֽ���.
static LONG rw_ttyx( LONG rw, unsigned minor, CHAR * buf, LONG count, off_t * pos )
{
	return ( rw == READ ) ? 
			tty_read ( minor, buf, count ) :
			tty_write( minor, buf, count ) ;
}

// �ն˶�д��������.
// ͬ��rw_ttyx(),ֻ�������˶Խ����Ƿ��п����ն˵ļ��
static LONG rw_tty( LONG rw, unsigned minor, CHAR * buf, LONG count, off_t * pos )
{
	// ������û�ж�Ӧ�Ŀ����ն�,�򷵻س����
	if ( current->tty < 0 )
	{
		return -EPERM;
	}
	// ��������ն˶�д����rw_ttyx(),������ʵ�ʶ�д�ֽ���
	return rw_ttyx( rw, current->tty, buf, count, pos );
}

// �ڴ����ݶ�д.δʵ��
static LONG rw_ram( LONG rw, CHAR * buf, LONG count, off_t *pos )
{
	return -EIO;
}

// �ڴ����ݶ�д��������.δʵ��
static LONG rw_mem( LONG rw, CHAR * buf, LONG count, off_t * pos )
{
	return -EIO;
}
// �ں���������д����.δʵ��
static LONG rw_kmem( LONG rw, CHAR * buf, LONG count, off_t * pos )
{
	return -EIO;
}

// �˿ڶ�д��������.
// ����:rw - ��д����;buf - ������;cout - ��д�ֽ���;pos - �˿ڵ�ַ.
// ����:ʵ�ʶ�д���ֽ���.
static LONG rw_port( LONG rw, CHAR * buf, LONG count, off_t * pos )
{
	LONG i = *pos;

	// ������Ҫ���д���ֽ���,���Ҷ˿ڵ�ַС��64k ʱ,ѭ��ִ�е����ֽڵĶ�д����
	while ( count-- > 0 && i < 65536 )
	{
		// ���Ƕ�����,��Ӷ˿�i �ж�ȡһ�ֽ����ݲ��ŵ��û���������
		if ( rw == READ )
		{
			put_fs_byte( inb( (USHORT)i ), buf++ );
		}
		// ����д����,����û����ݻ�������ȡһ�ֽ�������˿�i
		else
		{
			outb( get_fs_byte( buf++ ), (USHORT)i );
		}
		i++;
	}

	// �����/д���ֽ���,����Ӧ������дָ��
	i    -= *pos;
	*pos += i;

	// ���ض�/д���ֽ���

	return i;
}

static LONG rw_memory( LONG rw, unsigned minor, CHAR * buf, LONG count, off_t * pos )
{
	// �����ڴ��豸���豸��,�ֱ���ò�ͬ���ڴ��д����
	switch ( minor )
	{
	case 0:
		return rw_ram( rw, buf, count, pos );
	case 1:
		return rw_mem( rw, buf, count, pos );
	case 2:
		return rw_kmem( rw, buf, count, pos );
	case 3:
		return ( rw == READ ) ? 0 : count;	/* rw_null */
	case 4:
		return rw_port( rw, buf, count, pos );
	default:
		return -EIO;
	}
}
// ����ϵͳ���豸����
#define NRDEVS ( ( sizeof ( crw_table ) )/( sizeof ( crw_ptr ) ) )

// �ַ��豸��д����ָ���
static crw_ptr crw_table[] = 
{
	NULL,		/* nodev		*/		/* ���豸( ���豸 )	*/
	rw_memory,	/* /dev/mem etc	*/		/* /dev/mem ��		*/
	NULL,		/* /dev/fd		*/		/* /dev/fd ����		*/
	NULL,		/* /dev/hd		*/		/* /dev/hd Ӳ��		*/
	rw_ttyx,	/* /dev/ttyx	*/		/* /dev/ttyx �����ն� */
	rw_tty,		/* /dev/tty		*/		/* /dev/tty �ն�		*/
	NULL,		/* /dev/lp		*/		/* /dev/lp ��ӡ��	*/
	NULL 		/* unnamed pipes*/		/* δ�����ܵ�			*/
};

// �ַ��豸��д��������.
// ����:rw - ��д����;dev - �豸��;buf - ������;count - ��д�ֽ���;pos -��дָ��.
// ����:ʵ�ʶ�/д�ֽ���.

LONG rw_char( LONG rw, LONG dev, CHAR * buf, LONG count, off_t * pos )
{
	crw_ptr call_addr;

	// ����豸�ų���ϵͳ�豸��,�򷵻س�����
	if ( MAJOR( dev ) >= NRDEVS )
	{
		return -ENODEV;
	}
	// �����豸û�ж�Ӧ�Ķ�/д����,�򷵻س�����
	if ( !( call_addr = crw_table[ MAJOR( dev ) ] ) )
	{
		return -ENODEV;
	}
	// ���ö�Ӧ�豸�Ķ�д��������,������ʵ�ʶ�/д���ֽ���
	return call_addr( rw, MINOR( dev ), buf, count, pos );
}
