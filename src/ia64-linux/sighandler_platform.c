/** @file
 * Adjusts instruction pointer as necessary on ia64-linux.
 * @legal
 * Copyright &copy; 2004 Scott Lamb &lt;slamb@slamb.org&gt;.
 * This file is part of sigsafe, which is released under the MIT license.
 * @version     $Id: sighandler_platform.c 740 2004-05-07 11:50:40Z slamb $
 * @author      Scott Lamb &lt;slamb@slamb.org&gt;
 */

#define ORG_SLAMB_SIGSAFE_INTERNAL
#include <sigsafe.h>
#include <ucontext.h>
#include <unistd.h>

void sighandler_for_platform(ucontext_t *ctx) {
    struct sigsafe_syscall *s;
    void *ip;
    ip = (void*) ctx->uc_mcontext.sc_ip;
    for (s = sigsafe_syscalls; s->address != NULL; s++) {
        if (s->minjmp <= ip && ip <= s->maxjmp) {
#ifdef ORG_SLAMB_SIGSAFE_DEBUG_JUMP
            write(2, "[J]", 3);
#endif
            ctx->uc_mcontext.sc_ip = (unsigned long) s->jmpto;
            return;
        }
    }
}
