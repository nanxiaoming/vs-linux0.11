/*
 *  linux/fs/block_dev.c
 *
 *  ( C ) 1991  Linus Torvalds
 */

#include <errno.h>

#include <linux\sched.h>
#include <linux\kernel.h>
#include <asm\segment.h>
#include <asm\system.h>

// 
// block_dev.c �������ڿ��豸�ļ����ݷ��ʲ��������.���ļ�����block_read()��
// block_write()�������豸��д����.�����������ǹ�ϵͳ���ú���read()��write()���õ�,
// 


// ���ݿ�д����- ��ָ���豸�Ӹ���ƫ�ƴ�д��ָ�������ֽ�����.
// ����:dev - �豸��;pos - �豸�ļ���ƫ����ָ��;buf - �û���ַ�ռ��л�������ַ;
//       count - Ҫ���͵��ֽ���.
// �����ں���˵,д����������ٻ�������д������,ʲôʱ����������д���豸���ɸ��ٻ������
// ��������������.����,��Ϊ�豸���Կ�Ϊ��λ���ж�д��,��˶���д��ʼλ�ò����ڿ���ʼ
// ��ʱ,��Ҫ�Ƚ���ʼ�ֽ����ڵ����������,Ȼ����Ҫд�����ݴ�д��ʼ����д���ÿ�,�ٽ���
// ����һ������д��( �����ɸ��ٻ������ȥ���� ).
LONG block_write( LONG dev, LONG * pos, CHAR * buf, LONG count )
{
	// ��pos ��ַ����ɿ�ʼ��д��Ŀ����block.����������1 �ֽ��ڸÿ��е�ƫ��λ��offset.
	LONG block  = *pos >> BLOCK_SIZE_BITS;
	LONG offset = *pos & ( BLOCK_SIZE - 1 );
	LONG chars;
	LONG written = 0;
	Buffer_Head * bh;
	register CHAR * p;

	// ���Ҫд����ֽ���count,ѭ��ִ�����²���,ֱ��ȫ��д��.
	while ( count > 0 ) {

		// �����ڸÿ��п�д����ֽ���.�����Ҫд����ֽ������һ��,��ֻ��дcount �ֽ�

		chars = BLOCK_SIZE - offset;
		if ( chars > count )
			chars = count;
		//�������Ҫд1 ������,��ֱ������1 ����ٻ����,������Ҫ���뽫���޸ĵ����ݿ�,��Ԥ��
		//����������,Ȼ�󽫿�ŵ���1.

		if ( chars == BLOCK_SIZE )
			bh = getblk( dev, block );
		else
			bh = breada( dev, block, block + 1, block + 2, -1 );
		block++;

		// �����������ʧ��,�򷵻���д�ֽ���,���û��д���κ��ֽ�,�򷵻س����( ���� )
		if ( !bh )
			return written ? written : -EIO;

		// p ָ��������ݿ��п�ʼд��λ��.�����д������ݲ���һ��,����ӿ鿪ʼ��д( �޸� )����
		// ���ֽ�,�����������offset Ϊ��.
		p = offset + bh->b_data;
		offset = 0;

		// ���ļ���ƫ��ָ��ǰ����д�ֽ���.�ۼ���д�ֽ���chars.���ͼ���ֵ��ȥ�˴��Ѵ����ֽ���.

		*pos    += chars;
		written += chars;
		count   -= chars;
		// ���û�����������chars �ֽڵ�p ָ��ĸ��ٻ������п�ʼд���λ��.
		while ( chars-- > 0 )
			*( p++ ) = get_fs_byte( buf++ );

		// �øû����������޸ı�־,���ͷŸû�����( Ҳ���û��������ü����ݼ�1 ).
		bh->b_dirt = 1;
		brelse( bh );
	}
	return written;
}

//
// ���ݿ������- ��ָ���豸��λ�ö���ָ���ֽ��������ݵ����ٻ�����
//
LONG block_read( LONG dev, ULONG * pos, CHAR * buf, LONG count )
{
	// ��pos ��ַ����ɿ�ʼ��д��Ŀ����block.����������1 �ֽ��ڸÿ��е�ƫ��λ��offset.

	LONG block = *pos >> BLOCK_SIZE_BITS;
	LONG offset = *pos & ( BLOCK_SIZE - 1 );
	LONG chars;
	LONG read = 0;
	Buffer_Head * bh;
	register CHAR * p;

	// ���Ҫ������ֽ���count,ѭ��ִ�����²���,ֱ��ȫ������
	while ( count > 0 ) {
		// �����ڸÿ����������ֽ���.�����Ҫ������ֽ�������һ��,��ֻ���count �ֽ�.
		chars = BLOCK_SIZE - offset;
		if ( chars > count )
			chars = count;

		// ������Ҫ�����ݿ�,��Ԥ������������,�������������,�򷵻��Ѷ��ֽ���,���û�ж����κ�
		// �ֽ�,�򷵻س����.Ȼ�󽫿�ŵ���1.
		if ( !( bh = breada( dev, block, block + 1, block + 2, -1 ) ) )
			return read ? read : -EIO;
		block++;
		// p ָ����豸�������ݿ�����Ҫ��ȡ�Ŀ�ʼλ��.�������Ҫ��ȡ�����ݲ���һ��,����ӿ鿪ʼ
		// ��ȡ������ֽ�,��������轫offset ����
		p = offset + bh->b_data;
		offset = 0;
		// ���ļ���ƫ��ָ��ǰ���Ѷ����ֽ���chars.�ۼ��Ѷ��ֽ���.���ͼ���ֵ��ȥ�˴��Ѵ����ֽ���.
		*pos += chars;
		read += chars;
		count -= chars;
		// �Ӹ��ٻ�������p ָ��Ŀ�ʼλ�ø���chars �ֽ����ݵ��û�������,���ͷŸø��ٻ�����.
		while ( chars-- > 0 )
			put_fs_byte( *( p++ ), buf++ );
		brelse( bh );
	}
	return read;
}
