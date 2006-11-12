/** @file
 * Lists implemented raw system calls on i386-freebsd.
 * @legal
 * Copyright &copy; 2004 Scott Lamb &lt;slamb@slamb.org&gt;.
 * This file is part of sigsafe, which is released under the MIT license.
 * @version     $Id$
 * @author      Scott Lamb &lt;slamb@slamb.org&gt;
 */

SYSCALL(accept, 2)
SYSCALL(connect, 3)
#ifdef SIGSAFE_HAVE_KEVENT
SYSCALL(kevent, 6)
#endif
SYSCALL(nanosleep, 3)
SYSCALL(open, 3)
SYSCALL(poll, 3)
/* recv is emulated */
SYSCALL(recvfrom, 6)
SYSCALL(recvmsg, 3)
SYSCALL(read, 3)
SYSCALL(readv, 3)
SYSCALL(select, 5)
/* send is emulated */
SYSCALL(sendto, 6)
SYSCALL(sendmsg, 3)
SYSCALL(write, 3)
SYSCALL(writev, 3)
SYSCALL(wait4, 4)
