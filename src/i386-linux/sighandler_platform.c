/* $Id: sighandler_platform.c$ */

#define _GNU_SOURCE
#define ORG_SLAMB_SIGSAFE_INTERNAL
#include <sigsafe.h>
#include <ucontext.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

void sighandler_for_platform(ucontext_t *ctx) {
    struct sigsafe_syscall *s;
    void *eip;
    eip = (void*) ctx->uc_mcontext.gregs[REG_EIP];
    for (s = sigsafe_syscalls; s->address != NULL; s++) {
        if (s->minjmp <= eip && eip <= s->maxjmp) {
            ctx->uc_mcontext.gregs[REG_EIP] = (int) s->jmpto;
            return;
        }
    }
}
