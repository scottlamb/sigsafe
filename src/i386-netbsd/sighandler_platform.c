/** @file
 * Adjusts instruction pointer as necessary on i386-linux.
 * @legal
 * Copyright &copy; 2004 Scott Lamb &lt;slamb@slamb.org&gt;.
 * This file is part of sigsafe, which is released under the MIT license.
 * @version     $Id$
 * @author      Scott Lamb &lt;slamb@slamb.org&gt;
 */

#define ORG_SLAMB_SIGSAFE_INTERNAL
#include <sigsafe.h>
#include <signal.h>
#include <unistd.h>

void sighandler_for_platform(struct sigcontext_t *ctx) {
    struct sigsafe_syscall *s;
    void *eip;
    eip = (void*) ctx->sc_eip;
    for (s = sigsafe_syscalls; s->address != NULL; s++) {
        if (s->minjmp <= eip && eip <= s->maxjmp) {
#ifdef ORG_SLAMB_SIGSAFE_DEBUG_JUMP
            write(2, "[J]", 3);
#endif
            ctx->sc_eip = (long) s->jmpto;
            return;
        }
    }
}
