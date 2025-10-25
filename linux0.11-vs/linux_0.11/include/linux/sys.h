extern LONG sys_setup();
extern LONG sys_exit();
extern LONG sys_fork();
extern LONG sys_read();
extern LONG sys_write();
extern LONG sys_open();
extern LONG sys_close();
extern LONG sys_waitpid();
extern LONG sys_creat();
extern LONG sys_link();
extern LONG sys_unlink();
extern LONG sys_execve();
extern LONG sys_chdir();
extern LONG sys_time();
extern LONG sys_mknod();
extern LONG sys_chmod();
extern LONG sys_chown();
extern LONG sys_break();
extern LONG sys_stat();
extern LONG sys_lseek();
extern LONG sys_getpid();
extern LONG sys_mount();
extern LONG sys_umount();
extern LONG sys_setuid();
extern LONG sys_getuid();
extern LONG sys_stime();
extern LONG sys_ptrace();
extern LONG sys_alarm();
extern LONG sys_fstat();
extern LONG sys_pause();
extern LONG sys_utime();
extern LONG sys_stty();
extern LONG sys_gtty();
extern LONG sys_access();
extern LONG sys_nice();
extern LONG sys_ftime();
extern LONG sys_sync();
extern LONG sys_kill();
extern LONG sys_rename();
extern LONG sys_mkdir();
extern LONG sys_rmdir();
extern LONG sys_dup();
extern LONG sys_pipe();
extern LONG sys_times();
extern LONG sys_prof();
extern LONG sys_brk();
extern LONG sys_setgid();
extern LONG sys_getgid();
extern LONG sys_signal();
extern LONG sys_geteuid();
extern LONG sys_getegid();
extern LONG sys_acct();
extern LONG sys_phys();
extern LONG sys_lock();
extern LONG sys_ioctl();
extern LONG sys_fcntl();
extern LONG sys_mpx();
extern LONG sys_setpgid();
extern LONG sys_ulimit();
extern LONG sys_uname();
extern LONG sys_umask();
extern LONG sys_chroot();
extern LONG sys_ustat();
extern LONG sys_dup2();
extern LONG sys_getppid();
extern LONG sys_getpgrp();
extern LONG sys_setsid();
extern LONG sys_sigaction();
extern LONG sys_sgetmask();
extern LONG sys_ssetmask();
extern LONG sys_setreuid();
extern LONG sys_setregid();

fn_ptr sys_call_table[] = { sys_setup, sys_exit, sys_fork, sys_read,
sys_write, sys_open, sys_close, sys_waitpid, sys_creat, sys_link,
sys_unlink, sys_execve, sys_chdir, sys_time, sys_mknod, sys_chmod,
sys_chown, sys_break, sys_stat, sys_lseek, sys_getpid, sys_mount,
sys_umount, sys_setuid, sys_getuid, sys_stime, sys_ptrace, sys_alarm,
sys_fstat, sys_pause, sys_utime, sys_stty, sys_gtty, sys_access,
sys_nice, sys_ftime, sys_sync, sys_kill, sys_rename, sys_mkdir,
sys_rmdir, sys_dup, sys_pipe, sys_times, sys_prof, sys_brk, sys_setgid,
sys_getgid, sys_signal, sys_geteuid, sys_getegid, sys_acct, sys_phys,
sys_lock, sys_ioctl, sys_fcntl, sys_mpx, sys_setpgid, sys_ulimit,
sys_uname, sys_umask, sys_chroot, sys_ustat, sys_dup2, sys_getppid,
sys_getpgrp, sys_setsid, sys_sigaction, sys_sgetmask, sys_ssetmask,
sys_setreuid, sys_setregid };
