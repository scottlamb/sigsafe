/** @file
 * Adjusts instruction pointer as necessary on i386-linux.
 * @legal
 * Copyright &copy; 2004 Scott Lamb &lt;slamb@slamb.org&gt;.
 * This file is part of sigsafe, which is released under the MIT license.
 * @version     $Id$
 * @author      Scott Lamb &lt;slamb@slamb.org&gt;
 */

#define _GNU_SOURCE /* for REG_RIP */
#define ORG_SLAMB_SIGSAFE_INTERNAL
#include <sigsafe.h>
#include <ucontext.h>
#include <unistd.h>

void sighandler_for_platform(ucontext_t *ctx) {
    struct sigsafe_syscall *s;
    void *rip;
    rip = (void*) ctx->uc_mcontext.gregs[REG_RIP];
    for (s = sigsafe_syscalls; s->address != NULL; s++) {
        if (s->minjmp <= rip && rip <= s->maxjmp) {
#ifdef ORG_SLAMB_SIGSAFE_DEBUG_JUMP
            write(2, "[J]", 3);
#endif
            ctx->uc_mcontext.gregs[REG_RIP] = (long) s->jmpto;
            return;
        }
    }
}
