/* $Id$ */

#define ORG_SLAMB_SIGSAFE_INTERNAL
#include <sigsafe.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

void sighandler_for_platform(ucontext_t *ctx) {
    struct sigsafe_syscall *s;
    int *old_cancellation_type;
    void *srr0;
    sigset_t *uc_sigmask;

    srr0 = (void*) ctx->uc_mcontext->ss.srr0;

    for (s = sigsafe_syscalls; s->address != NULL; s++) {
        if (    srr0 >= s->address + s->max_jump_off
            &&  srr0 <= s->address + s->max_jump_off) {

            /* We are in a jump region. */

            /*
             * XXX
             * Set old_cancellation_type and uc_sigmask to point to correct
             * place
             */
            abort();

            pthread_setcanceltype(*old_cancellation_type, NULL);
            memcpy(uc_sigmask, &ctx->uc_sigmask, sizeof(sigset_t));

            /*
             * XXX
             * restore important registers,
             * jump to s->address + s->jump_to_off
             */
            abort();
        }
    }
}
