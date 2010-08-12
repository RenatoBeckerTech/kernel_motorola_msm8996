#!/bin/sh
#
# Check if current architecture are missing any function calls compared
# to i386.
# i386 define a number of legacy system calls that are i386 specific
# and listed below so they are ignored.
#
# Usage:
# syscallchk gcc gcc-options
#

ignore_list() {
cat << EOF
#include <asm/types.h>
#include <asm/unistd.h>

/* *at */
#define __IGNORE_open		/* openat */
#define __IGNORE_link		/* linkat */
#define __IGNORE_unlink		/* unlinkat */
#define __IGNORE_mknod		/* mknodat */
#define __IGNORE_chmod		/* fchmodat */
#define __IGNORE_chown		/* fchownat */
#define __IGNORE_mkdir		/* mkdirat */
#define __IGNORE_rmdir		/* unlinkat */
#define __IGNORE_lchown		/* fchownat */
#define __IGNORE_access		/* faccessat */
#define __IGNORE_rename		/* renameat */
#define __IGNORE_readlink	/* readlinkat */
#define __IGNORE_symlink	/* symlinkat */
#define __IGNORE_utimes		/* futimesat */
#if BITS_PER_LONG == 64
#define __IGNORE_stat		/* fstatat */
#define __IGNORE_lstat		/* fstatat */
#else
#define __IGNORE_stat64		/* fstatat64 */
#define __IGNORE_lstat64	/* fstatat64 */
#endif

/* CLOEXEC flag */
#define __IGNORE_pipe		/* pipe2 */
#define __IGNORE_dup2		/* dup3 */
#define __IGNORE_epoll_create	/* epoll_create1 */
#define __IGNORE_inotify_init	/* inotify_init1 */
#define __IGNORE_eventfd	/* eventfd2 */
#define __IGNORE_signalfd	/* signalfd4 */

/* MMU */
#ifndef CONFIG_MMU
#define __IGNORE_madvise
#define __IGNORE_mbind
#define __IGNORE_mincore
#define __IGNORE_mlock
#define __IGNORE_mlockall
#define __IGNORE_munlock
#define __IGNORE_munlockall
#define __IGNORE_mprotect
#define __IGNORE_msync
#define __IGNORE_migrate_pages
#define __IGNORE_move_pages
#define __IGNORE_remap_file_pages
#define __IGNORE_get_mempolicy
#define __IGNORE_set_mempolicy
#define __IGNORE_swapoff
#define __IGNORE_swapon
#endif

/* System calls for 32-bit kernels only */
#if BITS_PER_LONG == 64
#define __IGNORE_sendfile64
#define __IGNORE_ftruncate64
#define __IGNORE_truncate64
#define __IGNORE_stat64
#define __IGNORE_lstat64
#define __IGNORE_fstat64
#define __IGNORE_fcntl64
#define __IGNORE_fadvise64_64
#define __IGNORE_fstatat64
#define __IGNORE_fstatfs64
#define __IGNORE_statfs64
#define __IGNORE_llseek
#define __IGNORE_mmap2
#else
#define __IGNORE_sendfile
#define __IGNORE_ftruncate
#define __IGNORE_truncate
#define __IGNORE_stat
#define __IGNORE_lstat
#define __IGNORE_fstat
#define __IGNORE_fcntl
#define __IGNORE_fadvise64
#define __IGNORE_newfstatat
#define __IGNORE_fstatfs
#define __IGNORE_statfs
#define __IGNORE_lseek
#define __IGNORE_mmap
#endif

/* i386-specific or historical system calls */
#define __IGNORE_break
#define __IGNORE_stty
#define __IGNORE_gtty
#define __IGNORE_ftime
#define __IGNORE_prof
#define __IGNORE_lock
#define __IGNORE_mpx
#define __IGNORE_ulimit
#define __IGNORE_profil
#define __IGNORE_ioperm
#define __IGNORE_iopl
#define __IGNORE_idle
#define __IGNORE_modify_ldt
#define __IGNORE_ugetrlimit
#define __IGNORE_vm86
#define __IGNORE_vm86old
#define __IGNORE_set_thread_area
#define __IGNORE_get_thread_area
#define __IGNORE_madvise1
#define __IGNORE_oldstat
#define __IGNORE_oldfstat
#define __IGNORE_oldlstat
#define __IGNORE_oldolduname
#define __IGNORE_olduname
#define __IGNORE_umount
#define __IGNORE_waitpid
#define __IGNORE_stime
#define __IGNORE_nice
#define __IGNORE_signal
#define __IGNORE_sigaction
#define __IGNORE_sgetmask
#define __IGNORE_sigsuspend
#define __IGNORE_sigpending
#define __IGNORE_ssetmask
#define __IGNORE_readdir
#define __IGNORE_socketcall
#define __IGNORE_ipc
#define __IGNORE_sigreturn
#define __IGNORE_sigprocmask
#define __IGNORE_bdflush
#define __IGNORE__llseek
#define __IGNORE__newselect
#define __IGNORE_create_module
#define __IGNORE_query_module
#define __IGNORE_get_kernel_syms
#define __IGNORE_sysfs
#define __IGNORE_uselib
#define __IGNORE__sysctl

/* ... including the "new" 32-bit uid syscalls */
#define __IGNORE_lchown32
#define __IGNORE_getuid32
#define __IGNORE_getgid32
#define __IGNORE_geteuid32
#define __IGNORE_getegid32
#define __IGNORE_setreuid32
#define __IGNORE_setregid32
#define __IGNORE_getgroups32
#define __IGNORE_setgroups32
#define __IGNORE_fchown32
#define __IGNORE_setresuid32
#define __IGNORE_getresuid32
#define __IGNORE_setresgid32
#define __IGNORE_getresgid32
#define __IGNORE_chown32
#define __IGNORE_setuid32
#define __IGNORE_setgid32
#define __IGNORE_setfsuid32
#define __IGNORE_setfsgid32

/* these can be expressed using other calls */
#define __IGNORE_alarm		/* setitimer */
#define __IGNORE_creat		/* open */
#define __IGNORE_fork		/* clone */
#define __IGNORE_futimesat	/* utimensat */
#define __IGNORE_getpgrp	/* getpgid */
#define __IGNORE_getdents	/* getdents64 */
#define __IGNORE_pause		/* sigsuspend */
#define __IGNORE_poll		/* ppoll */
#define __IGNORE_select		/* pselect6 */
#define __IGNORE_epoll_wait	/* epoll_pwait */
#define __IGNORE_time		/* gettimeofday */
#define __IGNORE_uname		/* newuname */
#define __IGNORE_ustat		/* statfs */
#define __IGNORE_utime		/* utimes */
#define __IGNORE_vfork		/* clone */

/* sync_file_range had a stupid ABI. Allow sync_file_range2 instead */
#ifdef __NR_sync_file_range2
#define __IGNORE_sync_file_range
#endif

/* Unmerged syscalls for AFS, STREAMS, etc. */
#define __IGNORE_afs_syscall
#define __IGNORE_getpmsg
#define __IGNORE_putpmsg
#define __IGNORE_vserver
EOF
}

syscall_list() {
sed -n -e '/^\#define/ s/[^_]*__NR_\([^[:space:]]*\).*/\
\#if !defined \(__NR_\1\) \&\& !defined \(__IGNORE_\1\)\
\#warning syscall \1 not implemented\
\#endif/p' $1
}

(ignore_list && syscall_list ${srctree}/arch/x86/include/asm/unistd_32.h) | \
$* -E -x c - > /dev/null
