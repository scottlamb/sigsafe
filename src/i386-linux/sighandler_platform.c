/** @file
 * Adjusts instruction pointer as necessary on i386-linux.
 * @legal
 * Copyright &copy; 2004 Scott Lamb &lt;slamb@slamb.org&gt;.
 * This file is part of sigsafe, which is released under the MIT license.
 * @version     $Id$
 * @author      Scott Lamb &lt;slamb@slamb.org&gt;
 */

#define _GNU_SOURCE /* Necessary for REG_EIP. Why, I don't know. */
#include "sigsafe_internal.h"
#include <ucontext.h>
#include <unistd.h>

PRIVATE(void sigsafe_handler_for_platform_(ucontext_t *ctx)) {
    struct sigsafe_syscall_ *s;
    void *eip;
    eip = (void*) ctx->uc_mcontext.gregs[REG_EIP];
    for (s = sigsafe_syscalls_; s->minjmp != NULL; s++) {
        if (s->minjmp <= eip && eip <= s->maxjmp) {
#ifdef SIGSAFE_DEBUG_JUMP
            write(2, "[J]", 3);
#endif
            ctx->uc_mcontext.gregs[REG_EIP] = (int) s->jmpto;
            return;
        }
    }
}
