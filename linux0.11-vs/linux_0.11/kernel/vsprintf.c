/*
*  linux/kernel/vsprintf.c
*
*  ( C ) 1991  Linus Torvalds
*/

/* vsprintf.c -- Lars Wirzenius & Linus Torvalds. */
/*
* Wirzenius wrote this portably, Torvalds fucked it up :- )
*/

#include <stdarg.h>
#include <string.h>
#include <sys\types.h>

/* we use this so that we can do without the ctype library */
#define is_digit( c )	( ( c ) >= '0' && ( c ) <= '9' )

static LONG skip_atoi( const CHAR **s )
{
	LONG i = 0;

	while ( is_digit( **s ) )
		i = i * 10 + *( ( *s )++ ) - '0';
	return i;
}

#define ZEROPAD	1		/* pad with zero */
#define SIGN	2		/* unsigned/signed LONG */
#define PLUS	4		/* show plus */
#define SPACE	8		/* space if plus */
#define LEFT	16		/* left justified */
#define SPECIAL	32		/* 0x */
#define SMALL	64		/* use 'abcdef' instead of 'ABCDEF' */

__inline LONG do_div( ULONG *n, LONG base )
{
	LONG __res = *n % base;
	*n /= base;
	return __res;
}

static CHAR * number( CHAR * str, LONG num, LONG base, LONG size, LONG precision
	, LONG type )
{
	CHAR			c, sign, tmp[ 36 ];
	const CHAR *	digits = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	LONG			i;

	if ( type & SMALL )
	{
		digits = "0123456789abcdefghijklmnopqrstuvwxyz";
	}
	if ( type & LEFT ) 
	{
		type &= ~ZEROPAD;
	}
	if ( base < 2 || base > 36 )
	{
		return 0;
	}

	c = ( type & ZEROPAD ) ? '0' : ' ';

	if ( type & SIGN && num < 0 ) 
	{
		sign = '-';
		num = -num;
	}
	else
	{
		sign = ( type & PLUS ) ? '+' : ( ( type & SPACE ) ? ' ' : 0 );
	}
		
	if ( sign ) 
	{
		size--;
	}

	if ( type & SPECIAL )
	{
		if ( base == 16 ) 
		{
			size -= 2;
		}
		else if ( base == 8 ) 
		{
			size--;
		}
	}

	i = 0;

	if ( num == 0 )
	{
		tmp[ i++ ] = '0';
	}
	else while ( num != 0 )
	{
		tmp[ i++ ] = digits[ do_div( &num, base ) ];
	}

	if ( i > precision ) 
	{
		precision = i;
	}

	size -= precision;

	if ( !( type & ( ZEROPAD + LEFT ) ) )
	{
		while ( size-- > 0 )
		{
			*str++ = ' ';
		}
	}
	if ( sign )
	{
		*str++ = sign;
	}
	if ( type & SPECIAL )
	{
		if ( base == 8 )
		{
			*str++ = '0';
		}
		else if ( base == 16 ) 
		{
			*str++ = '0';
			*str++ = digits[ 33 ];
		}
	}
	if ( !( type & LEFT ) )
	{
		while ( size-- > 0 )
		{
			*str++ = c;
		}
	}

	while ( i<precision-- )
	{
		*str++ = '0';
	}

	while ( i-->0 )
	{
		*str++ = tmp[ i ];
	}

	while ( size-- > 0 )
	{
		*str++ = ' ';
	}
	return str;
}

LONG vsprintf( CHAR *buf, const CHAR *fmt, va_list args )
{
	LONG		len;
	LONG		i;
	CHAR *		str;
	CHAR *		s;
	LONG *		ip;
	LONG		flags;			/* flags to number() */
	LONG		field_width;	/* width of output field */
	LONG		precision;		/* min. # of digits for integers; max number of chars for from string */
	LONG		qualifier;		/* 'h', 'l', or 'L' for integer fields */

	for ( str = buf; *fmt; ++fmt )
	{
		if ( *fmt != '%' ) 
		{
			*str++ = *fmt;
			continue;
		}

		/* process flags */
		flags = 0;
repeat:
		++fmt;		/* this also skips first '%' */

		switch ( *fmt ) 
		{
		case '-': 
			flags |= LEFT;
			goto repeat;
		case '+': 
			flags |= PLUS;
			goto repeat;
		case ' ':
			flags |= SPACE; 
			goto repeat;
		case '#': 
			flags |= SPECIAL; 
			goto repeat;
		case '0': 
			flags |= ZEROPAD;
			goto repeat;
		}

		/* get field width */
		field_width = -1;

		if ( is_digit( *fmt ) )
		{
			field_width = skip_atoi( &fmt );
		}
		else if ( *fmt == '*' ) 
		{
			/* it's the next argument */
			++fmt;

			field_width = va_arg( args, LONG );

			if ( field_width < 0 ) 
			{
				field_width = -field_width;
				flags      |= LEFT;
			}
		}

		/* get the precision */
		precision = -1;

		if ( *fmt == '.' ) 
		{
			++fmt;

			if ( is_digit( *fmt ) )
			{
				precision = skip_atoi( &fmt );
			}
			else if ( *fmt == '*' ) 
			{
				/* it's the next argument */
				++fmt;
				precision = va_arg( args, LONG );
			}
			if ( precision < 0 )
			{
				precision = 0;
			}
		}

		/* get the conversion qualifier */
		qualifier = -1;

		if ( *fmt == 'h' || *fmt == 'l' || *fmt == 'L' ) 
		{
			qualifier = *fmt;
			++fmt;
		}

		switch ( *fmt )
		{
		case 'c':
			if ( !( flags & LEFT ) )
			{
				while ( --field_width > 0 )
				{
					*str++ = ' ';
				}
			}

			*str++ = ( UCHAR )va_arg( args, LONG );

			while ( --field_width > 0 )
			{
				*str++ = ' ';
			}
			break;

		case 's':

			s = va_arg( args, CHAR * );

			len = strlen( s );

			if ( precision < 0 )
			{
				precision = len;
			}
			else if ( len > precision )
			{
				len = precision;
			}

			if ( !( flags & LEFT ) )
			{
				while ( len < field_width-- )
				{
					*str++ = ' ';
				}
			}

			for ( i = 0; i < len; ++i )
			{
				*str++ = *s++;
			}

			while ( len < field_width-- )
			{
				*str++ = ' ';
			}
			break;

		case 'o':

			str = number( str, va_arg( args, ULONG ), 8 ,field_width, precision, flags );
			break;

		case 'p':

			if ( field_width == -1 ) 
			{
				field_width = 8;
				flags |= ZEROPAD;
			}

			str = number( str,( ULONG )va_arg( args, VOID * ), 16,field_width, precision, flags );
			break;

		case 'x':

			flags |= SMALL;

		case 'X':

			str = number( str, va_arg( args, ULONG ), 16,field_width, precision, flags );
			break;

		case 'd':
		case 'i':

			flags |= SIGN;

		case 'u':

			str = number( str, va_arg( args, ULONG ), 10,field_width, precision, flags );
			break;

		case 'n':

			ip  = va_arg( args, LONG * );
			*ip = ( str - buf );
			break;

		default:
			if ( *fmt != '%' )
			{
				*str++ = '%';
			}
			if ( *fmt )
			{
				*str++ = *fmt;
			}
			else
			{
				--fmt;
			}
			break;
		}
	}

	*str = '\0';
	return str - buf;
}
