/* $Id$ */

#define ORG_SLAMB_SIGSAFE_INTERNAL
#include <sigsafe.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

void sighandler_for_platform(ucontext_t *ctx) {
    struct sigsafe_syscall *s;
    void *srr0;
    struct sigsafe_tsd *tsd;
    sigset_t *uc_sigmask;

    srr0 = (void*) ctx->uc_mcontext->ss.srr0;

    for (s = sigsafe_syscalls; s->address != NULL; s++) {
        if (s->minjmp <= srr0 && srr0 <= s->maxjmp) {

            /* We are in a jump region. */

            /*
             * Set uc_sigmask to point to correct place.
             * See BUILD_FRAME.
             */
            uc_sigmask = (sigset_t*)((char*) ctx->uc_mcontext->ss.r1 + 56);
            tsd = (struct sigsafe_tsd*) ctx->uc_mcontext->ss.r7;

            memcpy(uc_sigmask, &ctx->uc_sigmask, sizeof(sigset_t));
            longjmp(tsd->env, 1);
        }
    }
}
