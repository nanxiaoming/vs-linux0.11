/*
*  linux/kernel/mktime.c
*
*  ( C ) 1991  Linus Torvalds
*/

#include <time.h>

/*
* This isn't the library routine, it is only used in the kernel.
* as such, we don't care about years<1970 etc, but assume everything
* is ok. Similarly, TZ etc is happily ignored. We just do everything
* as easily as possible. Let's find something public for the library
* routines ( although I think minix times is public ).
*/
/*
* PS. I hate whoever though up the year 1970 - couldn't they have gotten
* a leap-year instead? I also hate Gregorius, pope or no. I'm grumpy.
*/
/*
 * �ⲻ�ǿ⺯��,�������ں�ʹ��.������ǲ�����С��1970 �����ݵ�,���ٶ�һ�о�������.
 * ͬ��,ʱ������TZ ����Ҳ�Ⱥ���.����ֻ�Ǿ����ܼ򵥵ش�������.������ҵ�һЩ�����Ŀ⺯��
 * ( ��������Ϊminix ��ʱ�亯���ǹ����� ).
 * ����,�Һ��Ǹ�����1970 �꿪ʼ���� - �ѵ����ǾͲ���ѡ���һ�����꿪ʼ?�Һ޸����������
 * ����̻ʡ�����,��ʲô�����ں�.���Ǹ�Ƣ���������.
 */
#define MINUTE 60
#define HOUR ( 60*MINUTE )
#define DAY ( 24*HOUR )
#define YEAR ( 365*DAY )

/* interestingly, we assume leap-years */
/* ��Ȥ�������ǿ��ǽ������� */
// ��������Ϊ����,������ÿ���¿�ʼʱ������ʱ������.
static LONG month[ 12 ] = {
	0,
	DAY*( 31 ),
	DAY*( 31 + 29 ),
	DAY*( 31 + 29 + 31 ),
	DAY*( 31 + 29 + 31 + 30 ),
	DAY*( 31 + 29 + 31 + 30 + 31 ),
	DAY*( 31 + 29 + 31 + 30 + 31 + 30 ),
	DAY*( 31 + 29 + 31 + 30 + 31 + 30 + 31 ),
	DAY*( 31 + 29 + 31 + 30 + 31 + 30 + 31 + 31 ),
	DAY*( 31 + 29 + 31 + 30 + 31 + 30 + 31 + 31 + 30 ),
	DAY*( 31 + 29 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31 ),
	DAY*( 31 + 29 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31 + 30 )
};

// �ú��������1970 ��1 ��1 ��0 ʱ�𵽿������վ���������,��Ϊ����ʱ��
LONG kernel_mktime( struct tm * tm )
{
	LONG res;
	LONG year;

	year = tm->tm_year - 70;	// �� 1970 �굽���ھ���������,ֻ��ʶ��19XX��������

	/* magic offsets ( y+1 ) needed to get leapyears right.*/
	/* Ϊ�˻����ȷ��������,������Ҫ����һ��ħ��ƫֵ( y+1 ) */
	res = YEAR*year + DAY*( ( year + 1 ) / 4 );	// ��Щ�꾭��������ʱ�� + ÿ������ʱ��1 ��
	res += month[ tm->tm_mon ];				// ������ʱ��,�ڼ��ϵ��굽����ʱ������.
	/* and ( y+2 ) here. If it wasn't a leap-year, we have to adjust */
	if ( tm->tm_mon > 1 && ( ( year + 2 ) % 4 ) )
		res -= DAY;
	res += DAY*( tm->tm_mday - 1 );	// �ټ��ϱ��¹�ȥ������������ʱ��.
	res += HOUR*tm->tm_hour;		// �ټ��ϵ����ȥ��Сʱ��������ʱ��.
	res += MINUTE*tm->tm_min;		// �ټ���1 Сʱ�ڹ�ȥ�ķ�����������ʱ��.
	res += tm->tm_sec;				// �ټ���1 �������ѹ�������.
	return res;						// �����ڴ�1970 ����������������ʱ��.
}
