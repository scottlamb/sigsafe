/** @file
 * Lists implemented raw system calls on x86_64-linux.
 * @legal
 * Copyright &copy; 2004 Scott Lamb &lt;slamb@slamb.org&gt;.
 * This file is part of sigsafe, which is released under the MIT license.
 * @version     $Id$
 * @author      Scott Lamb &lt;slamb@slamb.org&gt;
 */

SYSCALL(accept, 3)
SYSCALL(connect, 3)
#ifdef SIGSAFE_HAVE_EPOLL
SYSCALL(epoll_wait, 4)
#endif
SYSCALL(nanosleep, 3)
SYSCALL(pause, 0)
SYSCALL(poll, 3)
SYSCALL(read, 3)
SYSCALL(readv, 3)
/* recv is emulated */
SYSCALL(recvfrom, 6)
SYSCALL(recvmsg, 3)
SYSCALL(select, 5)
/* send is emulated */
SYSCALL(sendto, 6)
SYSCALL(sendmsg, 3)
#define __NR_sigsuspend __NR_rt_sigsuspend
SYSCALL(sigsuspend, 1)
SYSCALL(write, 3)
SYSCALL(writev, 3)
SYSCALL(wait4, 4)
