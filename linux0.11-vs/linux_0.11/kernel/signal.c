/*
*  linux/kernel/signal.c
*
*  ( C ) 1991  Linus Torvalds
*/

#include <linux\sched.h>
#include <linux\kernel.h>
#include <asm\segment.h>

#include <signal.h>

volatile VOID do_exit( LONG error_code );

// ��ȡ��ǰ�����ź�����λͼ( ������ )
LONG sys_sgetmask()
{
	return current->blocked;
}

// �����µ��ź�����λͼ.SIGKILL ���ܱ�����.����ֵ��ԭ�ź�����λͼ
LONG sys_ssetmask( LONG newmask )
{
	LONG old = current->blocked;

	current->blocked = newmask & ~( 1 << ( SIGKILL - 1 ) );
	return old;
}

// ����sigaction ���ݵ�fs ���ݶ�to ��

static __inline VOID save_old( CHAR * from, CHAR * to )
{
	LONG i;

	verify_area( to, sizeof( struct sigaction ) );
	for ( i = 0; i < sizeof( struct sigaction ); i++ ) {
		put_fs_byte( *from, to );
		from++;
		to++;
	}
}

// ��sigaction ���ݴ�fs ���ݶ�from λ�ø��Ƶ�to ��
static __inline VOID get_new( CHAR * from, CHAR * to )
{
	LONG i;

	for ( i = 0; i < sizeof( struct sigaction ); i++ )
		*( to++ ) = get_fs_byte( from++ );
}

// signal()ϵͳ����.������sigaction().Ϊָ�����źŰ�װ�µ��źž��( �źŴ������ ).
// �źž���������û�ָ���ĺ���,Ҳ������SIG_DFL( Ĭ�Ͼ�� )��SIG_IGN( ���� ).
// ����signum --ָ�����ź�;handler -- ָ���ľ��;restorer �Cԭ����ǰִ�еĵ�ַλ��.
// ��������ԭ�źž��
LONG sys_signal( LONG signum, LONG handler, LONG restorer )
{
	struct sigaction tmp;

	if ( signum<1 || signum>32 || signum == SIGKILL )	// �ź�ֵҪ��( 1-32 )��Χ��
		return -1;
	tmp.sa_handler = ( VOID( * )( LONG ) ) handler;		// ָ�����źŴ�����
	tmp.sa_mask = 0;								// ִ��ʱ���ź�������.
	tmp.sa_flags = SA_ONESHOT | SA_NOMASK;			// �þ��ֻʹ��1 �κ�ͻָ���Ĭ��ֵ,�������ź����Լ��Ĵ��������յ�
	tmp.sa_restorer = ( VOID( * )() ) restorer;		// ���淵�ص�ַ
	handler = ( LONG )current->sigaction[ signum - 1 ].sa_handler;
	current->sigaction[ signum - 1 ] = tmp;
	return handler;
}

// sigaction()ϵͳ����.�ı�������յ�һ���ź�ʱ�Ĳ���.signum �ǳ���SIGKILL ������κ�
// �ź�.[ ����²���( action )��Ϊ�� ]���²�������װ.���oldaction ָ�벻Ϊ��,��ԭ����
// ��������oldaction.�ɹ��򷵻�0,����Ϊ-1
LONG sys_sigaction( LONG signum, const struct sigaction * action,
struct sigaction * oldaction )
{
	struct sigaction tmp;

	if ( signum<1 || signum>32 || signum == SIGKILL )
		return -1;
	tmp = current->sigaction[ signum - 1 ];
	get_new( ( CHAR * )action,
		( CHAR * )( signum - 1 + current->sigaction ) );
	if ( oldaction )
		save_old( ( CHAR * )&tmp, ( CHAR * )oldaction );
	// ��������ź����Լ����źž�����յ�,����������Ϊ0,�����������α��ź�
	if ( current->sigaction[ signum - 1 ].sa_flags & SA_NOMASK )
		current->sigaction[ signum - 1 ].sa_mask = 0;
	else
		current->sigaction[ signum - 1 ].sa_mask |= ( 1 << ( signum - 1 ) );
	return 0;
}
// ϵͳ�����жϴ���������������źŴ������( ��kernel/system_call.s,119 �� ).
// �öδ������Ҫ�����ǽ��źŵĴ��������뵽�û������ջ��,���ڱ�ϵͳ���ý���
// ���غ�����ִ���źž������,Ȼ�����ִ���û��ĳ���
VOID do_signal( LONG signr, LONG eax, LONG ebx, LONG ecx, LONG edx,
	LONG fs, LONG es, LONG ds,
	LONG eip, LONG cs, LONG eflags,
	ULONG * esp, LONG ss )
{
	ULONG sa_handler;
	LONG old_eip = eip;
	struct sigaction * sa = current->sigaction + signr - 1;
	LONG longs;
	ULONG * tmp_esp;

	sa_handler = ( ULONG )sa->sa_handler;
	// ����źž��ΪSIG_IGN( ���� ),�򷵻�;������ΪSIG_DFL( Ĭ�ϴ��� ),������ź���
	// SIGCHLD �򷵻�,������ֹ���̵�ִ��
	if ( sa_handler == 1 )
		return;
	if ( !sa_handler ) {
		if ( signr == SIGCHLD )
			return;
		else
			// ����Ӧ����do_exit( 1<<signr ) ).
			do_exit( 1 << ( signr - 1 ) );
	}

	// ������źž��ֻ��ʹ��һ��,�򽫸þ���ÿ�( ���źž���Ѿ�������sa_handler ָ���� ).
	if ( sa->sa_flags & SA_ONESHOT )
		sa->sa_handler = NULL;
	// ������δ��뽫�źŴ��������뵽�û���ջ��,ͬʱҲ��sa_restorer,signr,����������( ���
	// SA_NOMASK û��λ ),eax,ecx,edx ��Ϊ�����Լ�ԭ����ϵͳ���õĳ��򷵻�ָ�뼰��־�Ĵ���ֵ
	// ѹ���ջ.����ڱ���ϵͳ�����ж�( 0x80 )�����û�����ʱ������ִ���û����źž������,Ȼ��
	// �ټ���ִ���û�����.
	// ���û�����ϵͳ���õĴ���ָ��eip ָ����źŴ�����.
	*( &eip ) = sa_handler;
	// ��������ź��Լ��Ĵ������յ��ź��Լ�,��Ҳ��Ҫ�����̵�������ѹ���ջ
	longs = ( sa->sa_flags & SA_NOMASK ) ? 7 : 8;
	// ��ԭ���ó�����û��Ķ�ջָ��������չ7( ��8 )������( ������ŵ����źž���Ĳ����� ),
	// ������ڴ�ʹ�����( ��������ڴ泬���������ҳ�� ).

	*( &esp ) -= longs;
	verify_area( esp, longs * 4 );
	// ���û���ջ�д��µ��ϴ��sa_restorer, �ź�signr, ������blocked( ���SA_NOMASK ��λ ),
	// eax, ecx, edx, eflags ���û�����ԭ����ָ��
	tmp_esp = esp;
	put_fs_long( ( LONG )sa->sa_restorer, tmp_esp++ );
	put_fs_long( signr, tmp_esp++ );
	if ( !( sa->sa_flags & SA_NOMASK ) )
		put_fs_long( current->blocked, tmp_esp++ );
	put_fs_long( eax, tmp_esp++ );
	put_fs_long( ecx, tmp_esp++ );
	put_fs_long( edx, tmp_esp++ );
	put_fs_long( eflags, tmp_esp++ );
	put_fs_long( old_eip, tmp_esp++ );
	current->blocked |= sa->sa_mask;// ����������( ������ )����sa_mask �е���λ
}
