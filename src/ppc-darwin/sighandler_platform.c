/** @file
 * Adjusts instruction pointer as necessary on ppc-darwin.
 * @legal
 * Copyright &copy; 2004 Scott Lamb &lt;slamb@slamb.org&gt;.
 * This file is part of sigsafe, which is released under the MIT license.
 * @version     $Id$
 * @author      Scott Lamb &lt;slamb@slamb.org&gt;
 */

#include "sigsafe_internal.h"
#include <ucontext.h>
#include <unistd.h>

PRIVATE_DEF(void sigsafe_handler_for_platform_(ucontext_t *ctx)) {
    struct sigsafe_syscall_ *s;
    void *srr0;
    srr0 = (void*) ctx->uc_mcontext->ss.srr0;
    for (s = sigsafe_syscalls_; s->minjmp != NULL; s++) {
        if (s->minjmp <= srr0 && srr0 <= s->maxjmp) {
#ifdef SIGSAFE_DEBUG_JUMP
            write(2, "[J]", 3);
#endif
            ctx->uc_mcontext->ss.srr0 = (long) s->jmpto;
            return;
        }
    }
}
