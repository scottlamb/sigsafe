/** @file
 * Process tracing under Darwin.
 * @bug
 * This code doesn't work! It's halfway between ptrace and Mach thread code
 * now, and neither works as expected. The ptrace stuff returns
 * <tt>EINVAL</tt> and the Mach thread code makes race_checker think the
 * process is getting <tt>SIGHUP</tt>ed? Weird.
 * @legal
 * Copyright &copy; 2004 Scott Lamb &lt;slamb@slamb.org&gt;.
 * This file is part of sigsafe, which is released under the MIT license.
 * @version     $Id$
 * @author      Scott Lamb &lt;slamb@slamb.org&gt;
 */

#include <mach/mach.h>
#include <mach/mach_error.h>
#include <sys/types.h>
#include <sys/ptrace.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include "race_checker.h"

/*
 * ptrace under Darwin doesn't seem to work, so I'm using the Mach thread
 * functions instead. They're described in the message
 * "Mach Exception Handlers 101 (Was Re: ptrace, gdb)"
 * <http://www.omnigroup.com/mailman/archive/macosx-dev/2000-June/002030.html>
 */

void mach_error_wrap(kern_return_t krc, const char *name) {
    if (krc != KERN_SUCCESS) {
        fprintf(stderr, "%s returned %d (%s)\n",
                name, krc, mach_error_string(krc));
        abort();
    }
}

/**
 * Returns a Mach thread for a process.
 * It is the caller's responsibility to free it.
 */
void thread_for_pid(pid_t pid, thread_t *thread) {
    task_t task;
    thread_t *thread_list;
    mach_msg_type_number_t thread_count;

    mach_error_wrap(task_for_pid(mach_task_self(), pid, &task),
                    "task_for_pid");
    mach_error_wrap(task_threads(task, &thread_list, &thread_count),
                    "task_threads");
    assert(thread_count == 1);
    memcpy(thread, &thread_list[0], sizeof(thread_t));
    mach_error_wrap(vm_deallocate(mach_task_self(), (vm_address_t) thread_list,
                                  thread_count * sizeof(thread_t)),
                    "vm_deallocate");
    mach_error_wrap(mach_port_deallocate(mach_task_self(), task),
                    "mach_port_deallocate");
}

void trace_attach(pid_t pid) {
    thread_t thread;
    thread_for_pid(pid, &thread);
    mach_error_wrap(thread_suspend(thread), "thread_suspend");
    error_wrap(kill(pid, SIGCONT), "kill", ERRNO);
    mach_error_wrap(mach_port_deallocate(mach_task_self(), thread),
                    "mach_port_deallocate");
    printf("attached\n");
}

void trace_step(pid_t pid, int signum) {
    thread_t thread;
    struct ppc_thread_state state;
    int state_count = PPC_THREAD_STATE_COUNT;

    thread_for_pid(pid, &thread);
    mach_error_wrap(thread_get_state(thread, PPC_THREAD_STATE,
                                     (natural_t*) &state,
                                     &state_count), "thread_get_state");
    state.srr1 |= 0x400UL; /* enable SE bit */
    mach_error_wrap(thread_set_state(thread, PPC_THREAD_STATE,
                                     (natural_t*)&state, state_count),
                    "thread_set_state");
    if (signum != 0) {
        error_wrap(kill(pid, signum), "kill", ERRNO);
    }
    mach_error_wrap(thread_resume(thread), "thread_resume");
    mach_error_wrap(mach_port_deallocate(mach_task_self(), thread),
                    "mach_port_deallocate");
    printf("stepped once\n");
}

/*
 * XXX
 * Once I do manage to get it to step once...what happens?
 * I probably need to do the Mach exception trapping stuff mentioned in that
 * link above. But I don't think a SIGCHLD as if it died from SIGHUP is quite
 * what's supposed to happen to make me do that. Does it send SIGCHLD for this
 * at all?
 *
 * Stupid Mach.
 */

void trace_detach(pid_t pid, int signum) {
    fprintf(stderr, "trace_detach: Wow, you got it to get this far?");
    abort();
    /*errno = 0;
    error_wrap(ptrace(PT_DETACH, pid, (caddr_t) 1, signum),
               "ptrace(PT_DETACH, ...)", ERRNO);*/
}
