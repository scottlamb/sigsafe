/** @file
 * Adjusts instruction pointer as necessary on ia64-linux.
 * @legal
 * Copyright &copy; 2004 Scott Lamb &lt;slamb@slamb.org&gt;.
 * This file is part of sigsafe, which is released under the MIT license.
 * @version     $Id: sighandler_platform.c 740 2004-05-07 11:50:40Z slamb $
 * @author      Scott Lamb &lt;slamb@slamb.org&gt;
 */

#include "sigsafe_internal.h"
#include <ucontext.h>
#include <unistd.h>

void sigsafe_handler_for_platform_(ucontext_t *ctx) {
    struct sigsafe_syscall_ *s;
    void *ip;
    ip = (void*) ctx->uc_mcontext.sc_ip;
    for (s = sigsafe_syscalls_; s->minjmp != NULL; s++) {
        /*
         * XXX
         *
         * There are two funny things about the next lines:
         * - the extra dereference; why? function pointers on ia64 are just
         *   like this?
         * - the "+ 1" in the maxjmp. It's clearly something to do with how
         *   the break.i instruction is bundled, but I don't completely get
         *   it.
         */
        void *minjmp = * (void**) s->minjmp;
        void *maxjmp = * (void**) s->maxjmp + 1;
        void *jmpto  = * (void**) s->jmpto;
        if (minjmp <= ip && ip <= maxjmp) {
#ifdef SIGSAFE_DEBUG_JUMP
            write(2, "[J]", 3);
#endif
            ctx->uc_mcontext.sc_ip = (unsigned long) jmpto;
            return;
        }
    }
}
