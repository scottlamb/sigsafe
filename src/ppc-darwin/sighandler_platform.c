/* $Id$ */

#define ORG_SLAMB_SIGSAFE_INTERNAL
#include <sigsafe.h>
#include <ucontext.h>
#include <signal.h>

void sighandler_for_platform(ucontext_t *ctx) {
    struct sigsafe_syscall *s;
    void *srr0;
    srr0 = (void*) ctx->uc_mcontext->ss.srr0;
    for (s = sigsafe_syscalls; s->address != NULL; s++) {
        if (s->minjmp <= srr0 && srr0 <= s->maxjmp) {
            ctx->uc_mcontext->ss.srr0 = (int) s->jmpto;
            sigreturn((struct sigcontext*) ctx);
        }
    }
}
