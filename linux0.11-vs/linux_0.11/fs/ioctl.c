/*
 *  linux/fs/ioctl.c
 *
 *  ( C ) 1991  Linus Torvalds
 */

#include <string.h>
#include <errno.h>
#include <sys\stat.h>

#include <linux\sched.h>

extern LONG tty_ioctl( LONG dev, LONG cmd, LONG arg );

typedef LONG( *ioctl_ptr )( LONG dev, LONG cmd, LONG arg );

#define NRDEVS ( ( sizeof ( ioctl_table ) )/( sizeof ( ioctl_ptr ) ) )

static ioctl_ptr ioctl_table[] = 
{
	NULL,		/* nodev		*/
	NULL,		/* /dev/mem		*/
	NULL,		/* /dev/fd		*/
	NULL,		/* /dev/hd		*/
	tty_ioctl,	/* /dev/ttyx	*/
	tty_ioctl,	/* /dev/tty		*/
	NULL,		/* /dev/lp		*/
	NULL 		/* named pipes	*/
};

LONG sys_ioctl( ULONG fd, ULONG cmd, ULONG arg )
{
	File * filp;
	LONG dev, mode;

	if ( fd >= NR_OPEN || !( filp = current->filp[ fd ] ) )
	{
		return -EBADF;
	}

	mode = filp->f_inode->i_mode;

	if ( !S_ISCHR( mode ) && !S_ISBLK( mode ) )
	{
		return -EINVAL;
	}

	dev = filp->f_inode->i_zone[ 0 ];

	if ( MAJOR( dev ) >= NRDEVS )
	{
		return -ENODEV;
	}

	if ( !ioctl_table[ MAJOR( dev ) ] )
	{
		return -ENOTTY;
	}

	return ioctl_table[ MAJOR( dev ) ]( dev, cmd, arg );
}
