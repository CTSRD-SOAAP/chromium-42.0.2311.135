/*
 * Copyright 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_NONSFI_LINUX_LINUX_SYSCALL_DEFINES_H_
#define NATIVE_CLIENT_SRC_NONSFI_LINUX_LINUX_SYSCALL_DEFINES_H_ 1

#define CLONE_VM             0x00000100
#define CLONE_FS             0x00000200
#define CLONE_FILES          0x00000400
#define CLONE_SIGHAND        0x00000800
#define CLONE_THREAD         0x00010000
#define CLONE_SYSVSEM        0x00040000
#define CLONE_SETTLS         0x00080000

#define FUTEX_WAIT_PRIVATE 128
#define FUTEX_WAKE_PRIVATE 129

#define LINUX_TCGETS 0x5401

#define LINUX_SIG_UNBLOCK 1

#define LINUX_SA_SIGINFO 0x00000004
#define LINUX_SA_RESTART 0x10000000
#define LINUX_SA_ONSTACK 0x08000000

/* From linux/arch/{arch}/include/uapi/asm/signal.h */
#if defined(__mips__)
/*
 * We at least know the numbers are different on MIPS. This part would
 * be guarded by #if defined(__i386__) || defined(__arm__), however
 * this file is included by code which does not have --target set.
 */
#error "Unsupported architecture"
#endif
#define LINUX_SIGHUP           1
#define LINUX_SIGINT           2
#define LINUX_SIGQUIT          3
#define LINUX_SIGILL           4
#define LINUX_SIGTRAP          5
#define LINUX_SIGABRT          6
#define LINUX_SIGBUS           7
#define LINUX_SIGFPE           8
#define LINUX_SIGKILL          9
#define LINUX_SIGUSR1         10
#define LINUX_SIGSEGV         11
#define LINUX_SIGUSR2         12
#define LINUX_SIGPIPE         13
#define LINUX_SIGALRM         14
#define LINUX_SIGTERM         15
#define LINUX_SIGSTKFLT       16
#define LINUX_SIGCHLD         17
#define LINUX_SIGSYS          31

#endif
