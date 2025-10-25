#ifndef _SYS_STAT_H
#define _SYS_STAT_H

#include <sys/types.h>

struct stat 
{
	dev_t	st_dev;			// Offset=0x0 Size=0x2
	ino_t	st_ino;			// Offset=0x2 Size=0x2
	umode_t	st_mode;		// Offset=0x4 Size=0x2
	nlink_t	st_nlink;		// Offset=0x6 Size=0x1
	uid_t	st_uid;			// Offset=0x8 Size=0x2
	gid_t	st_gid;			// Offset=0xa Size=0x1
	dev_t	st_rdev;		// Offset=0xc Size=0x2
	off_t	st_size;		// Offset=0x10 Size=0x4
	time_t	st_atime;		// Offset=0x14 Size=0x4
	time_t	st_mtime;		// Offset=0x18 Size=0x4
	time_t	st_ctime;		// Offset=0x1c Size=0x4
};

#define S_IFMT  00170000
#define S_IFREG  0100000
#define S_IFBLK  0060000
#define S_IFDIR  0040000
#define S_IFCHR  0020000
#define S_IFIFO  0010000
#define S_ISUID  0004000
#define S_ISGID  0002000
#define S_ISVTX  0001000

#define S_ISREG( m )	( ( ( m ) & S_IFMT ) == S_IFREG )
#define S_ISDIR( m )	( ( ( m ) & S_IFMT ) == S_IFDIR )
#define S_ISCHR( m )	( ( ( m ) & S_IFMT ) == S_IFCHR )
#define S_ISBLK( m )	( ( ( m ) & S_IFMT ) == S_IFBLK )
#define S_ISFIFO( m )	( ( ( m ) & S_IFMT ) == S_IFIFO )

#define S_IRWXU 00700
#define S_IRUSR 00400
#define S_IWUSR 00200
#define S_IXUSR 00100

#define S_IRWXG 00070
#define S_IRGRP 00040
#define S_IWGRP 00020
#define S_IXGRP 00010

#define S_IRWXO 00007
#define S_IROTH 00004
#define S_IWOTH 00002
#define S_IXOTH 00001

extern int chmod( const CHAR *_path, mode_t mode );
extern int fstat( LONG fildes, struct stat *stat_buf );
extern int mkdir( const CHAR *_path, mode_t mode );
extern int mkfifo( const CHAR *_path, mode_t mode );
extern int stat( const CHAR *filename, struct stat *stat_buf );
extern mode_t umask( mode_t mask );

#endif
