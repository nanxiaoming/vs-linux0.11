/*
*  linux/fs/read_write.c
*
*  ( C ) 1991  Linus Torvalds
*/

#include <sys\stat.h>
#include <errno.h>
#include <sys\types.h>

#include <linux\kernel.h>
#include <linux\sched.h>
#include <asm\segment.h>

extern LONG rw_char( LONG rw, LONG dev, CHAR * buf, LONG count, off_t * pos );
extern LONG read_pipe( M_Inode * inode, CHAR * buf, LONG count );
extern LONG write_pipe( M_Inode * inode, CHAR * buf, LONG count );
extern LONG block_read( LONG dev, off_t * pos, CHAR * buf, LONG count );
extern LONG block_write( LONG dev, off_t * pos, CHAR * buf, LONG count );
extern LONG file_read( M_Inode * inode, File * filp,
	CHAR * buf, LONG count );
extern LONG file_write( M_Inode * inode, File * filp,
	CHAR * buf, LONG count );

// �ض�λ�ļ���дָ��ϵͳ���ú���.
// ����fd ���ļ����,offset ���µ��ļ���дָ��ƫ��ֵ,origin ��ƫ�Ƶ���ʼλ��,��SEEK_SET
// ( 0,���ļ���ʼ�� )��SEEK_CUR( 1,�ӵ�ǰ��дλ�� )��SEEK_END( 2,���ļ�β�� )����֮һ.

LONG sys_lseek( ULONG fd, off_t offset, LONG origin )
{
	File * file;
	LONG tmp;

	// ����ļ����ֵ���ڳ��������ļ���NR_OPEN( 20 ),���߸þ�����ļ��ṹָ��Ϊ��,����
	// ��Ӧ�ļ��ṹ��i �ڵ��ֶ�Ϊ��,����ָ���豸�ļ�ָ���ǲ��ɶ�λ��,�򷵻س����벢�˳�.

	if ( fd >= NR_OPEN || !( file = current->filp[ fd ] ) || !( file->f_inode )
		|| !IS_SEEKABLE( MAJOR( file->f_inode->i_dev ) ) )
		return -EBADF;

	// ����ļ���Ӧ��i �ڵ��ǹܵ��ڵ�,�򷵻س�����,�˳�.�ܵ�ͷβָ�벻�������ƶ���
	if ( file->f_inode->i_pipe )
		return -ESPIPE;
	// �������õĶ�λ��־,�ֱ����¶�λ�ļ���дָ��
	switch ( origin ) {
	// origin = SEEK_SET,Ҫ�����ļ���ʼ����Ϊԭ�������ļ���дָ��.��ƫ��ֵС����,�����
	// �ش�����.���������ļ���дָ�����offset
	case 0:
		if ( offset < 0 ) return -EINVAL;
		file->f_pos = offset;
		break;
	// origin = SEEK_CUR,Ҫ�����ļ���ǰ��дָ�봦��Ϊԭ���ض�λ��дָ��.����ļ���ǰָ���
	// ��ƫ��ֵС��0,�򷵻س������˳�.�����ڵ�ǰ��дָ���ϼ���ƫ��ֵ
	case 1:
		if ( file->f_pos + offset < 0 ) return -EINVAL;
		file->f_pos += offset;
		break;
	// origin = SEEK_END,Ҫ�����ļ�ĩβ��Ϊԭ���ض�λ��дָ��.��ʱ���ļ���С����ƫ��ֵС����
	// �򷵻س������˳�.�����ض�λ��дָ��Ϊ�ļ����ȼ���ƫ��ֵ
	case 2:
		if ( ( tmp = file->f_inode->i_size + offset ) < 0 )
			return -EINVAL;
		file->f_pos = tmp;
		break;
	// origin ���ó���,���س������˳�
	default:
		return -EINVAL;
	}
	return file->f_pos;
}

// ���ļ�ϵͳ���ú���.
// ����fd ���ļ����,buf �ǻ�����,count �������ֽ���
LONG sys_read( ULONG fd, CHAR * buf, LONG count )
{
	File * file;
	M_Inode * inode;

	// ����ļ����ֵ���ڳ��������ļ���NR_OPEN,������Ҫ��ȡ���ֽڼ���ֵС��0,���߸þ��
	// ���ļ��ṹָ��Ϊ��,�򷵻س����벢�˳�
	if ( fd >= NR_OPEN || count < 0 || !( file = current->filp[ fd ] ) )
		return -EINVAL;
	if ( !count )
		return 0;
	verify_area( buf, count );
	inode = file->f_inode;
	if ( inode->i_pipe )
		return ( file->f_mode & 1 ) ? read_pipe( inode, buf, count ) : -EIO;
	// ������ַ����ļ�,����ж��ַ��豸����,���ض�ȡ���ַ���
	if ( S_ISCHR( inode->i_mode ) )
		return rw_char( READ, inode->i_zone[ 0 ], buf, count, &file->f_pos );
	// ����ǿ��豸�ļ�,��ִ�п��豸������,�����ض�ȡ���ֽ���
	if ( S_ISBLK( inode->i_mode ) )
		return block_read( inode->i_zone[ 0 ], &file->f_pos, buf, count );
	// �����Ŀ¼�ļ������ǳ����ļ�,��������֤��ȡ��count ����Ч�Բ����е���( ����ȡ�ֽ�������
	// �ļ���ǰ��дָ��ֵ�����ļ���С,���������ö�ȡ�ֽ���Ϊ�ļ�����-��ǰ��дָ��ֵ,����ȡ��
	// ����0,�򷵻�0 �˳� ),Ȼ��ִ���ļ�������,���ض�ȡ���ֽ������˳�
	if ( S_ISDIR( inode->i_mode ) || S_ISREG( inode->i_mode ) ) {
		if ( count + file->f_pos > inode->i_size )
			count = inode->i_size - file->f_pos;
		if ( count <= 0 )
			return 0;
		return file_read( inode, file, buf, count );
	}
	printk( "( Read )inode->i_mode=%06o\n\r", inode->i_mode );
	return -EINVAL;
}

LONG sys_write( ULONG fd, CHAR * buf, LONG count )
{
	File * file;
	M_Inode * inode;

	if ( fd >= NR_OPEN || count < 0 || !( file = current->filp[ fd ] ) )
		return -EINVAL;
	if ( !count )
		return 0;
	inode = file->f_inode;
	if ( inode->i_pipe )
		return ( file->f_mode & 2 ) ? write_pipe( inode, buf, count ) : -EIO;
	
	// ������ַ����ļ� �����д�ַ��豸����
	if ( S_ISCHR( inode->i_mode ) )
		return rw_char( WRITE, inode->i_zone[ 0 ], buf, count, &file->f_pos );
	// ���豸
	if ( S_ISBLK( inode->i_mode ) )
		return block_write( inode->i_zone[ 0 ], &file->f_pos, buf, count );
	// �����ļ�
	if ( S_ISREG( inode->i_mode ) )
		return file_write( inode, file, buf, count );
	printk( "( Write )inode->i_mode=%06o\n\r", inode->i_mode );
	return -EINVAL;
}
