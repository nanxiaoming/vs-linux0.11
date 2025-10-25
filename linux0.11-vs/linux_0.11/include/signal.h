#ifndef _SIGNAL_H
#define _SIGNAL_H

#include <sys\types.h>

typedef LONG sig_atomic_t;
typedef ULONG sigset_t;		/* 32 bits */

#define _NSIG       32
#define NSIG		_NSIG

#define SIGHUP		 1
#define SIGINT		 2
#define SIGQUIT		 3
#define SIGILL		 4
#define SIGTRAP		 5
#define SIGABRT		 6
#define SIGIOT		 6
#define SIGUNUSED	 7
#define SIGFPE		 8
#define SIGKILL		 9
#define SIGUSR1		10
#define SIGSEGV		11
#define SIGUSR2		12
#define SIGPIPE		13
#define SIGALRM		14
#define SIGTERM		15
#define SIGSTKFLT	16
#define SIGCHLD		17
#define SIGCONT		18
#define SIGSTOP		19
#define SIGTSTP		20
#define SIGTTIN		21
#define SIGTTOU		22

/* Ok, I haven't implemented sigactions, but trying to keep headers POSIX */
#define SA_NOCLDSTOP	1
#define SA_NOMASK	0x40000000
#define SA_ONESHOT	0x80000000

#define SIG_BLOCK          0	/* for blocking signals */
#define SIG_UNBLOCK        1	/* for unblocking signals */
#define SIG_SETMASK        2	/* for setting the signal mask */

#define SIG_DFL		( ( VOID ( * )( LONG ) )0 )	/* default signal handling */
#define SIG_IGN		( ( VOID ( * )( LONG ) )1 )	/* ignore signal */

struct sigaction {
	VOID( *sa_handler )( LONG );
	sigset_t sa_mask;
	LONG sa_flags;
	VOID( *sa_restorer )();
};

VOID( *signal( LONG _sig, VOID( *_func )( LONG ) ) )( LONG );
int raise( LONG sig );
int kill( pid_t pid, LONG sig );
int sigaddset( sigset_t *mask, LONG signo );
int sigdelset( sigset_t *mask, LONG signo );
int sigemptyset( sigset_t *mask );
int sigfillset( sigset_t *mask );
int sigismember( sigset_t *mask, LONG signo ); /* 1 - is, 0 - not, -1 error */
int sigpending( sigset_t *set );
int sigprocmask( LONG how, sigset_t *set, sigset_t *oldset );
int sigsuspend( sigset_t *sigmask );
int sigaction( LONG sig, struct sigaction *act, struct sigaction *oldact );

#endif /* _SIGNAL_H */
