/** @file
 * Process tracing under Darwin.
 * @bug
 * This code doesn't work! Stepping, continuing, and detaching all yield
 * <tt>EINVAL</tt>. If you can figure out the problem, please let me know.
 * @legal
 * Copyright &copy; 2004 Scott Lamb &lt;slamb@slamb.org&gt;.
 * This file is part of sigsafe, which is released under the MIT license.
 * @version     $Id$
 * @author      Scott Lamb &lt;slamb@slamb.org&gt;
 */

#include <sys/types.h>
#include <sys/ptrace.h>
#include <stdlib.h>
#include <errno.h>
#include "race_checker.h"

void trace_attach(pid_t pid) {
    errno = 0;
    error_wrap(ptrace(PT_ATTACH, pid, NULL, NULL),
               "ptrace(PT_ATTACH, ...)", ERRNO);
}

void trace_step(pid_t pid, int signum) {
    errno = 0;
    error_wrap(ptrace(PT_STEP, pid, (caddr_t) 1, (void*) signum),
               "ptrace(PT_STEP, ...)", ERRNO);
}

void trace_detach(pid_t pid, int signum) {
    errno = 0;
    error_wrap(ptrace(PT_DETACH, pid, (caddr_t) 1, (void*) signum),
               "ptrace(PT_DETACH, ...)", ERRNO);
}
