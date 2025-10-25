/*
 *  linux/fs/namei.c
 *
 *  ( C ) 1991  Linus Torvalds
 */

/*
 * Some corrections by tytso.
 */

#include <linux\sched.h>
#include <linux\kernel.h>
#include <asm\segment.h>

#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <const.h>
#include <sys\stat.h>

 // ����ģʽ��.x ��include/fcntl.h ������ļ����ʱ�־.
 // ����x ֵ������Ӧ��ֵ( ��ֵ��ʾrwx Ȩ��: r, w, rw, wxrwxrwx )( ��ֵ��8 ���� ).

#define ACC_MODE( x ) ( "\004\002\006\377"[ ( x )&O_ACCMODE ] )

/*
 * comment out this line if you want names > NAME_LEN chars to be
 * truncated. Else they will be disallowed.
 */
/* #define NO_TRUNCATE */

//
// ��������ļ�������>NAME_LEN ���ַ����ص�,�ͽ����涨��ע�͵�.
//
#define MAY_EXEC 1
#define MAY_WRITE 2
#define MAY_READ 4

/*
 *	permission()
 *
 * is used to check for read/write/execute permissions on a file.
 * I don't know if we should look at just the euid or both euid and
 * uid, but that should be easily changed.
 */

// 
// permission()
// 
// �ú������ڼ��һ���ļ��Ķ�/д/ִ��Ȩ��.�Ҳ�֪���Ƿ�ֻ����euid,����
// ��Ҫ���euid ��uid ����,������������޸�.
//

//  
// ����ļ��������Ȩ��.
// ����:inode - �ļ���Ӧ��i �ڵ�;mask - ��������������.
// ����:������ɷ���1,���򷵻�0.
//
static LONG permission( M_Inode * inode, LONG mask )
{
	LONG mode = inode->i_mode;

	/* special case: not even root can read/write a deleted file */

	/* �������:��ʹ�ǳ����û�( root )Ҳ���ܶ�/дһ���ѱ�ɾ�����ļ� */
	// ���i �ڵ��ж�Ӧ���豸,����i �ڵ������������0,�򷵻�.
	if ( inode->i_dev && !inode->i_nlinks )
	{
		return 0;
	}

	// ����,������̵���Ч�û�id( euid )��i �ڵ���û�id ��ͬ,��ȡ�ļ��������û�����Ȩ��
	else if ( current->euid == inode->i_uid )
	{
		mode >>= 6;
	}

	// ����,������̵���Ч��id( egid )��i �ڵ����id ��ͬ,��ȡ���û��ķ���Ȩ��
	else if ( current->egid == inode->i_gid )
	{
		mode >>= 3;
	}
	// ���������ȡ�ĵķ���Ȩ������������ͬ,�����ǳ����û�,�򷵻�1,���򷵻�0
	if ( ( ( mode & mask & 0007 ) == mask ) || suser() )
	{
		return 1;
	}
	return 0;
}

/*
 * ok, we cannot use strncmp, as the name is not in our data space.
 * Thus we'll have to use match. No big problem. Match also makes
 * some sanity tests.
 *
 * NOTE! unlike strncmp, match returns 1 for success, 0 for failure.
 */

/*
 * ok,���ǲ���ʹ��strncmp �ַ����ȽϺ���,��Ϊ���Ʋ������ǵ����ݿռ�( �����ں˿ռ� ).
 * �������ֻ��ʹ��match().���ⲻ��.match()ͬ��Ҳ����һЩ�����Ĳ���.
 *
 * ע�⣡��strncmp ��ͬ����match()�ɹ�ʱ����1,ʧ��ʱ����0.
 */
// ָ�������ַ����ȽϺ���.
// ����:len - �Ƚϵ��ַ�������;name - �ļ���ָ��;de - Ŀ¼��ṹ.
// ����:��ͬ����1,��ͬ����0.
static LONG match( LONG len, const CHAR *name, Dir_Entry *de )
{
	register 
		
	LONG	same;
	CHAR *	D;

	if ( !de || !de->inode || len > NAME_LEN )
	{
		return 0;
	}

	if ( len < NAME_LEN && de->name[ len ] )
	{
		return 0;
	}

	D = de->name;

	__asm xor	eax, eax
	__asm mov	edi, D
	__asm mov	esi, name
	__asm mov	ecx, len
	__asm cld
	__asm repe	cmps BYTE PTR fs : [ esi ], BYTE PTR es : [ edi ];
	__asm setz	al
	__asm mov	same, eax

	return same;
}

/*
 *	find_entry()
 *
 * finds an entry in the specified directory with the wanted name. It
 * returns the cache buffer in which the entry was found, and the entry
 * itself ( as a parameter - res_dir ). It does NOT read the inode of the
 * entry - you'll have to do that yourself if you want to.
 *
 * This also takes care of the few special cases due to '..'-traversal
 * over a pseudo-root and a mount point.
 */
/*
 *	find_entry()
 *
 * ��ָ����Ŀ¼��Ѱ��һ��������ƥ���Ŀ¼��.����һ�������ҵ�Ŀ¼��ĸ���
 * �������Լ�Ŀ¼���( ��Ϊһ������- res_dir ).������Ŀ¼���i �ڵ�- ��
 * ����Ҫ�Ļ����Լ�����.
 *
 * '..'Ŀ¼��,�����ڼ�Ҳ��Լ�����������ֱ���- �����Խһ��α��Ŀ¼��
 * ����װ��.
 */

// ����ָ��Ŀ¼���ļ�����Ŀ¼��.
// ����:dir - ָ��Ŀ¼i �ڵ��ָ��;name - �ļ���;namelen - �ļ�������;
// ����:���ٻ�����ָ��;res_dir - ���ص�Ŀ¼��ṹָ��;
static 
Buffer_Head * 
find_entry(
			M_Inode		**	dir,
	const	CHAR		*	name, 
			LONG			namelen, 
			Dir_Entry	**	res_dir 
)
{
	LONG				entries;
	LONG				block, i;
	Buffer_Head		*	bh;
	Dir_Entry		*	de;
	Super_Block		*	sb;

	// ���������NO_TRUNCATE,�����ļ������ȳ�����󳤶�NAME_LEN,�򷵻�.
#ifdef NO_TRUNCATE

	if ( namelen > NAME_LEN )
	{
		return NULL;
	}

	//���û�ж���NO_TRUNCATE,�����ļ������ȳ�����󳤶�NAME_LEN,��ض�֮
#else
	if ( namelen > NAME_LEN )
	{
		namelen = NAME_LEN;
	}
#endif

	// ���㱾Ŀ¼��Ŀ¼������entries.�ÿշ���Ŀ¼��ṹָ��
	entries = ( *dir )->i_size / ( sizeof ( Dir_Entry ) );
	*res_dir = NULL;

	// ����ļ������ȵ���0,�򷵻�NULL,�˳�
	if ( !namelen )
	{
		return NULL;
	}

	/* check for '..', as we might have to do some "magic" for it */
	//���Ŀ¼��'..',��Ϊ������Ҫ�����ر���
	if ( namelen == 2 && get_fs_byte( name ) == '.' && get_fs_byte( name + 1 ) == '.' ) 
	{
		/* '..' in a pseudo-root results in a faked '.' ( just change namelen ) */
		/* α���е�'..'��ͬһ����'.'( ֻ��ı����ֳ��� ) */
		// �����ǰ���̵ĸ��ڵ�ָ�뼴��ָ����Ŀ¼,���ļ����޸�Ϊ'.',
		if ( ( *dir ) == current->root )
			// ���������Ŀ¼��i �ڵ�ŵ���ROOT_INO( 1 )�Ļ�,˵�����ļ�ϵͳ���ڵ�.��ȡ�ļ�ϵͳ�ĳ�����
			namelen = 1;

		else if ( ( *dir )->i_num == ROOT_INO ) 
		{
			/* '..' over a mount-point results in 'dir' being exchanged for the mounted
			   directory-inode. NOTE! We set mounted, so that we can iput the new dir */
			/* ��һ����װ���ϵ�'..'������Ŀ¼��������װ���ļ�ϵͳ��Ŀ¼i �ڵ�.
			   ע�⣡����������mounted ��־,��������ܹ�ȡ������Ŀ¼ */
			sb = get_super( ( *dir )->i_dev );

			// �������װ����i �ڵ����,�����ͷ�ԭi �ڵ�,Ȼ��Ա���װ����i �ڵ���д���.
			// ��*dir ָ��ñ���װ����i �ڵ�;��i �ڵ����������1.

			if ( sb->s_imount ) 
			{
				iput( *dir );
				( *dir ) = sb->s_imount;
				( *dir )->i_count++;
			}
		}
	}
	// �����i �ڵ���ָ��ĵ�һ��ֱ�Ӵ��̿��Ϊ0,�򷵻�NULL,�˳�
	if ( !( block = ( *dir )->i_zone[ 0 ] ) )
	{
		return NULL;
	}
	// �ӽڵ������豸��ȡָ����Ŀ¼�����ݿ�,������ɹ�,�򷵻�NULL,�˳�
	if ( !( bh = bread( ( *dir )->i_dev, block ) ) )
	{
		return NULL;
	}
	// ��Ŀ¼�����ݿ�������ƥ��ָ���ļ�����Ŀ¼��,������de ָ�����ݿ�,���ڲ�����Ŀ¼��Ŀ¼����
	// ��������,ѭ��ִ������
	i = 0;

	de = ( Dir_Entry * ) bh->b_data;

	while ( i < entries ) 
	{
		// �����ǰĿ¼�����ݿ��Ѿ�������,��û���ҵ�ƥ���Ŀ¼��,���ͷŵ�ǰĿ¼�����ݿ�
		if ( ( CHAR * )de >= BLOCK_SIZE + bh->b_data ) 
		{
			brelse( bh );
			bh = NULL;
			// �ڶ�����һĿ¼�����ݿ�.�����Ϊ��,��ֻҪ��û��������Ŀ¼�е�����Ŀ¼��,�������ÿ�,
			// ��������һĿ¼�����ݿ�.���ÿ鲻��,����de ָ���Ŀ¼�����ݿ�,��������
			if ( !( block = bmap( *dir, i / DIR_ENTRIES_PER_BLOCK ) ) ||
				 !( bh = bread( ( *dir )->i_dev, block ) ) 
				) 
			{
				i += DIR_ENTRIES_PER_BLOCK;
				continue;
			}
			de = ( Dir_Entry * ) bh->b_data;
		}
		// ����ҵ�ƥ���Ŀ¼��Ļ�,�򷵻ظ�Ŀ¼��ṹָ��͸�Ŀ¼�����ݿ�ָ��,�˳�
		if ( match( namelen, name, de ) ) 
		{
			*res_dir = de;
			return bh;
		}
		// ���������Ŀ¼�����ݿ��бȽ���һ��Ŀ¼��
		de++;
		i++;
	}
	brelse( bh );
	return NULL;
}

/*
 *	add_entry()
 *
 * adds a file entry to the specified directory, using the same
 * semantics as find_entry(). It returns NULL if it failed.
 *
 * NOTE!! The inode part of 'de' is left at 0 - which means you
 * may not sleep between calling this and putting something into
 * the entry, as someone else might have used it while you slept.
 */
/*
 *	add_entry()
 *
 * ʹ����find_entry()ͬ���ķ���,��ָ��Ŀ¼�����һ�ļ�Ŀ¼��.
 * ���ʧ���򷵻�NULL.
 *
 * ע�⣡��'de'( ָ��Ŀ¼��ṹָ�� )��i �ڵ㲿�ֱ�����Ϊ0 - ���ʾ
 * �ڵ��øú�������Ŀ¼���������Ϣ֮�䲻��˯��,��Ϊ��˯����ô����
 * ��( ���� )���ܻ��Ѿ�ʹ���˸�Ŀ¼��.
 */
// ����ָ����Ŀ¼���ļ������Ŀ¼��.
// ����:dir - ָ��Ŀ¼��i �ڵ�;name - �ļ���;namelen - �ļ�������;
// ����:���ٻ�����ָ��;res_dir - ���ص�Ŀ¼��ṹָ��;
static Buffer_Head *
add_entry( 
			M_Inode		*	dir,
	const	CHAR		*	name,
			LONG			namelen,
			Dir_Entry	**	res_dir 
)
{
	LONG				block, i;
	Buffer_Head		*	bh;
	Dir_Entry		*	de;

	*res_dir = NULL;

#ifdef NO_TRUNCATE
	// ���������NO_TRUNCATE,�����ļ������ȳ�����󳤶�NAME_LEN,�򷵻�
	if ( namelen > NAME_LEN )
		return NULL;
#else
	//���û�ж���NO_TRUNCATE,�����ļ������ȳ�����󳤶�NAME_LEN,��ض�֮
	if ( namelen > NAME_LEN )
		namelen = NAME_LEN;
#endif

	if ( !namelen )
		return NULL;
	if ( !( block = dir->i_zone[ 0 ] ) )
		return NULL;
	if ( !( bh = bread( dir->i_dev, block ) ) )
		return NULL;

	// ��Ŀ¼�����ݿ���ѭ���������δʹ�õ�Ŀ¼��.������Ŀ¼��ṹָ��de ָ����ٻ�������ݿ�
	// ��ʼ��,Ҳ����һ��Ŀ¼��
	i = 0;

	de = ( Dir_Entry * ) bh->b_data;

	while ( 1 )
	{
		// �����ǰ�б��Ŀ¼���Ѿ�������ǰ���ݿ�,���ͷŸ����ݿ�,��������һ����̿�block.���
		// ����ʧ��,�򷵻�NULL,�˳�.
		if ( ( CHAR * )de >= BLOCK_SIZE + bh->b_data ) 
		{
			brelse( bh );
			bh = NULL;

			block = create_block( dir, i / DIR_ENTRIES_PER_BLOCK );

			if ( !block )
			{
				return NULL;
			}

			// �����ȡ���̿鷵�ص�ָ��Ϊ��,�������ÿ����
			if ( !( bh = bread( dir->i_dev, block ) ) ) 
			{
				i += DIR_ENTRIES_PER_BLOCK;
				continue;
			}
			// ����,��Ŀ¼��ṹָ��de ־��ÿ�ĸ��ٻ������ݿ鿪ʼ��
			de = ( Dir_Entry * ) bh->b_data;
		}

		// �����ǰ��������Ŀ¼�����i*Ŀ¼�ṹ��С�Ѿ������˸�Ŀ¼��ָ���Ĵ�Сi_size,��˵���õ�i
		// ��Ŀ¼�δʹ��,���ǿ���ʹ����.���ǶԸ�Ŀ¼���������( �ø�Ŀ¼���i �ڵ�ָ��Ϊ�� ).��
		// ���¸�Ŀ¼�ĳ���ֵ( ����һ��Ŀ¼��ĳ���,����Ŀ¼��i �ڵ����޸ı�־,�ٸ��¸�Ŀ¼�ĸı�ʱ
		// ��Ϊ��ǰʱ��
		if ( i*sizeof( Dir_Entry ) >= dir->i_size )
		{
			de->inode		= 0;
			dir->i_size		= ( i + 1 )*sizeof( Dir_Entry );
			dir->i_dirt		= 1;
			dir->i_ctime	= CURRENT_TIME;
		}
		// ����Ŀ¼���i �ڵ�Ϊ��,���ʾ�ҵ�һ����δʹ�õ�Ŀ¼��.���Ǹ���Ŀ¼���޸�ʱ��Ϊ��ǰʱ��.
		// �����û������������ļ�������Ŀ¼����ļ����ֶ�,����Ӧ�ĸ��ٻ�������޸ı�־.���ظ�Ŀ¼
		// ���ָ���Լ��ø��ٻ�������ָ��,�˳�
		if ( !de->inode ) 
		{
			dir->i_mtime = CURRENT_TIME;

			for ( i = 0; i < NAME_LEN; i++ )
				de->name[ i ] = ( i < namelen ) ? get_fs_byte( name + i ) : 0;

			bh->b_dirt = 1;

			*res_dir = de;

			return bh;
		}
		// �����Ŀ¼���Ѿ���ʹ��,����������һ��Ŀ¼��
		de++;
		i++;
	}
	// ִ�в�������.Ҳ��Linus ��д��δ���ʱ���ȸ���������find_entry()�Ĵ���,�����޸ĵ�: )
	brelse( bh );
	return NULL;
}

/*
 *	get_dir()
 *
 * Getdir traverses the pathname until it hits the topmost directory.
 * It returns NULL on failure.
 */
/*
 *	get_dir()
 *
 * �ú������ݸ�����·������������,ֱ���ﵽ��˵�Ŀ¼.
 * ���ʧ���򷵻�NULL.
 */
//// ��Ѱָ��·������Ŀ¼.
// ����:pathname - ·����.
// ����:Ŀ¼��i �ڵ�ָ��.ʧ��ʱ����NULL.
static M_Inode * 
get_dir( const CHAR * pathname )
{
			CHAR			c;
	const	CHAR		*	thisname;
			M_Inode		*	inode;
			Buffer_Head *	bh;
			LONG			namelen, inr, idev;
			Dir_Entry	*	de;

	if ( !current->root || !current->root->i_count )
		panic( "No root inode" );

	if ( !current->pwd || !current->pwd->i_count )
		panic( "No cwd inode" );

	if ( ( c = get_fs_byte( pathname ) ) == '/' ) 
	{
		inode = current->root;
		pathname++;
	}
	else if ( c )
		inode = current->pwd;
	else
		return NULL;	/* empty name is bad */

	inode->i_count++;

	while ( 1 ) 
	{
		thisname = pathname;

		if ( !S_ISDIR( inode->i_mode ) || !permission( inode, MAY_EXEC ) ) 
		{
			// ����i �ڵ㲻��Ŀ¼�ڵ�,����û�пɽ���ķ������,���ͷŸ�i �ڵ�,����NULL,�˳�
			iput( inode );
			return NULL;
		}

		// ��·������ʼ����������ַ�,ֱ���ַ����ǽ�β��( NULL )������'/',��ʱnamelen �����ǵ�ǰ����
		// Ŀ¼���ĳ���.������Ҳ��һ��Ŀ¼��,�����û�м�'/',�򲻻᷵�ظ����Ŀ¼��i �ڵ㣡
		// ����:/var/log/httpd,��ֻ����log/Ŀ¼��i �ڵ�

		for ( namelen = 0; ( c = get_fs_byte( pathname++ ) ) && ( c != '/' ); namelen++ )
			/* nothing */;
		// ���ַ��ǽ�β��NULL,������Ѿ�����ָ��Ŀ¼,�򷵻ظ�i �ڵ�ָ��,�˳�
		if ( !c )
			return inode;

		// ���ò���ָ��Ŀ¼���ļ�����Ŀ¼���,�ڵ�ǰ����Ŀ¼��Ѱ����Ŀ¼��.���û���ҵ�,
		// ���ͷŸ�i �ڵ�,������NULL,�˳�
		if ( !( bh = find_entry( &inode, thisname, namelen, &de ) ) ) 
		{
			iput( inode );
			return NULL;
		}
		// ȡ����Ŀ¼���i �ڵ��inr ���豸��idev,�ͷŰ�����Ŀ¼��ĸ��ٻ����͸�i �ڵ�
		inr		= de->inode;
		idev	= inode->i_dev;

		brelse( bh );

		iput( inode );

		// ȡ�ڵ��inr ��i �ڵ���Ϣ,��ʧ��,�򷵻�NULL,�˳�.��������Ը���Ŀ¼��i �ڵ���в���
		if ( !( inode = iget( idev, inr ) ) )
		{
			return NULL;
		}
	}
}

/*
 *	dir_namei()
 *
 * dir_namei() returns the inode of the directory of the
 * specified name, and the name within that directory.
 */
/*
 *	dir_namei()
 *
 * dir_namei()��������ָ��Ŀ¼����i �ڵ�ָ��,�Լ������Ŀ¼������.
 */
// ����:pathname - Ŀ¼·����;namelen - ·��������.
// ����:ָ��Ŀ¼�����Ŀ¼��i �ڵ�ָ������Ŀ¼�����䳤��.
static M_Inode *
dir_namei( 
	const	CHAR	*	pathname,
			LONG	*	namelen, 
	const	CHAR	**	name )
{
			CHAR		c;
	const	CHAR	*	basename;
			M_Inode *	dir;

	// ȡָ��·�������Ŀ¼��i �ڵ�,�������򷵻�NULL,�˳�
	if ( !( dir = get_dir( pathname ) ) )
	{
		return NULL;
	}
	// ��·����pathname �����������,�鴦���һ��'/'����������ַ���,�����䳤��,�������
	// ��Ŀ¼��i �ڵ�ָ��
	basename = pathname;

	while ( c = get_fs_byte( pathname++ ) )
		if ( c == '/' )
			basename = pathname;

	*namelen = pathname - basename - 1;
	*name = basename;
	return dir;
}

/*
 *	namei()
 *
 * is used by most simple commands to get the inode of a specified name.
 * Open, link etc use their own routines, but this is enough for things
 * like 'chmod' etc.
 */
/*
 *	namei()
 *
 * �ú��������򵥵���������ȡ��ָ��·�����Ƶ�i �ڵ�.open��link ����ʹ������
 * �Լ�����Ӧ����,���������޸�ģʽ'chmod'������������,�ú������㹻����.
 */
// ȡָ��·������i �ڵ�.
// ����:pathname - ·����.
// ����:��Ӧ��i �ڵ�
M_Inode * namei( const CHAR * pathname )
{
	const
	CHAR		*	basename;
	LONG			inr, dev, namelen;
	M_Inode		*	dir;
	Buffer_Head *	bh;
	Dir_Entry	*	de;

	// ���Ȳ���ָ��·�������Ŀ¼��Ŀ¼������i �ڵ�,��������,�򷵻�NULL,�˳�
	if ( !( dir = dir_namei( pathname, &namelen, &basename ) ) )
		return NULL;

	// ������ص�������ֵĳ�����0,���ʾ��·������һ��Ŀ¼��Ϊ���һ��
	if ( !namelen )			/* special case: '/usr/' etc */ /* ��Ӧ��'/usr/'����� */
		return dir;

	// �ڷ��صĶ���Ŀ¼��Ѱ��ָ���ļ�����Ŀ¼���i �ڵ�.��Ϊ������Ҳ��һ��Ŀ¼��,�����û
	// �м�'/',�򲻻᷵�ظ����Ŀ¼��i �ڵ㣡����:/var/log/httpd,��ֻ����log/Ŀ¼��i �ڵ�.
	// ���dir_namei()������'/'���������һ�����ֵ���һ���ļ���������.���������Ҫ����������
	// ���ʹ��Ѱ��Ŀ¼��i �ڵ㺯��find_entry()���д���.
	bh = find_entry( &dir, basename, namelen, &de );

	if ( !bh ) 
	{
		iput( dir );
		return NULL;
	}

	// ȡ��Ŀ¼���i �ڵ�ź�Ŀ¼���豸��,���ͷŰ�����Ŀ¼��ĸ��ٻ������Լ�Ŀ¼i �ڵ�
	inr = de->inode;
	dev = dir->i_dev;
	brelse( bh );
	iput( dir );

	// ȡ��Ӧ�ںŵ�i �ڵ�,�޸��䱻����ʱ��Ϊ��ǰʱ��,�������޸ı�־.��󷵻ظ�i �ڵ�ָ��
	dir = iget( dev, inr );

	if ( dir ) 
	{
		dir->i_atime = CURRENT_TIME;
		dir->i_dirt = 1;
	}
	return dir;
}

/*
 *	open_namei()
 *
 * namei for open - this is in fact almost the whole open-routine.
 */
/*
 *	open_namei()
 *
 * open()��ʹ�õ�namei ����- ����ʵ�����������Ĵ��ļ�����.
 */
//// �ļ���namei ����.
// ����:pathname - �ļ�·����;flag - �ļ��򿪱�־;mode - �ļ������������;
// ����:�ɹ�����0,���򷵻س�����;res_inode - ���صĶ�Ӧ�ļ�·�����ĵ�i �ڵ�ָ��
LONG 
open_namei( 
	const	CHAR		*	pathname, 
			LONG			flag, 
			LONG			mode,
			M_Inode		**  res_inode )
{
	const 
	CHAR			*	basename;
	LONG				inr, dev, namelen;
	M_Inode			*	dir, *inode;
	Buffer_Head		*	bh;
	Dir_Entry		*	de;

	// ����ļ��������ģʽ��־��ֻ��( 0 ),���ļ���0 ��־O_TRUNC ȴ��λ��,���Ϊֻд��־
	if ( ( flag & O_TRUNC ) && !( flag & O_ACCMODE ) )
		flag |= O_WRONLY;

	// ʹ�ý��̵��ļ��������������,���ε�����ģʽ�е���Ӧλ,��������ͨ�ļ���־

	mode &= 0777 & ~current->umask;

	mode |= I_REGULAR;

	// ����·����Ѱ�ҵ���Ӧ��i �ڵ�,�Լ�����ļ������䳤��

	if ( !( dir = dir_namei( pathname, &namelen, &basename ) ) )
		return -ENOENT;

	// �������ļ�������Ϊ0( ����'/usr/'����·��������� ),��ô���򿪲������Ǵ�������0,
	// ���ʾ��һ��Ŀ¼��,ֱ�ӷ��ظ�Ŀ¼��i �ڵ�,���˳�

	if ( !namelen ) 			/* special case: '/usr/' etc */
	{
		if ( !( flag & ( O_ACCMODE | O_CREAT | O_TRUNC ) ) ) 
		{
			*res_inode = dir;
			return 0;
		}
		// �����ͷŸ�i �ڵ�,���س�����
		iput( dir );
		return -EISDIR;
	}

	// ��dir �ڵ��Ӧ��Ŀ¼��ȡ�ļ�����Ӧ��Ŀ¼��ṹde �͸�Ŀ¼�����ڵĸ��ٻ�����
	bh = find_entry( &dir, basename, namelen, &de );

	// ����ø��ٻ���ָ��ΪNULL,���ʾû���ҵ���Ӧ�ļ�����Ŀ¼��,���ֻ�����Ǵ����ļ�����

	if ( !bh ) 
	{
		// ������Ǵ����ļ�,���ͷŸ�Ŀ¼��i �ڵ�,���س�����˳�
		if ( !( flag & O_CREAT ) ) 
		{
			iput( dir );
			return -ENOENT;
		}
		// ����û��ڸ�Ŀ¼û��д��Ȩ��,���ͷŸ�Ŀ¼��i �ڵ�,���س�����˳�
		if ( !permission( dir, MAY_WRITE ) ) 
		{
			iput( dir );
			return -EACCES;
		}
		// ��Ŀ¼�ڵ��Ӧ���豸������һ����i �ڵ�,��ʧ��,���ͷ�Ŀ¼��i �ڵ�,������û�пռ������
		inode = new_inode( dir->i_dev );

		if ( !inode ) 
		{
			iput( dir );
			return -ENOSPC;
		}
		// ����ʹ�ø���i �ڵ�,������г�ʼ����:�ýڵ���û�id;��Ӧ�ڵ����ģʽ;�����޸ı�־
		
		inode->i_uid	= current->euid;
		inode->i_mode	= (USHORT)mode;
		inode->i_dirt	= 1;

		// Ȼ����ָ��Ŀ¼dir �����һ��Ŀ¼��

		bh = add_entry( dir, basename, namelen, &de );

		// ������ص�Ӧ�ú�����Ŀ¼��ĸ��ٻ�����ָ��ΪNULL,���ʾ���Ŀ¼�����ʧ��.���ǽ���
		// ��i �ڵ���������Ӽ�����1;���ͷŸ�i �ڵ���Ŀ¼��i �ڵ�,���س�����,�˳�.
		if ( !bh ) 
		{
			inode->i_nlinks--;
			iput( inode );
			iput( dir );
			return -ENOSPC;
		}

		// ��ʼ���ø���Ŀ¼��:��i �ڵ��Ϊ�����뵽��i �ڵ�ĺ���;���ø��ٻ��������޸ı�־.Ȼ��
		// �ͷŸø��ٻ�����,�ͷ�Ŀ¼��i �ڵ�.������Ŀ¼���i �ڵ�ָ��,�˳�
		de->inode = inode->i_num;
		bh->b_dirt = 1;
		brelse( bh );
		iput( dir );
		*res_inode = inode;

		return 0;
	}
	// ��������Ŀ¼��ȡ�ļ�����Ӧ��Ŀ¼��ṹ�����ɹ�( Ҳ��bh ��ΪNULL ),ȡ����Ŀ¼���i �ڵ��
	// �������ڵ��豸��,���ͷŸø��ٻ������Լ�Ŀ¼��i �ڵ�
	inr = de->inode;
	dev = dir->i_dev;
	brelse( bh );
	iput( dir );

	// �����ռʹ�ñ�־O_EXCL ��λ,�򷵻��ļ��Ѵ��ڳ�����,�˳�
	if ( flag & O_EXCL )
		return -EEXIST;

	// ���ȡ��Ŀ¼���Ӧi �ڵ�Ĳ���ʧ��,�򷵻ط��ʳ�����,�˳�
	if ( !( inode = iget( dev, inr ) ) )
		return -EACCES;

	// ����i �ڵ���һ��Ŀ¼�Ľڵ㲢�ҷ���ģʽ��ֻ����ֻд,����û�з��ʵ����Ȩ��,���ͷŸ�i
	// �ڵ�,���ط���Ȩ�޳�����,�˳�
	if ( ( S_ISDIR( inode->i_mode ) && ( flag & O_ACCMODE ) ) ||
		 !permission( inode, ACC_MODE( flag ) ) 
		) 
	{
		iput( inode );
		return -EPERM;
	}

	// ���¸�i �ڵ�ķ���ʱ���ֶ�Ϊ��ǰʱ��.
	inode->i_atime = CURRENT_TIME;

	// ��������˽�0 ��־,�򽫸�i �ڵ���ļ����Ƚ�Ϊ0
	if ( flag & O_TRUNC )
	{
		truncate( inode );
	}
	// ��󷵻ظ�Ŀ¼��i �ڵ��ָ��,������0( �ɹ� )
	*res_inode = inode;
	return 0;
}

// ϵͳ���ú���- ����һ�������ļ�����ͨ�ļ��ڵ�( node ).
// ��������Ϊfilename,��mode ��dev ָ�����ļ�ϵͳ�ڵ�( ��ͨ�ļ����豸�����ļ��������ܵ� ).
// ����:filename - ·����;mode - ָ��ʹ������Լ��������ڵ������;dev - �豸��.
// ����:�ɹ��򷵻�0,���򷵻س�����
LONG sys_mknod( const CHAR * filename, LONG mode, LONG dev )
{
	const 
	CHAR		*	basename;
	LONG			namelen;
	M_Inode		*	dir, *inode;
	Buffer_Head *	bh;
	Dir_Entry	*	de;

	// ������ǳ����û�,�򷵻ط�����ɳ�����
	if ( !suser() )
		return -EPERM;

	// ����Ҳ�����Ӧ·����Ŀ¼��i �ڵ�,�򷵻س�����
	if ( !( dir = dir_namei( filename, &namelen, &basename ) ) )
		return -ENOENT;

	// �����˵��ļ�������Ϊ0,��˵��������·�������û��ָ���ļ���,�ͷŸ�Ŀ¼i �ڵ�,����
	// ������,�˳�
	if ( !namelen ) 
	{
		iput( dir );
		return -ENOENT;
	}

	// ����ڸ�Ŀ¼��û��д��Ȩ��,���ͷŸ�Ŀ¼��i �ڵ�,���ط�����ɳ�����,�˳�
	if ( !permission( dir, MAY_WRITE ) ) 
	{
		iput( dir );
		return -EPERM;
	}
	// �����Ӧ·�����������ļ�����Ŀ¼���Ѿ�����,���ͷŰ�����Ŀ¼��ĸ��ٻ�����,�ͷ�Ŀ¼
	// ��i �ڵ�,�����ļ��Ѿ����ڳ�����,�˳�.
	bh = find_entry( &dir, basename, namelen, &de );

	if ( bh ) 
	{
		brelse( bh );
		iput( dir );
		return -EEXIST;
	}

	// ����һ���µ�i �ڵ�,������ɹ�,���ͷ�Ŀ¼��i �ڵ�,�����޿ռ������,�˳�
	inode = new_inode( dir->i_dev );

	if ( !inode ) 
	{
		iput( dir );
		return -ENOSPC;
	}
	// ���ø�i �ڵ������ģʽ.���Ҫ�������ǿ��豸�ļ��������ַ��豸�ļ�,����i �ڵ��ֱ�ӿ�
	// ָ��0 �����豸��.
	inode->i_mode = (USHORT)mode;

	if ( S_ISBLK( mode ) || S_ISCHR( mode ) )
		inode->i_zone[ 0 ] = (USHORT)dev;
	// ���ø�i �ڵ���޸�ʱ�䡢����ʱ��Ϊ��ǰʱ��

	inode->i_mtime = inode->i_atime = CURRENT_TIME;
	inode->i_dirt = 1;

	// ��Ŀ¼�������һ��Ŀ¼��,���ʧ��( ������Ŀ¼��ĸ��ٻ�����ָ��ΪNULL ),���ͷ�Ŀ¼��
	// i �ڵ�;�������i �ڵ��������Ӽ�����λ,���ͷŸ�i �ڵ�.���س�����,�˳�

	bh = add_entry( dir, basename, namelen, &de );

	if ( !bh ) 
	{
		iput( dir );
		inode->i_nlinks = 0;
		iput( inode );
		return -ENOSPC;
	}
	// ���Ŀ¼���i �ڵ��ֶε�����i �ڵ��,�ø��ٻ��������޸ı�־,�ͷ�Ŀ¼���µ�i �ڵ�,
	// �ͷŸ��ٻ�����,��󷵻�0( �ɹ� )
	de->inode = inode->i_num;
	bh->b_dirt = 1;
	iput( dir );
	iput( inode );
	brelse( bh );
	return 0;
}

// ϵͳ���ú���- ����Ŀ¼.
// ����:pathname - ·����;mode - Ŀ¼ʹ�õ�Ȩ������.
// ����:�ɹ��򷵻�0,���򷵻س�����
LONG sys_mkdir( const CHAR * pathname, LONG mode )
{
	const 
	CHAR		*	basename;
	LONG			namelen;
	M_Inode		*	dir, *inode;
	Buffer_Head *	bh, *dir_block;
	Dir_Entry	*	de;
	
	// ������ǳ����û�,�򷵻ط�����ɳ�����
	if ( !suser() )
		return -EPERM;

	// ����Ҳ�����Ӧ·����Ŀ¼��i �ڵ�,�򷵻س�����
	if ( !( dir = dir_namei( pathname, &namelen, &basename ) ) )
		return -ENOENT;

	// �����˵��ļ�������Ϊ0,��˵��������·�������û��ָ���ļ���,�ͷŸ�Ŀ¼i �ڵ�,����
	// ������,�˳�
	if ( !namelen ) 
	{
		iput( dir );
		return -ENOENT;
	}
	// ����ڸ�Ŀ¼��û��д��Ȩ��,���ͷŸ�Ŀ¼��i �ڵ�,���ط�����ɳ�����,�˳�
	if ( !permission( dir, MAY_WRITE ) ) 
	{
		iput( dir );
		return -EPERM;
	}
	// �����Ӧ·�����������ļ�����Ŀ¼���Ѿ�����,���ͷŰ�����Ŀ¼��ĸ��ٻ�����,�ͷ�Ŀ¼
	// ��i �ڵ�,�����ļ��Ѿ����ڳ�����,�˳�
	bh = find_entry( &dir, basename, namelen, &de );

	if ( bh ) 
	{
		brelse( bh );
		iput( dir );
		return -EEXIST;
	}

	// ����һ���µ�i �ڵ�,������ɹ�,���ͷ�Ŀ¼��i �ڵ�,�����޿ռ������,�˳�

	inode = new_inode( dir->i_dev );
	if ( !inode )
	{
		iput( dir );
		return -ENOSPC;
	}

	// �ø���i �ڵ��Ӧ���ļ�����Ϊ32( һ��Ŀ¼��Ĵ�С ),�ýڵ����޸ı�־,�Լ��ڵ���޸�ʱ��
	// �ͷ���ʱ��

	inode->i_size = 32;
	inode->i_dirt = 1;
	inode->i_mtime = inode->i_atime = CURRENT_TIME;

	// Ϊ��i �ڵ�����һ���̿�,����ڵ��һ��ֱ�ӿ�ָ����ڸÿ��.�������ʧ��,���ͷŶ�ӦĿ¼
	// ��i �ڵ�;��λ�������i �ڵ����Ӽ���;�ͷŸ��µ�i �ڵ�,����û�пռ������,�˳�
	if ( !( inode->i_zone[ 0 ] = (USHORT)new_block( inode->i_dev ) ) ) 
	{
		iput( dir );
		inode->i_nlinks--;
		iput( inode );
		return -ENOSPC;
	}

	// �ø��µ�i �ڵ����޸ı�־
	inode->i_dirt = 1;
	// ��������Ĵ��̿�.������,���ͷŶ�ӦĿ¼��i �ڵ�;�ͷ�����Ĵ��̿�;��λ�������i �ڵ�
	// ���Ӽ���;�ͷŸ��µ�i �ڵ�,����û�пռ������,�˳�
	if ( !( dir_block = bread( inode->i_dev, inode->i_zone[ 0 ] ) ) ) 
	{
		iput( dir );
		free_block( inode->i_dev, inode->i_zone[ 0 ] );
		inode->i_nlinks--;
		iput( inode );
		return -ERROR;
	}

	// ��de ָ��Ŀ¼�����ݿ�,�ø�Ŀ¼���i �ڵ���ֶε����������i �ڵ��,�����ֶε���"."
	de = ( Dir_Entry * ) dir_block->b_data;
	de->inode = inode->i_num;
	strcpy( de->name, "." );

	// Ȼ��de ָ����һ��Ŀ¼��ṹ,�ýṹ���ڴ���ϼ�Ŀ¼�Ľڵ�ź�����".."
	de++;
	de->inode = dir->i_num;
	strcpy( de->name, ".." );
	inode->i_nlinks = 2;

	// Ȼ�����øø��ٻ��������޸ı�־,���ͷŸû�����
	dir_block->b_dirt = 1;
	brelse( dir_block );
	// ��ʼ��������i �ڵ��ģʽ�ֶ�,���ø�i �ڵ����޸ı�־
	inode->i_mode = I_DIRECTORY | ( mode & 0777 & ~current->umask );
	inode->i_dirt = 1;
	// ��Ŀ¼�������һ��Ŀ¼��,���ʧ��( ������Ŀ¼��ĸ��ٻ�����ָ��ΪNULL ),���ͷ�Ŀ¼��
	// i �ڵ�;�������i �ڵ��������Ӽ�����λ,���ͷŸ�i �ڵ�.���س�����,�˳�
	bh = add_entry( dir, basename, namelen, &de );
	if ( !bh )
	{
		iput( dir );
		free_block( inode->i_dev, inode->i_zone[ 0 ] );
		inode->i_nlinks = 0;
		iput( inode );
		return -ENOSPC;
	}
	// ���Ŀ¼���i �ڵ��ֶε�����i �ڵ��,�ø��ٻ��������޸ı�־,�ͷ�Ŀ¼���µ�i �ڵ�,�ͷ�
	// ���ٻ�����,��󷵻�0( �ɹ� )
	de->inode = inode->i_num;
	bh->b_dirt = 1;
	dir->i_nlinks++;
	dir->i_dirt = 1;
	iput( dir );
	iput( inode );
	brelse( bh );
	return 0;
}

/*
 * routine to check that the specified directory is empty ( for rmdir )
 */
/*
 * ���ڼ��ָ����Ŀ¼�Ƿ�Ϊ�յ��ӳ���( ����rmdir ϵͳ���ú��� ).
 */
// ���ָ��Ŀ¼�Ƿ��ǿյ�.
// ����:inode - ָ��Ŀ¼��i �ڵ�ָ��.
// ����:0 - �ǿյ�;1 - ����.
static LONG empty_dir( M_Inode * inode )
{
	LONG			nr, block;
	LONG			len;
	Buffer_Head *	bh;
	Dir_Entry	*	de;

	// ����ָ��Ŀ¼������Ŀ¼��ĸ���( Ӧ��������2 ��,��"."��".."�����ļ�Ŀ¼�� )
	len = inode->i_size / sizeof ( Dir_Entry );

	// ���Ŀ¼���������2 �����߸�Ŀ¼i �ڵ�ĵ�1 ��ֱ�ӿ�û��ָ���κδ��̿��,������Ӧ����
	// �������,����ʾ������Ϣ"�豸dev ��Ŀ¼��",����0( ʧ�� )
	if ( len < 2 || !inode->i_zone[ 0 ] || !( bh = bread( inode->i_dev, inode->i_zone[ 0 ] ) ) ) 
	{
		printk( "warning - bad directory on dev %04x\n", inode->i_dev );
		return 0;
	}

	// ��de ָ���ж������̿����ݵĸ��ٻ������е�1 ��Ŀ¼��
	de = ( Dir_Entry * ) bh->b_data;

	// �����1 ��Ŀ¼���i �ڵ���ֶ�ֵ�����ڸ�Ŀ¼��i �ڵ��,���ߵ�2 ��Ŀ¼���i �ڵ���ֶ�
	// Ϊ��,��������Ŀ¼��������ֶβ��ֱ����"."��"..",����ʾ��������Ϣ"�豸dev ��Ŀ¼��"
	// ������0
	if ( de[ 0 ].inode != inode->i_num || 
		!de[ 1 ].inode ||
		strcmp( "." , de[ 0 ].name ) ||
		strcmp( "..", de[ 1 ].name ) 
		) 
	{
		printk( "warning - bad directory on dev %04x\n", inode->i_dev );
		return 0;
	}

	// ��nr ����Ŀ¼�����;de ָ�������Ŀ¼��.
	nr  = 2;
	de += 2;

	// ѭ������Ŀ¼�����е�Ŀ¼��( len-2 �� ),����û��Ŀ¼���i �ڵ���ֶβ�Ϊ0( ��ʹ�� )
	while ( nr < len ) 
	{
		// ����ÿ���̿��е�Ŀ¼���Ѿ������,���ͷŸô��̿�ĸ��ٻ�����,��ȡ��һ�麬��Ŀ¼���
		// ���̿�.����Ӧ��û��ʹ��( ���Ѿ�����,���ļ��Ѿ�ɾ���� ),���������һ��,��������,���
		// ��,����0.������de ָ���������׸�Ŀ¼��

		if ( ( VOID * )de >= ( VOID * )( bh->b_data + BLOCK_SIZE ) ) 
		{
			brelse( bh );

			block = bmap( inode, nr / DIR_ENTRIES_PER_BLOCK );

			if ( !block ) 
			{
				nr += DIR_ENTRIES_PER_BLOCK;
				continue;
			}
			if ( !( bh = bread( inode->i_dev, block ) ) )
			{
				return 0;
			}
			de = ( Dir_Entry * ) bh->b_data;
		}
		// �����Ŀ¼���i �ڵ���ֶβ�����0,���ʾ��Ŀ¼��Ŀǰ����ʹ��,���ͷŸø��ٻ�����,
		// ����0,�˳�
		if ( de->inode ) 
		{
			brelse( bh );
			return 0;
		}
		// ����,����û�в�ѯ���Ŀ¼�е�����Ŀ¼��,��������
		de++;
		nr++;
	}
	// ������˵����Ŀ¼��û���ҵ����õ�Ŀ¼��( ��Ȼ����ͷ�������� ),�򷵻ػ�����,����1
	brelse( bh );
	return 1;
}

// ϵͳ���ú���- ɾ��ָ�����Ƶ�Ŀ¼.
// ����: name - Ŀ¼��( ·���� ).
// ����:����0 ��ʾ�ɹ�,���򷵻س����
LONG sys_rmdir( const CHAR * name )
{
	const CHAR * basename;
	LONG namelen;
	M_Inode * dir, *inode;
	Buffer_Head * bh;
	Dir_Entry * de;

	// ������ǳ����û�,�򷵻ط�����ɳ�����
	if ( !suser() )
		return -EPERM;

	// ����Ҳ�����Ӧ·����Ŀ¼��i �ڵ�,�򷵻س�����
	if ( !( dir = dir_namei( name, &namelen, &basename ) ) )
		return -ENOENT;

	// �����˵��ļ�������Ϊ0,��˵��������·�������û��ָ���ļ���,�ͷŸ�Ŀ¼i �ڵ�,����
	// ������,�˳�
	if ( !namelen ) 
	{
		iput( dir );
		return -ENOENT;
	}

	// ����ڸ�Ŀ¼��û��д��Ȩ��,���ͷŸ�Ŀ¼��i �ڵ�,���ط�����ɳ�����,�˳�
	if ( !permission( dir, MAY_WRITE ) ) 
	{
		iput( dir );
		return -EPERM;
	}
	// �����Ӧ·�����������ļ�����Ŀ¼�����,���ͷŰ�����Ŀ¼��ĸ��ٻ�����,�ͷ�Ŀ¼
	// ��i �ڵ�,�����ļ��Ѿ����ڳ�����,�˳�.����dir �ǰ���Ҫ��ɾ��Ŀ¼����Ŀ¼i �ڵ�,de
	// ��Ҫ��ɾ��Ŀ¼��Ŀ¼��ṹ

	bh = find_entry( &dir, basename, namelen, &de );

	if ( !bh )
	{
		iput( dir );
		return -ENOENT;
	}
	// ȡ��Ŀ¼��ָ����i �ڵ�.���������ͷ�Ŀ¼��i �ڵ�,���ͷź���Ŀ¼��ĸ��ٻ�����,����
	// �����
	if ( !( inode = iget( dir->i_dev, de->inode ) ) ) 
	{
		iput( dir );
		brelse( bh );
		return -EPERM;
	}
	// ����Ŀ¼����������ɾ����־���ҽ��̵���Ч�û�id �����ڸ�i �ڵ���û�id,���ʾû��Ȩ��ɾ
	// ����Ŀ¼,�����ͷŰ���Ҫɾ��Ŀ¼����Ŀ¼i �ڵ�͸�Ҫɾ��Ŀ¼��i �ڵ�,�ͷŸ��ٻ�����,
	// ���س�����
	if ( ( dir->i_mode & S_ISVTX ) &&
		   current->euid && 
		   inode->i_uid != current->euid 
		)
	{
		iput( dir );
		iput( inode );
		brelse( bh );
		return -EPERM;
	}
	// ���Ҫ��ɾ����Ŀ¼���i �ڵ���豸�Ų����ڰ�����Ŀ¼���Ŀ¼���豸��,���߸ñ�ɾ��Ŀ¼��
	// �������Ӽ�������1( ��ʾ�з������ӵ� ),����ɾ����Ŀ¼,�����ͷŰ���Ҫɾ��Ŀ¼����Ŀ¼
	// i �ڵ�͸�Ҫɾ��Ŀ¼��i �ڵ�,�ͷŸ��ٻ�����,���س�����
	if ( inode->i_dev != dir->i_dev || inode->i_count > 1 ) 
	{
		iput( dir );
		iput( inode );

		brelse( bh );

		return -EPERM;
	}

	// ���Ҫ��ɾ��Ŀ¼��Ŀ¼��i �ڵ�Ľڵ�ŵ��ڰ�������ɾ��Ŀ¼��i �ڵ��,���ʾ��ͼɾ��"."
	// Ŀ¼.�����ͷŰ���Ҫɾ��Ŀ¼����Ŀ¼i �ڵ�͸�Ҫɾ��Ŀ¼��i �ڵ�,�ͷŸ��ٻ�����,����
	// ������
	if ( inode == dir )  /* we may not delete ".", but "../dir" is ok */
	{	
		iput( inode );
		iput( dir );
		brelse( bh );
		return -EPERM;
	}

	// ��Ҫ��ɾ����Ŀ¼��i �ڵ�����Ա����ⲻ��һ��Ŀ¼,���ͷŰ���Ҫɾ��Ŀ¼����Ŀ¼i �ڵ��
	// ��Ҫɾ��Ŀ¼��i �ڵ�,�ͷŸ��ٻ�����,���س�����

	if ( !S_ISDIR( inode->i_mode ) ) 
	{
		iput( inode );
		iput( dir );
		brelse( bh );
		return -ENOTDIR;
	}
	// �����豻ɾ����Ŀ¼����,���ͷŰ���Ҫɾ��Ŀ¼����Ŀ¼i �ڵ�͸�Ҫɾ��Ŀ¼��i �ڵ�,�ͷ�
	// ���ٻ�����,���س�����
	if ( !empty_dir( inode ) ) 
	{
		iput( inode );
		iput( dir );
		brelse( bh );
		return -ENOTEMPTY;
	}
	// �����豻ɾ��Ŀ¼��i �ڵ��������������2,����ʾ������Ϣ
	if ( inode->i_nlinks != 2 )
		printk( "empty directory has nlink!=2 ( %d )", inode->i_nlinks );

	// �ø��豻ɾ��Ŀ¼��Ŀ¼���i �ڵ���ֶ�Ϊ0,��ʾ��Ŀ¼���ʹ��,���ú��и�Ŀ¼��ĸ���
	// ���������޸ı�־,���ͷŸû�����
	de->inode	= 0;
	bh->b_dirt	= 1;

	brelse( bh );

	inode->i_nlinks = 0;
	inode->i_dirt = 1;
	dir->i_nlinks--;
	dir->i_ctime = dir->i_mtime = CURRENT_TIME;
	dir->i_dirt	 = 1;

	iput( dir );
	iput( inode );

	return 0;
}

// ϵͳ���ú���- ɾ���ļ����Լ�����Ҳɾ������ص��ļ�.
// ���ļ�ϵͳɾ��һ������.�����һ���ļ������һ������,����û�н������򿪸��ļ�,����ļ�
// Ҳ����ɾ��,���ͷ���ռ�õ��豸�ռ�.
// ����:name - �ļ���.
// ����:�ɹ��򷵻�0,���򷵻س����

LONG sys_unlink( const CHAR * name )
{
	const 
	CHAR		*	basename;
	LONG			namelen;
	M_Inode		*	dir, *inode;
	Buffer_Head *	bh;
	Dir_Entry	*	de;

	if ( !( dir = dir_namei( name, &namelen, &basename ) ) )
		return -ENOENT;

	if ( !namelen ) 
	{
		iput( dir );
		return -ENOENT;
	}

	if ( !permission( dir, MAY_WRITE ) ) 
	{
		iput( dir );
		return -EPERM;
	}
	// �����Ӧ·�����������ļ�����Ŀ¼�����,���ͷŰ�����Ŀ¼��ĸ��ٻ�����,�ͷ�Ŀ¼
	// ��i �ڵ�,�����ļ��Ѿ����ڳ�����,�˳�.����dir �ǰ���Ҫ��ɾ��Ŀ¼����Ŀ¼i �ڵ�,de
	// ��Ҫ��ɾ��Ŀ¼��Ŀ¼��ṹ

	bh = find_entry( &dir, basename, namelen, &de );

	if ( !bh ) 
	{
		iput( dir );
		return -ENOENT;
	}

	// ȡ��Ŀ¼��ָ����i �ڵ�.���������ͷ�Ŀ¼��i �ڵ�,���ͷź���Ŀ¼��ĸ��ٻ�����,
	// ���س����
	if ( !( inode = iget( dir->i_dev, de->inode ) ) ) 
	{
		iput( dir );
		brelse( bh );
		return -ENOENT;
	}

	// �����Ŀ¼����������ɾ����־�����û����ǳ����û�,���ҽ��̵���Ч�û�id �����ڱ�ɾ���ļ�
	// ��i �ڵ���û�id,���ҽ��̵���Ч�û�id Ҳ������Ŀ¼i �ڵ���û�id,��û��Ȩ��ɾ�����ļ�
	// ��.���ͷŸ�Ŀ¼i �ڵ�͸��ļ���Ŀ¼���i �ڵ�,�ͷŰ�����Ŀ¼��Ļ�����,���س����
	if ( ( dir->i_mode & S_ISVTX ) && !suser() &&
		   current->euid != inode->i_uid &&
		   current->euid != dir->i_uid 
		) 
	{
		iput( dir );
		iput( inode );
		brelse( bh );
		return -EPERM;
	}

	// �����ָ���ļ�����һ��Ŀ¼,��Ҳ����ɾ��,�ͷŸ�Ŀ¼i �ڵ�͸��ļ���Ŀ¼���i �ڵ�,
	// �ͷŰ�����Ŀ¼��Ļ�����,���س����
	if ( S_ISDIR( inode->i_mode ) ) 
	{
		iput( inode );
		iput( dir );
		brelse( bh );
		return -EPERM;
	}

	// �����i �ڵ���������Ѿ�Ϊ0,����ʾ������Ϣ,������Ϊ1
	if ( !inode->i_nlinks ) 
	{
		printk( "Deleting nonexistent file ( %04x:%d ), %d\n",
			inode->i_dev, inode->i_num, inode->i_nlinks );
		inode->i_nlinks = 1;
	}
	// �����ļ�����Ŀ¼���е�i �ڵ���ֶ���Ϊ0,��ʾ�ͷŸ�Ŀ¼��,�����ð�����Ŀ¼��Ļ�����
	// ���޸ı�־,�ͷŸø��ٻ�����
	de->inode = 0;
	bh->b_dirt = 1;
	brelse( bh );
	// ��i �ڵ����������1,�����޸ı�־,���¸ı�ʱ��Ϊ��ǰʱ��.����ͷŸ�i �ڵ��Ŀ¼��i ��
	// ��,����0( �ɹ� )
	inode->i_nlinks--;
	inode->i_dirt = 1;
	inode->i_ctime = CURRENT_TIME;
	iput( inode );
	iput( dir );
	return 0;
}

// ϵͳ���ú���- Ϊ�ļ�����һ���ļ���.
// Ϊһ���Ѿ����ڵ��ļ�����һ��������( Ҳ��ΪӲ����- hard link ).
// ����:oldname - ԭ·����;newname - �µ�·����.
// ����:���ɹ��򷵻�0,���򷵻س����
LONG sys_link( const CHAR * oldname, const CHAR * newname )
{
	const CHAR	*	basename;
	Dir_Entry	*	de;
	M_Inode		*	oldinode, *dir;
	Buffer_Head *	bh;
	LONG			namelen;

	// ȡԭ�ļ�·������Ӧ��i �ڵ�oldinode.���Ϊ0,���ʾ����,���س����

	oldinode = namei( oldname );

	if ( !oldinode )
		return -ENOENT;

	// ���ԭ·������Ӧ����һ��Ŀ¼��,���ͷŸ�i �ڵ�,���س����
	if ( S_ISDIR( oldinode->i_mode ) )
	{
		iput( oldinode );
		return -EPERM;
	}

	// ������·���������Ŀ¼��i �ڵ�,�����������ļ������䳤��.���Ŀ¼��i �ڵ�û���ҵ�,
	// ���ͷ�ԭ·������i �ڵ�,���س����
	dir = dir_namei( newname, &namelen, &basename );

	if ( !dir ) 
	{
		iput( oldinode );
		return -EACCES;
	}
	// �����·�����в������ļ���,���ͷ�ԭ·����i �ڵ����·����Ŀ¼��i �ڵ�,���س����
	if ( !namelen ) 
	{
		iput( oldinode );
		iput( dir );
		return -EPERM;
	}

	// �����·����Ŀ¼���豸����ԭ·�������豸�Ų�һ��,��Ҳ���ܽ�������,�����ͷ���·����
	// Ŀ¼��i �ڵ��ԭ·������i �ڵ�,���س����
	if ( dir->i_dev != oldinode->i_dev ) 
	{
		iput( dir );
		iput( oldinode );
		return -EXDEV;
	}

	// ����û�û������Ŀ¼��д��Ȩ��,��Ҳ���ܽ�������,�����ͷ���·����Ŀ¼��i �ڵ�
	// ��ԭ·������i �ڵ�,���س����
	if ( !permission( dir, MAY_WRITE ) )
	{
		iput( dir );
		iput( oldinode );
		return -EACCES;
	}

	// ��ѯ����·�����Ƿ��Ѿ�����,�������,��Ҳ���ܽ�������,�����ͷŰ������Ѵ���Ŀ¼���
	// ���ٻ�����,�ͷ���·����Ŀ¼��i �ڵ��ԭ·������i �ڵ�,���س����
	bh = find_entry( &dir, basename, namelen, &de );

	if ( bh )
	{
		brelse( bh );
		iput( dir );
		iput( oldinode );
		return -EEXIST;
	}

	// ����Ŀ¼�����һ��Ŀ¼��.��ʧ�����ͷŸ�Ŀ¼��i �ڵ��ԭ·������i �ڵ�,���س����
	bh = add_entry( dir, basename, namelen, &de );

	if ( !bh ) 
	{
		iput( dir );
		iput( oldinode );
		return -ENOSPC;
	}
	// �����ʼ���ø�Ŀ¼���i �ڵ�ŵ���ԭ·������i �ڵ��,���ð���������Ŀ¼��ĸ��ٻ�����
	// ���޸ı�־,�ͷŸû�����,�ͷ�Ŀ¼��i �ڵ�
	de->inode = oldinode->i_num;
	bh->b_dirt = 1;
	brelse( bh );
	iput( dir );

	// ��ԭ�ڵ��Ӧ�ü�����1,�޸���ı�ʱ��Ϊ��ǰʱ��,������i �ڵ����޸ı�־,����ͷ�ԭ
	// ·������i �ڵ�,������0( �ɹ� )
	oldinode->i_nlinks++;
	oldinode->i_ctime = CURRENT_TIME;
	oldinode->i_dirt = 1;
	iput( oldinode );

	return 0;
}
