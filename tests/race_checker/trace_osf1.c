/** @file
 * Process tracing under OSF/1.
 * @legal
 * Copyright &copy; 2004 Scott Lamb &lt;slamb@slamb.org&gt;.
 * This file is part of sigsafe, which is released under the MIT license.
 * @version     $Id$
 * @author      Scott Lamb &lt;slamb@slamb.org&gt;
 */

#include <sys/signal.h>
#include <sys/ptrace.h>
#include <stdlib.h>
#include "race_checker.h"

void trace_me(void) {
    error_wrap(ptrace(PT_TRACE_ME, 0, NULL, 0),
               "ptrace(PT_TRACE_ME, ...)", ERRNO);
}

void trace_attach(pid_t pid) {}

void trace_step(pid_t pid, int signum) {
    error_wrap(ptrace(PT_STEP, pid, NULL, signum),
               "ptrace(PT_STEP, ...)", ERRNO);
}

void trace_continue(pid_t pid, int signum) {
    error_wrap(ptrace(PT_CONTINUE, pid, NULL, signum),
               "ptrace(PT_CONTINUE, ...)", ERRNO);
}
