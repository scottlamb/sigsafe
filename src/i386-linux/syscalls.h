/** @file
 * Lists implemented raw system calls on i386-linux.
 * @legal
 * Copyright &copy; 2004 Scott Lamb &lt;slamb@slamb.org&gt;.
 * This file is part of sigsafe, which is released under the MIT license.
 * @version     $Id$
 * @author      Scott Lamb &lt;slamb@slamb.org&gt;
 */

/* accept goes through socketcall */
/* connect goes through socketcall */
#ifdef SIGSAFE_HAVE_EPOLL
SYSCALL(epoll_wait, 4)
#endif
SYSCALL(nanosleep, 3)
SYSCALL(open, 3)
SYSCALL(poll, 3)
SYSCALL(read, 3)
SYSCALL(readv, 3)
/* recv goes through socketcall */
/* recvfrom goes through socketcall */
/* recvmsg goes through socketcall */
SYSCALL(select, 5)
/* send goes through socketcall */
/* sendmsg goes through socketcall */
/* sendto goes through socketcall */
SYSCALL(socketcall, 2)
SYSCALL(write, 3)
SYSCALL(writev, 3)
SYSCALL(wait4, 4)
