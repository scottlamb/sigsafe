/** @file
 * Lists implemented raw system calls on ia64-hpux.
 * @legal
 * Copyright &copy; 2004 Scott Lamb &lt;slamb@slamb.org&gt;.
 * This file is part of sigsafe, which is released under the MIT license.
 * @version     $Id: syscalls.h 740 2004-05-07 11:50:40Z slamb $
 * @author      Scott Lamb &lt;slamb@slamb.org&gt;
 */

SYSCALL(accept, 3)
SYSCALL(connect, 3)
SYSCALL(nanosleep, 3)
SYSCALL(open, 3)
SYSCALL(pause, 0)
SYSCALL(poll, 3)
SYSCALL(read, 3)
SYSCALL(readv, 3)
SYSCALL(recv, 4)
SYSCALL(recvfrom, 6)
SYSCALL(recvmsg, 3)
SYSCALL(select, 5)
SYSCALL(send, 4)
SYSCALL(sendto, 6)
SYSCALL(sendmsg, 3)
SYSCALL(sigsuspend, 1)
SYSCALL(write, 3)
SYSCALL(writev, 3)
SYSCALL(wait4, 4)
