/** @file
 * Lists implemented raw system calls on alpha-linux.
 * @legal
 * Copyright &copy; 2004 Scott Lamb &lt;slamb@slamb.org&gt;.
 * This file is part of sigsafe, which is released under the MIT license.
 * @version     $Id$
 * @author      Scott Lamb &lt;slamb@slamb.org&gt;
 */

SYSCALL(accept, 2)
SYSCALL(connect, 3)
#ifdef SIGSAFE_HAVE_EPOLL
SYSCALL(epoll_wait, 4)
#endif
SYSCALL(nanosleep, 3)
SYSCALL(open, 3)
SYSCALL(poll, 3)
SYSCALL(read, 3)
SYSCALL(readv, 3)
SYSCALL(recv, 4)
SYSCALL(recvmsg, 3)
SYSCALL(recvfrom, 6)
SYSCALL(select, 5)
SYSCALL(send, 4)
SYSCALL(sendmsg, 3)
SYSCALL(sendto, 6)
SYSCALL(wait4, 4)
SYSCALL(write, 3)
SYSCALL(writev, 3)
