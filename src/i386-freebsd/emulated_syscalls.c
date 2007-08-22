/** @file
 * Emulated system calls under i386-freebsd.
 * @legal
 * Copyright &copy; 2004 Scott Lamb &lt;slamb@slamb.org&gt;.
 * This file is part of sigsafe, which is released under the MIT license.
 * @version     $Id$
 * @author      Scott Lamb &lt;slamb@slamb.org&gt;
 */

#include "sigsafe_internal.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/resource.h>

ssize_t
sigsafe_recv(int s, void *buf, size_t len, int flags)
{
    return sigsafe_recvfrom(s, buf, len, flags, NULL, NULL);
}

ssize_t
sigsafe_send(int s, const void *buf, size_t len, int flags)
{
    return sendto(s, buf, len, flags, NULL, 0);
}

pid_t
sigsafe_wait(int *status)
{
    return sigsafe_wait4(WAIT_ANY, status, 0, NULL);
}

pid_t
sigsafe_wait3(int *status, int options, struct rusage *rusage)
{
    return sigsafe_wait4(WAIT_ANY, status, options, rusage);
}

int
sigsafe_pause(void)
{
    sigset_t set;

    sigemptyset(&set);
    return sigsafe_sigsuspend(&set);
}
