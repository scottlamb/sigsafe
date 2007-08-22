/** @file
 * Process tracing under Solaris.
 * This doesn't work yet.
 * @legal
 * Copyright &copy; 2004 Scott Lamb &lt;slamb@slamb.org&gt;.
 * This file is part of sigsafe, which is released under the MIT license.
 * @version     $Id$
 * @author      Scott Lamb &lt;slamb@slamb.org&gt;
 */

#include <sys/types.h>
#include <unistd.h>
#include "race_checker.h"

void
trace_me(void)
{
    error_wrap(ptrace(0, 0, 0, 0),
               "ptrace(PT_TRACE_ME, ...)", ERRNO);
}

void
trace_attach(pid_t pid)
{
}

void
trace_step(pid_t pid, int signum)
{
    error_wrap(ptrace(9, pid, 1, signum),
               "ptrace(PT_STEP, ...)", ERRNO);
}

void
trace_continue(pid_t pid, int signum)
{
    error_wrap(ptrace(7, pid, 1, signum),
               "ptrace(PT_CONT, ...)", ERRNO);
}
