/** @file
 * Lists implemented raw system calls on i386-linux.
 * @legal
 * Copyright &copy; 2004 Scott Lamb &lt;slamb@slamb.org&gt;.
 * This file is part of sigsafe, which is released under the MIT license.
 * @version     $Id$
 * @author      Scott Lamb &lt;slamb@slamb.org&gt;
 */

SYSCALL(nanosleep, 3)
SYSCALL(read, 3)
SYSCALL(readv, 3)
SYSCALL(write, 3)
SYSCALL(writev, 3)
SYSCALL(select, 5)
SYSCALL(poll, 3)
#ifdef HAVE_EPOLL
SYSCALL(epoll_wait, 4)
#endif
SYSCALL(wait4, 4)
SYSCALL(socketcall, 2)
