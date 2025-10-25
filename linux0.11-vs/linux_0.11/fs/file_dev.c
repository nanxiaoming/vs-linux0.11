/*
 *  linux/fs/file_dev.c
 *
 *  ( C ) 1991  Linus Torvalds
 */

#include <errno.h>
#include <fcntl.h>

#include <linux\sched.h>
#include <linux\kernel.h>
#include <asm\segment.h>

#define MIN( a,b ) ( ( ( a )<( b ) )?( a ):( b ) )	// ȡa,b �е���Сֵ
#define MAX( a,b ) ( ( ( a )>( b ) )?( a ):( b ) )	// ȡa,b �е����ֵ

 // �ļ�������- ����i �ڵ���ļ��ṹ,���豸����.
 // ��i �ڵ����֪���豸��,��filp �ṹ����֪���ļ��е�ǰ��дָ��λ��.buf ָ���û�̬��
 // ��������λ��,count Ϊ��Ҫ��ȡ���ֽ���.����ֵ��ʵ�ʶ�ȡ���ֽ���,������( С��0 ).

LONG file_read( M_Inode * inode, File * filp, CHAR * buf, LONG count )
{
	LONG			left, chars, nr;
	Buffer_Head	*	bh;

	// ����Ҫ��ȡ���ֽڼ���ֵС�ڵ�����,�򷵻�
	if ( ( left = count ) <= 0 )
	{
		return 0;
	}

	// ������Ҫ��ȡ���ֽ���������0,��ѭ��ִ�����²���,ֱ��ȫ������
	while ( left ) 
	{
		// ����i �ڵ���ļ���ṹ��Ϣ,ȡ���ݿ��ļ���ǰ��дλ�����豸�϶�Ӧ���߼����nr.��nr ��
		// Ϊ0,���i �ڵ�ָ�����豸�϶�ȡ���߼���,���������ʧ�����˳�ѭ��.��nr Ϊ0,��ʾָ��
		// �����ݿ鲻����,�û����ָ��ΪNULL
		if ( nr = bmap( inode, ( filp->f_pos ) / BLOCK_SIZE ) ) 
		{
			if ( !( bh = bread( inode->i_dev, nr ) ) )
			{
				break;
			}
		}
		else
		{
			bh = NULL;
		}

		// �����ļ���дָ�������ݿ��е�ƫ��ֵnr,��ÿ��пɶ��ֽ���Ϊ( BLOCK_SIZE-nr ),Ȼ���뻹��
		// ��ȡ���ֽ���left ���Ƚ�,����Сֵ��Ϊ����������ֽ���chars.��( BLOCK_SIZE-nr )����˵��
		// �ÿ�����Ҫ��ȡ�����һ������,��֮����Ҫ��ȡһ������

		nr		= filp->f_pos % BLOCK_SIZE;
		chars	= MIN( BLOCK_SIZE - nr, left );

		// ������д�ļ�ָ��.ָ��ǰ�ƴ˴ν���ȡ���ֽ���chars.ʣ���ֽڼ�����Ӧ��ȥchars

		filp->f_pos += chars;
		left		-= chars;

		// �����豸�϶���������,��p ָ��������ݿ黺�����п�ʼ��ȡ��λ��,���Ҹ���chars �ֽ�
		// ���û�������buf ��.�������û�������������chars ��0 ֵ�ֽ�

		if ( bh ) 
		{
			CHAR * p = nr + bh->b_data;
			while ( chars-- > 0 )
			{
				put_fs_byte( *( p++ ), buf++ );
			}
			brelse( bh );
		}
		else
		{
			while ( chars-- > 0 )
			{
				put_fs_byte( 0, buf++ );
			}
		}
	}

	// �޸ĸ�i �ڵ�ķ���ʱ��Ϊ��ǰʱ��.���ض�ȡ���ֽ���,����ȡ�ֽ���Ϊ0,�򷵻س����
	inode->i_atime = CURRENT_TIME;

	return ( count - left ) ? ( count - left ) : -ERROR;
}

// �ļ�д����- ����i �ڵ���ļ��ṹ��Ϣ,���û�����д��ָ���豸.
// ��i �ڵ����֪���豸��,��filp �ṹ����֪���ļ��е�ǰ��дָ��λ��.buf ָ���û�̬��
// ��������λ��,count Ϊ��Ҫд����ֽ���.����ֵ��ʵ��д����ֽ���,������( С��0 ).

LONG file_write( M_Inode * inode, File * filp, CHAR * buf, LONG count )
{
	off_t			pos;
	LONG			block, c;
	Buffer_Head *	bh;
	CHAR		*	p;
	LONG			i = 0;

	/*
	 * ok, append may not work when many processes are writing at the same time
	 * but so what. That way leads to madness anyway.
	 */
	/*
	 * ok,��������ͬʱдʱ,append �������ܲ���,����������.����������������
	 * ���»���һ��.
	 */
	// �����Ҫ���ļ����������,���ļ���дָ���Ƶ��ļ�β��.����ͽ����ļ���дָ�봦д��.

	if ( filp->f_flags & O_APPEND )
		pos = inode->i_size;
	else
		pos = filp->f_pos;

	// ����д���ֽ���i С����Ҫд����ֽ���count,��ѭ��ִ�����²���

	while ( i<count ) 
	{
		// �������ݿ��( pos/BLOCK_SIZE )���豸�϶�Ӧ���߼���,���������豸�ϵ��߼����.����߼�
		// ���=0,���ʾ����ʧ��,�˳�ѭ��
		if ( !( block = create_block( inode, pos / BLOCK_SIZE ) ) )
			break;
		// ���ݸ��߼���Ŷ�ȡ�豸�ϵ���Ӧ���ݿ�,���������˳�ѭ��
		if ( !( bh = bread( inode->i_dev, block ) ) )
			break;

		// ����ļ���дָ�������ݿ��е�ƫ��ֵc,��p ָ��������ݿ黺�����п�ʼ��ȡ��λ��.�ø�
		// ���������޸ı�־

		c = pos % BLOCK_SIZE;
		p = c + bh->b_data;
		bh->b_dirt = 1;

		// �ӿ�ʼ��дλ�õ���ĩ����д��c=( BLOCK_SIZE-c )���ֽ�.��c ����ʣ�໹��д����ֽ���
		// ( count-i ),��˴�ֻ����д��c=( count-i )����

		c = BLOCK_SIZE - c;

		if ( c > count - i ) c = count - i;

		// �ļ���дָ��ǰ�ƴ˴���д����ֽ���.�����ǰ�ļ���дָ��λ��ֵ�������ļ��Ĵ�С,��
		// �޸�i �ڵ����ļ���С�ֶ�,����i �ڵ����޸ı�־

		pos += c;

		if ( pos > inode->i_size )
		{
			inode->i_size = pos;
			inode->i_dirt = 1;
		}
		// ��д���ֽڼ����ۼӴ˴�д����ֽ���c.���û�������buf �и���c ���ֽڵ����ٻ�������p
		// ָ��ʼ��λ�ô�.Ȼ���ͷŸû�����
		i += c;
		while ( c-- > 0 )
			*( p++ ) = get_fs_byte( buf++ );
		brelse( bh );
	}

	// �����ļ��޸�ʱ��Ϊ��ǰʱ��
	inode->i_mtime = CURRENT_TIME;

	// ����˴β����������ļ�β�������,����ļ���дָ���������ǰ��дλ��,������i �ڵ��޸�
	// ʱ��Ϊ��ǰʱ��

	if ( !( filp->f_flags & O_APPEND ) ) 
	{
		filp->f_pos = pos;
		inode->i_ctime = CURRENT_TIME;
	}

	// ����д����ֽ���,��д���ֽ���Ϊ0,�򷵻س����-1
	return ( i ? i : -1 );
}
